---- MODULE SolverSession ----------------------------------------------------
(***************************************************************************)
(* Formal protocol model of solverSession_c                                *)
(* (src/lib/solver/solversession.{h,cpp}; design/2026-11-20-core-api.md):  *)
(*                                                                         *)
(*   - Lifecycle state machine:                                            *)
(*       Idle -> start() -> Running                                        *)
(*       Running -> end of phase -> Paused | Finished | Stopped | Error    *)
(*       Paused -> state() (read-only snapshot) / resume() -> Running      *)
(*       Idle | Running | Paused -> abort() -> Stopped                     *)
(*   - The end-of-phase decision in two steps, as in the code: runPhase()  *)
(*     reads abortRequested_ outside queueMutex_ (DDecide), finishPhase()  *)
(*     publishes under it (DPublish) and turns Paused/Finished into        *)
(*     Stopped when an abort() came in between (SPEC-SESS-6).              *)
(*   - abort() from a thread of its own (aborter), racing everything:      *)
(*     start()'s resume-data validation (start() re-checks Idle before     *)
(*     going Running), the end of a phase, pause and resume.               *)
(*   - Bounded pull queue (resQ, soft capacity Cap); the cap is lifted on  *)
(*     pause (requestPause()) and on a failing phase, so in-flight         *)
(*     results never wedge against a full queue, even when the host        *)
(*     stopped calling pop(). The host may keep popping after              *)
(*     requestPause().                                                     *)
(*   - Pending (salvaged) assemblies carried across pause/resume and       *)
(*     taken before unsearched work.                                       *)
(*   - Error: a phase may fail (an assertion or any other exception in the *)
(*     driver, or a pool worker's exception rethrown by finish()); the     *)
(*     driver lifts the cap, tears the producers down and publishes Error. *)
(*                                                                         *)
(* PRODUCERS (what is abstracted, and why): a producer is "a thread that   *)
(* pushes results into the queue", with one step that takes an item and    *)
(* one that delivers it. In the code these are the assembler's threads     *)
(* (assembly only), or the disassembler pool: assembler threads submit to  *)
(* the pool, pool workers disassemble under a ThreadBudget token, and the  *)
(* worker itself delivers, in no particular order. The pool's internal     *)
(* queue and numbering are DisasmPool.tla's and Pipeline.tla's subject;    *)
(* here they collapse into "taken" (busy) items:                          *)
(* a pause either salvages a taken item into pending (the pool's           *)
(* never-started queue, requestStop()) or lets it complete into the queue  *)
(* past the cap (in flight). The budget token is held while producing and *)
(* returned BEFORE delivery: no thread enqueues while holding a token      *)
(* (assembly only has no budget, the inline pool none either, and a pool   *)
(* worker returns its token first), so                                     *)
(* enqueueResult() waits token-free and the old token-yield branch is gone *)
(* from both the code and this model. Re-submitting pending items, which   *)
(* the code's driver does before the assembly phase, is done by the        *)
(* producers here (pending first): same salvage.                           *)
(*                                                                         *)
(* NOT MODELED (deliberate gaps, see README.md):                           *)
(*   - Assembler-internal splits and the pool's internals                  *)
(*     (AssemblyPool.tla, DisasmPool.tla, Pipeline.tla).                   *)
(*   - Resume-data validation failures (synchronous, before any worker     *)
(*     starts: Idle -> Error, nothing to interleave with but abort(),      *)
(*     whose race with start() is modeled through HValidate/HCommit).      *)
(*   - countOnly mode (skips queue pushes; trivial projection).            *)
(*   - stopAfterPreparation (a phase that ends Paused before producing).   *)
(*                                                                         *)
(* INVARIANT IDs (shared with code comments, stable across refactors):     *)
(*   SPEC-SESS-1 ..... lifecycle state machine & LIFECYCLE-1/2 invariants  *)
(*   SPEC-SESS-2 ..... pull queue obeys soft cap while running and bounded *)
(*                     O(P) overshoot per pause (or failure) once lifted   *)
(*   SPEC-SESS-3 ..... item conservation: every item 1..K is unsearched,   *)
(*                     pending, in-flight, queued, popped, or discarded    *)
(*   SPEC-SESS-4 ..... no duplicate delivery; Finished + drained => all K  *)
(*   SPEC-SESS-5 ..... ThreadBudget conservation & clean shutdown          *)
(*   SPEC-SESS-6 ..... abort() is total: once abort() has run, the session *)
(*                     is never Paused, and abort() returns only with the  *)
(*                     session Stopped (or Error, or already Finished)     *)
(***************************************************************************)
EXTENDS Naturals, Integers, Sequences, FiniteSets, TLC

CONSTANTS
  P,            (* producer worker count (assembler/pool workers)          *)
  BudgetTotal,  (* shared ThreadBudget tokens                              *)
  K,            (* total search items 1..K                                 *)
  Cap,          (* soft pull-queue capacity (options_.queueCapacity)       *)
  MaxPauses     (* maximum pause/resume cycles explored                    *)

ASSUME P \in Nat /\ P > 0
ASSUME BudgetTotal \in Nat /\ BudgetTotal > 0
ASSUME K \in Nat /\ K > 0
ASSUME Cap \in Nat /\ Cap > 0
ASSUME MaxPauses \in Nat

Producers == 1..P
NoItem == 0
Terminal == {"Finished", "Stopped", "Error"}

(* --algorithm SolverSession {

  variables
    sessState = "Idle",                  (* Idle|Running|Paused|Finished|Stopped|Error *)
    phaseActive = FALSE,                 (* TRUE while driver's workers are live *)
    unsearched = [i \in 1..K |-> i],     (* remaining search territory           *)
    pending = <<>>,                      (* salvaged items for next resume()     *)
    resQ = <<>>,                         (* bounded pull queue (soft cap Cap)    *)
    popped = <<>>,                       (* items pulled by host via pop()       *)
    discarded = 0,                       (* items dropped by abort() or Error    *)
    busy = [w \in Producers |-> NoItem], (* item currently held by producer w    *)
    holds = [w \in Producers |-> FALSE], (* ThreadBudget token held by w         *)
    wdone = [w \in Producers |-> FALSE], (* producer w finished current phase    *)
    available = BudgetTotal,             (* free ThreadBudget tokens             *)
    pauseReq = FALSE,                    (* pauseRequested_ (also set on failure) *)
    abortReq = FALSE,                    (* abortRequested_                      *)
    failReq = FALSE,                     (* the phase failed (exception caught)  *)
    pausesDone = 0,
    hostDone = FALSE,
    aborted = FALSE;                     (* ghost: abort() has run (never reset) *)

  \* Producers: run during each Running phase, acquire a budget token, take
  \* pending items FIRST then unsearched items, return the token, then push
  \* into resQ obeying the soft cap (lifted on pauseReq).
  fair process (producer \in 1..P)
  variables it = NoItem;
  {
  PLoop:
    while (sessState \notin Terminal) {
    PPhaseWait:
      await (phaseActive /\ ~wdone[self]) \/ sessState \in Terminal;
      if (sessState \in Terminal) {
        skip;
      } else {
      PWorkLoop:
        while (~wdone[self]) {
        PTake:
          \* Acquire a budget token before taking work, or exit on pause/abort
          \* or when no work remains.
          await available > 0 \/ pauseReq \/ abortReq
             \/ (Len(pending) = 0 /\ Len(unsearched) = 0);
          if (pauseReq \/ abortReq \/ (Len(pending) = 0 /\ Len(unsearched) = 0)) {
            wdone[self] := TRUE;
          } else {
            available := available - 1;
            holds[self] := TRUE;
            if (Len(pending) > 0) {
              it := Head(pending);
              pending := Tail(pending);
            } else {
              it := Head(unsearched);
              unsearched := Tail(unsearched);
            };
            busy[self] := it;
          PProduce:
            \* The job is done: the token goes back before delivery. Before
            \* that, an abort discards the item, a pause either salvages the
            \* never-started item back to pending (requestStop()) or lets the
            \* in-flight one complete.
            available := available + 1;
            holds[self] := FALSE;
            if (abortReq) {
              discarded := discarded + 1;
              busy[self] := NoItem;
              it := NoItem;
              wdone[self] := TRUE;
            } else if (pauseReq) {
              either {
                pending := Append(pending, it);
                busy[self] := NoItem;
                it := NoItem;
                wdone[self] := TRUE;
              } or {
                skip; \* in flight: delivered below, past the lifted cap
              };
            };
          PEnqLoop:
            \* solverSession_c::enqueueResult(): token-free; holds
            \* queueMutex_ across the cap check and push, drops it only
            \* inside the cvNotFull_ wait.
            while (busy[self] /= NoItem) {
              if (abortReq) {
                discarded := discarded + 1;
                busy[self] := NoItem;
                it := NoItem;
                wdone[self] := TRUE;
              } else if (Len(resQ) < Cap \/ pauseReq) {
                resQ := Append(resQ, it);
                busy[self] := NoItem;
                it := NoItem;
              } else {
              PEnqWait:
                await Len(resQ) < Cap \/ pauseReq \/ abortReq;
              };
            };
          };
        };
      };
    };
  }

  \* Session driver thread (solverSession_c::runPhase / finishPhase): may
  \* fail during a phase (catch block: lift the cap, tear down), joins the
  \* producers, decides the end state outside queueMutex_ (DDecide) and
  \* publishes it under queueMutex_ (DPublish).
  fair process (driver = 0)
  variables next = "none";
  {
  DLoop:
    while (sessState \notin Terminal) {
    DMaybeFail:
      either {
        skip;
      } or {
        \* teardownAfterFailure(): lift the cap so in-flight results drain,
        \* then the pool's abort() joins its threads.
        await phaseActive /\ ~failReq;
        failReq := TRUE;
        pauseReq := TRUE;
      };
    DWait:
      await (phaseActive /\ (\A w \in Producers : wdone[w])) \/ sessState \in Terminal;
      if (sessState \in Terminal) {
        skip;
      } else {
        phaseActive := FALSE;
        \* runPhase() step 6: abortRequested_ read without queueMutex_
        if (failReq) {
          next := "Error";
        } else if (abortReq) {
          next := "Stopped";
        } else if (Len(unsearched) = 0 /\ Len(pending) = 0) {
          next := "Finished";
        } else {
          next := "Paused";
        };
      DPublish:
        \* finishPhase(), under queueMutex_: an abort() since DDecide wins
        \* (SPEC-SESS-6). Error stays Error.
        with (fin = IF abortReq /\ next \in {"Paused", "Finished"} THEN "Stopped" ELSE next) {
          if (fin = "Stopped") {
            discarded := discarded + Len(pending) + Len(unsearched) + Len(resQ);
            pending := <<>>;
            unsearched := <<>>;
            resQ := <<>>;
          } else if (fin = "Error") {
            discarded := discarded + Len(pending) + Len(unsearched);
            pending := <<>>;
            unsearched := <<>>;
          };
          sessState := fin;
        };
      };
    };
  }

  \* Host thread: start() (validation, then the Idle re-check), pop(),
  \* requestPause() (and keeps popping, or waits as pause() does),
  \* state()/resume(). Deliberately allows pausing WITHOUT popping when
  \* resQ is full, proving pause() never deadlocks on a full queue.
  fair process (host = -1)
  {
  HValidate:
    \* start(): resume-data validation runs before any worker exists, with
    \* the session still Idle (an abort() may land here)
    skip;
  HCommit:
    \* start() re-checks Idle under queueMutex_: it never revives an
    \* aborted session
    if (sessState = "Idle") {
      pauseReq := FALSE;
      abortReq := FALSE;
      sessState := "Running";
      phaseActive := TRUE;
    };
  HLoop:
    while (~hostDone) {
    HStep:
      if (sessState \in Terminal /\ Len(resQ) = 0) {
        hostDone := TRUE;
      } else {
        either {
          \* pop(): an item at once in any state
          await Len(resQ) > 0;
          popped := Append(popped, Head(resQ));
          resQ := Tail(resQ);
        } or {
          \* state() is legal only in Paused, then resume() -> Running
          await sessState = "Paused";
          pauseReq := FALSE;
          abortReq := FALSE;
          wdone := [w \in Producers |-> FALSE];
          sessState := "Running";
          phaseActive := TRUE;
        } or {
          await sessState = "Running" /\ ~pauseReq /\ pausesDone < MaxPauses;
          pauseReq := TRUE;
          pausesDone := pausesDone + 1;
          either {
            \* requestPause(): the host goes on popping
            skip;
          } or {
            \* pause(): blocks in join() WITHOUT popping, even on a full
            \* queue (only the lifted cap lets the producers finish)
          HJoin:
            await sessState /= "Running";
          };
        };
      };
    };
  }

  \* abort() from any thread at any time (or never: the skip branch).
  \* Fair, so a started abort() completes its join().
  fair process (aborter = -2)
  variables saw = "none";
  {
  ABegin:
    either {
      skip;
    } or {
      \* under queueMutex_: flags, clear the queue; Idle and Paused have no
      \* driver deciding their end, so abort() finalises them itself
      saw := sessState;
      aborted := TRUE;
      abortReq := TRUE;
      pauseReq := TRUE;
      if (sessState \in {"Idle", "Paused"}) {
        discarded := discarded + Len(resQ) + Len(pending) + Len(unsearched);
        resQ := <<>>;
        pending := <<>>;
        unsearched := <<>>;
        sessState := "Stopped";
      } else {
        discarded := discarded + Len(resQ);
        resQ := <<>>;
      };
    AJoin:
      \* join(): returns once the driver has published the phase's end
      await sessState /= "Running";
      assert sessState \in {"Stopped", "Error"} \/ saw \in {"Finished", "Error"};
    };
  }

} *)
\* BEGIN TRANSLATION
VARIABLES sessState, phaseActive, unsearched, pending, resQ, popped, 
          discarded, busy, holds, wdone, available, pauseReq, abortReq, 
          failReq, pausesDone, hostDone, aborted, pc, it, next, saw

vars == << sessState, phaseActive, unsearched, pending, resQ, popped, 
           discarded, busy, holds, wdone, available, pauseReq, abortReq, 
           failReq, pausesDone, hostDone, aborted, pc, it, next, saw >>

ProcSet == (1..P) \cup {0} \cup {-1} \cup {-2}

Init == (* Global variables *)
        /\ sessState = "Idle"
        /\ phaseActive = FALSE
        /\ unsearched = [i \in 1..K |-> i]
        /\ pending = <<>>
        /\ resQ = <<>>
        /\ popped = <<>>
        /\ discarded = 0
        /\ busy = [w \in Producers |-> NoItem]
        /\ holds = [w \in Producers |-> FALSE]
        /\ wdone = [w \in Producers |-> FALSE]
        /\ available = BudgetTotal
        /\ pauseReq = FALSE
        /\ abortReq = FALSE
        /\ failReq = FALSE
        /\ pausesDone = 0
        /\ hostDone = FALSE
        /\ aborted = FALSE
        (* Process producer *)
        /\ it = [self \in 1..P |-> NoItem]
        (* Process driver *)
        /\ next = "none"
        (* Process aborter *)
        /\ saw = "none"
        /\ pc = [self \in ProcSet |-> CASE self \in 1..P -> "PLoop"
                                        [] self = 0 -> "DLoop"
                                        [] self = -1 -> "HValidate"
                                        [] self = -2 -> "ABegin"]

PLoop(self) == /\ pc[self] = "PLoop"
               /\ IF sessState \notin Terminal
                     THEN /\ pc' = [pc EXCEPT ![self] = "PPhaseWait"]
                     ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
               /\ UNCHANGED << sessState, phaseActive, unsearched, pending, 
                               resQ, popped, discarded, busy, holds, wdone, 
                               available, pauseReq, abortReq, failReq, 
                               pausesDone, hostDone, aborted, it, next, saw >>

PPhaseWait(self) == /\ pc[self] = "PPhaseWait"
                    /\ (phaseActive /\ ~wdone[self]) \/ sessState \in Terminal
                    /\ IF sessState \in Terminal
                          THEN /\ TRUE
                               /\ pc' = [pc EXCEPT ![self] = "PLoop"]
                          ELSE /\ pc' = [pc EXCEPT ![self] = "PWorkLoop"]
                    /\ UNCHANGED << sessState, phaseActive, unsearched, 
                                    pending, resQ, popped, discarded, busy, 
                                    holds, wdone, available, pauseReq, 
                                    abortReq, failReq, pausesDone, hostDone, 
                                    aborted, it, next, saw >>

PWorkLoop(self) == /\ pc[self] = "PWorkLoop"
                   /\ IF ~wdone[self]
                         THEN /\ pc' = [pc EXCEPT ![self] = "PTake"]
                         ELSE /\ pc' = [pc EXCEPT ![self] = "PLoop"]
                   /\ UNCHANGED << sessState, phaseActive, unsearched, pending, 
                                   resQ, popped, discarded, busy, holds, wdone, 
                                   available, pauseReq, abortReq, failReq, 
                                   pausesDone, hostDone, aborted, it, next, 
                                   saw >>

PTake(self) == /\ pc[self] = "PTake"
               /\    available > 0 \/ pauseReq \/ abortReq
                  \/ (Len(pending) = 0 /\ Len(unsearched) = 0)
               /\ IF pauseReq \/ abortReq \/ (Len(pending) = 0 /\ Len(unsearched) = 0)
                     THEN /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                          /\ pc' = [pc EXCEPT ![self] = "PWorkLoop"]
                          /\ UNCHANGED << unsearched, pending, busy, holds, 
                                          available, it >>
                     ELSE /\ available' = available - 1
                          /\ holds' = [holds EXCEPT ![self] = TRUE]
                          /\ IF Len(pending) > 0
                                THEN /\ it' = [it EXCEPT ![self] = Head(pending)]
                                     /\ pending' = Tail(pending)
                                     /\ UNCHANGED unsearched
                                ELSE /\ it' = [it EXCEPT ![self] = Head(unsearched)]
                                     /\ unsearched' = Tail(unsearched)
                                     /\ UNCHANGED pending
                          /\ busy' = [busy EXCEPT ![self] = it'[self]]
                          /\ pc' = [pc EXCEPT ![self] = "PProduce"]
                          /\ wdone' = wdone
               /\ UNCHANGED << sessState, phaseActive, resQ, popped, discarded, 
                               pauseReq, abortReq, failReq, pausesDone, 
                               hostDone, aborted, next, saw >>

PProduce(self) == /\ pc[self] = "PProduce"
                  /\ available' = available + 1
                  /\ holds' = [holds EXCEPT ![self] = FALSE]
                  /\ IF abortReq
                        THEN /\ discarded' = discarded + 1
                             /\ busy' = [busy EXCEPT ![self] = NoItem]
                             /\ it' = [it EXCEPT ![self] = NoItem]
                             /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                             /\ UNCHANGED pending
                        ELSE /\ IF pauseReq
                                   THEN /\ \/ /\ pending' = Append(pending, it[self])
                                              /\ busy' = [busy EXCEPT ![self] = NoItem]
                                              /\ it' = [it EXCEPT ![self] = NoItem]
                                              /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                                           \/ /\ TRUE
                                              /\ UNCHANGED <<pending, busy, wdone, it>>
                                   ELSE /\ TRUE
                                        /\ UNCHANGED << pending, busy, wdone, 
                                                        it >>
                             /\ UNCHANGED discarded
                  /\ pc' = [pc EXCEPT ![self] = "PEnqLoop"]
                  /\ UNCHANGED << sessState, phaseActive, unsearched, resQ, 
                                  popped, pauseReq, abortReq, failReq, 
                                  pausesDone, hostDone, aborted, next, saw >>

PEnqLoop(self) == /\ pc[self] = "PEnqLoop"
                  /\ IF busy[self] /= NoItem
                        THEN /\ IF abortReq
                                   THEN /\ discarded' = discarded + 1
                                        /\ busy' = [busy EXCEPT ![self] = NoItem]
                                        /\ it' = [it EXCEPT ![self] = NoItem]
                                        /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                                        /\ pc' = [pc EXCEPT ![self] = "PEnqLoop"]
                                        /\ resQ' = resQ
                                   ELSE /\ IF Len(resQ) < Cap \/ pauseReq
                                              THEN /\ resQ' = Append(resQ, it[self])
                                                   /\ busy' = [busy EXCEPT ![self] = NoItem]
                                                   /\ it' = [it EXCEPT ![self] = NoItem]
                                                   /\ pc' = [pc EXCEPT ![self] = "PEnqLoop"]
                                              ELSE /\ pc' = [pc EXCEPT ![self] = "PEnqWait"]
                                                   /\ UNCHANGED << resQ, busy, 
                                                                   it >>
                                        /\ UNCHANGED << discarded, wdone >>
                        ELSE /\ pc' = [pc EXCEPT ![self] = "PWorkLoop"]
                             /\ UNCHANGED << resQ, discarded, busy, wdone, it >>
                  /\ UNCHANGED << sessState, phaseActive, unsearched, pending, 
                                  popped, holds, available, pauseReq, abortReq, 
                                  failReq, pausesDone, hostDone, aborted, next, 
                                  saw >>

PEnqWait(self) == /\ pc[self] = "PEnqWait"
                  /\ Len(resQ) < Cap \/ pauseReq \/ abortReq
                  /\ pc' = [pc EXCEPT ![self] = "PEnqLoop"]
                  /\ UNCHANGED << sessState, phaseActive, unsearched, pending, 
                                  resQ, popped, discarded, busy, holds, wdone, 
                                  available, pauseReq, abortReq, failReq, 
                                  pausesDone, hostDone, aborted, it, next, saw >>

producer(self) == PLoop(self) \/ PPhaseWait(self) \/ PWorkLoop(self)
                     \/ PTake(self) \/ PProduce(self) \/ PEnqLoop(self)
                     \/ PEnqWait(self)

DLoop == /\ pc[0] = "DLoop"
         /\ IF sessState \notin Terminal
               THEN /\ pc' = [pc EXCEPT ![0] = "DMaybeFail"]
               ELSE /\ pc' = [pc EXCEPT ![0] = "Done"]
         /\ UNCHANGED << sessState, phaseActive, unsearched, pending, resQ, 
                         popped, discarded, busy, holds, wdone, available, 
                         pauseReq, abortReq, failReq, pausesDone, hostDone, 
                         aborted, it, next, saw >>

DMaybeFail == /\ pc[0] = "DMaybeFail"
              /\ \/ /\ TRUE
                    /\ UNCHANGED <<pauseReq, failReq>>
                 \/ /\ phaseActive /\ ~failReq
                    /\ failReq' = TRUE
                    /\ pauseReq' = TRUE
              /\ pc' = [pc EXCEPT ![0] = "DWait"]
              /\ UNCHANGED << sessState, phaseActive, unsearched, pending, 
                              resQ, popped, discarded, busy, holds, wdone, 
                              available, abortReq, pausesDone, hostDone, 
                              aborted, it, next, saw >>

DWait == /\ pc[0] = "DWait"
         /\ (phaseActive /\ (\A w \in Producers : wdone[w])) \/ sessState \in Terminal
         /\ IF sessState \in Terminal
               THEN /\ TRUE
                    /\ pc' = [pc EXCEPT ![0] = "DLoop"]
                    /\ UNCHANGED << phaseActive, next >>
               ELSE /\ phaseActive' = FALSE
                    /\ IF failReq
                          THEN /\ next' = "Error"
                          ELSE /\ IF abortReq
                                     THEN /\ next' = "Stopped"
                                     ELSE /\ IF Len(unsearched) = 0 /\ Len(pending) = 0
                                                THEN /\ next' = "Finished"
                                                ELSE /\ next' = "Paused"
                    /\ pc' = [pc EXCEPT ![0] = "DPublish"]
         /\ UNCHANGED << sessState, unsearched, pending, resQ, popped, 
                         discarded, busy, holds, wdone, available, pauseReq, 
                         abortReq, failReq, pausesDone, hostDone, aborted, it, 
                         saw >>

DPublish == /\ pc[0] = "DPublish"
            /\ LET fin == IF abortReq /\ next \in {"Paused", "Finished"} THEN "Stopped" ELSE next IN
                 /\ IF fin = "Stopped"
                       THEN /\ discarded' = discarded + Len(pending) + Len(unsearched) + Len(resQ)
                            /\ pending' = <<>>
                            /\ unsearched' = <<>>
                            /\ resQ' = <<>>
                       ELSE /\ IF fin = "Error"
                                  THEN /\ discarded' = discarded + Len(pending) + Len(unsearched)
                                       /\ pending' = <<>>
                                       /\ unsearched' = <<>>
                                  ELSE /\ TRUE
                                       /\ UNCHANGED << unsearched, pending, 
                                                       discarded >>
                            /\ resQ' = resQ
                 /\ sessState' = fin
            /\ pc' = [pc EXCEPT ![0] = "DLoop"]
            /\ UNCHANGED << phaseActive, popped, busy, holds, wdone, available, 
                            pauseReq, abortReq, failReq, pausesDone, hostDone, 
                            aborted, it, next, saw >>

driver == DLoop \/ DMaybeFail \/ DWait \/ DPublish

HValidate == /\ pc[-1] = "HValidate"
             /\ TRUE
             /\ pc' = [pc EXCEPT ![-1] = "HCommit"]
             /\ UNCHANGED << sessState, phaseActive, unsearched, pending, resQ, 
                             popped, discarded, busy, holds, wdone, available, 
                             pauseReq, abortReq, failReq, pausesDone, hostDone, 
                             aborted, it, next, saw >>

HCommit == /\ pc[-1] = "HCommit"
           /\ IF sessState = "Idle"
                 THEN /\ pauseReq' = FALSE
                      /\ abortReq' = FALSE
                      /\ sessState' = "Running"
                      /\ phaseActive' = TRUE
                 ELSE /\ TRUE
                      /\ UNCHANGED << sessState, phaseActive, pauseReq, 
                                      abortReq >>
           /\ pc' = [pc EXCEPT ![-1] = "HLoop"]
           /\ UNCHANGED << unsearched, pending, resQ, popped, discarded, busy, 
                           holds, wdone, available, failReq, pausesDone, 
                           hostDone, aborted, it, next, saw >>

HLoop == /\ pc[-1] = "HLoop"
         /\ IF ~hostDone
               THEN /\ pc' = [pc EXCEPT ![-1] = "HStep"]
               ELSE /\ pc' = [pc EXCEPT ![-1] = "Done"]
         /\ UNCHANGED << sessState, phaseActive, unsearched, pending, resQ, 
                         popped, discarded, busy, holds, wdone, available, 
                         pauseReq, abortReq, failReq, pausesDone, hostDone, 
                         aborted, it, next, saw >>

HStep == /\ pc[-1] = "HStep"
         /\ IF sessState \in Terminal /\ Len(resQ) = 0
               THEN /\ hostDone' = TRUE
                    /\ pc' = [pc EXCEPT ![-1] = "HLoop"]
                    /\ UNCHANGED << sessState, phaseActive, resQ, popped, 
                                    wdone, pauseReq, abortReq, pausesDone >>
               ELSE /\ \/ /\ Len(resQ) > 0
                          /\ popped' = Append(popped, Head(resQ))
                          /\ resQ' = Tail(resQ)
                          /\ pc' = [pc EXCEPT ![-1] = "HLoop"]
                          /\ UNCHANGED <<sessState, phaseActive, wdone, pauseReq, abortReq, pausesDone>>
                       \/ /\ sessState = "Paused"
                          /\ pauseReq' = FALSE
                          /\ abortReq' = FALSE
                          /\ wdone' = [w \in Producers |-> FALSE]
                          /\ sessState' = "Running"
                          /\ phaseActive' = TRUE
                          /\ pc' = [pc EXCEPT ![-1] = "HLoop"]
                          /\ UNCHANGED <<resQ, popped, pausesDone>>
                       \/ /\ sessState = "Running" /\ ~pauseReq /\ pausesDone < MaxPauses
                          /\ pauseReq' = TRUE
                          /\ pausesDone' = pausesDone + 1
                          /\ \/ /\ TRUE
                                /\ pc' = [pc EXCEPT ![-1] = "HLoop"]
                             \/ /\ pc' = [pc EXCEPT ![-1] = "HJoin"]
                          /\ UNCHANGED <<sessState, phaseActive, resQ, popped, wdone, abortReq>>
                    /\ UNCHANGED hostDone
         /\ UNCHANGED << unsearched, pending, discarded, busy, holds, 
                         available, failReq, aborted, it, next, saw >>

HJoin == /\ pc[-1] = "HJoin"
         /\ sessState /= "Running"
         /\ pc' = [pc EXCEPT ![-1] = "HLoop"]
         /\ UNCHANGED << sessState, phaseActive, unsearched, pending, resQ, 
                         popped, discarded, busy, holds, wdone, available, 
                         pauseReq, abortReq, failReq, pausesDone, hostDone, 
                         aborted, it, next, saw >>

host == HValidate \/ HCommit \/ HLoop \/ HStep \/ HJoin

ABegin == /\ pc[-2] = "ABegin"
          /\ \/ /\ TRUE
                /\ pc' = [pc EXCEPT ![-2] = "Done"]
                /\ UNCHANGED <<sessState, unsearched, pending, resQ, discarded, pauseReq, abortReq, aborted, saw>>
             \/ /\ saw' = sessState
                /\ aborted' = TRUE
                /\ abortReq' = TRUE
                /\ pauseReq' = TRUE
                /\ IF sessState \in {"Idle", "Paused"}
                      THEN /\ discarded' = discarded + Len(resQ) + Len(pending) + Len(unsearched)
                           /\ resQ' = <<>>
                           /\ pending' = <<>>
                           /\ unsearched' = <<>>
                           /\ sessState' = "Stopped"
                      ELSE /\ discarded' = discarded + Len(resQ)
                           /\ resQ' = <<>>
                           /\ UNCHANGED << sessState, unsearched, pending >>
                /\ pc' = [pc EXCEPT ![-2] = "AJoin"]
          /\ UNCHANGED << phaseActive, popped, busy, holds, wdone, available, 
                          failReq, pausesDone, hostDone, it, next >>

AJoin == /\ pc[-2] = "AJoin"
         /\ sessState /= "Running"
         /\ Assert(sessState \in {"Stopped", "Error"} \/ saw \in {"Finished", "Error"}, 
                   "Failure of assertion at line 328, column 7.")
         /\ pc' = [pc EXCEPT ![-2] = "Done"]
         /\ UNCHANGED << sessState, phaseActive, unsearched, pending, resQ, 
                         popped, discarded, busy, holds, wdone, available, 
                         pauseReq, abortReq, failReq, pausesDone, hostDone, 
                         aborted, it, next, saw >>

aborter == ABegin \/ AJoin

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == driver \/ host \/ aborter
           \/ (\E self \in 1..P: producer(self))
           \/ Terminating

Spec == /\ Init /\ [][Next]_vars
        /\ \A self \in 1..P : WF_vars(producer(self))
        /\ WF_vars(driver)
        /\ WF_vars(host)
        /\ WF_vars(aborter)

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION

(***************************************************************************)
(* Checkable properties. Names double as the SPEC IDs quoted in code.      *)
(***************************************************************************)
Held == {w \in Producers : holds[w]}
Inflight == Cardinality({w \in Producers : busy[w] /= NoItem})

TypeOK ==
  /\ sessState \in {"Idle", "Running", "Paused", "Finished", "Stopped", "Error"}
  /\ phaseActive \in BOOLEAN
  /\ unsearched \in Seq(1..K)
  /\ pending \in Seq(1..K)
  /\ resQ \in Seq(1..K)
  /\ popped \in Seq(1..K)
  /\ discarded \in 0..K
  /\ \A w \in Producers : busy[w] \in 0..K
  /\ \A w \in Producers : holds[w] \in BOOLEAN
  /\ \A w \in Producers : wdone[w] \in BOOLEAN
  /\ available \in 0..BudgetTotal
  /\ pauseReq \in BOOLEAN /\ abortReq \in BOOLEAN /\ failReq \in BOOLEAN
  /\ pausesDone \in 0..MaxPauses
  /\ hostDone \in BOOLEAN /\ aborted \in BOOLEAN

(* SPEC-SESS-1: lifecycle state machine & LIFECYCLE-1/2 invariants. *)
StateMachineOK ==
  /\ (sessState = "Finished") =>
       (~phaseActive /\ Len(unsearched) = 0 /\ Len(pending) = 0 /\ Inflight = 0)
  /\ (sessState = "Paused") =>
       (~phaseActive /\ Inflight = 0 /\ (Len(unsearched) > 0 \/ Len(pending) > 0))
  /\ (sessState = "Stopped") =>
       (~phaseActive /\ Len(resQ) = 0 /\ Len(pending) = 0 /\ Len(unsearched) = 0 /\ Inflight = 0)
  /\ (sessState = "Error") =>
       (~phaseActive /\ Len(pending) = 0 /\ Len(unsearched) = 0 /\ Inflight = 0)

(* SPEC-SESS-2: pull queue obeys soft cap Cap while running without pauses,
   and lifts by at most P in-flight items per pause (or per failing phase). *)
QueueSoftBounded ==
  /\ Len(resQ) <= Cap + P * (pausesDone + (IF failReq THEN 1 ELSE 0))
  /\ (~pauseReq /\ pausesDone = 0) => Len(resQ) <= Cap

(* SPEC-SESS-3: item conservation across pause/resume/abort/error. *)
ItemConservation ==
  K = Len(unsearched) + Len(pending) + Inflight
    + Len(resQ) + Len(popped) + discarded

(* SPEC-SESS-4: no duplicate delivery; Finished + empty queue => all K items
   (unless an abort() after Finished discarded the queue). *)
NoDuplicateDelivery ==
  /\ \A i \in 1..Len(popped) :
       \A j \in 1..Len(popped) :
         i /= j => popped[i] /= popped[j]
  /\ (sessState = "Finished" /\ Len(resQ) = 0 /\ ~aborted) => Len(popped) = K

(* SPEC-SESS-5: shared ThreadBudget conservation & clean shutdown. *)
BudgetConservation == available + Cardinality(Held) = BudgetTotal

CleanShutdown ==
  /\ (~phaseActive) => (available = BudgetTotal /\ Inflight = 0)
  /\ \A w \in Producers : wdone[w] => (~holds[w] /\ busy[w] = NoItem)

(* SPEC-SESS-6: abort() is total. Once it has run the session is never
   Paused (resumable) again, and Running only while it winds down with the
   abort flag still set: neither the end-of-phase decision nor start() can
   undo it. The aborter's AJoin assert adds that abort() returns with the
   session Stopped (or Error, or Finished/Error already before it). *)
AbortTotal ==
  aborted => (sessState /= "Paused" /\ (sessState = "Running" => abortReq))

(* Liveness: every run terminates in Finished, Stopped or Error with an
   empty queue and idle workers -- even when the host pauses on a full queue
   without popping first. *)
AllTerminate ==
  <>(sessState \in Terminal /\ Len(resQ) = 0 /\ ~phaseActive /\ hostDone)
=============================================================================
