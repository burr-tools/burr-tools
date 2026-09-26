---- MODULE Pipeline ----------------------------------------------------------
(***************************************************************************)
(* End-to-end model of the BurrTools solve pipeline over ONE shared       *)
(* ThreadBudget (src/lib/thread_budget.h architecture note;               *)
(* design/2026-09-21-cpp20-concurrency.md sec. 1):                        *)
(*                                                                         *)
(*   Tier 1 (assembly): workers pop subtree tasks holding one token,      *)
(*   search, then submit found assemblies to Tier 2, yielding the token   *)
(*   across queue-space waits exactly like disassemblerpool.cpp submit(). *)
(*   Tier 2 (disassembly): workers hold one token per job and file        *)
(*   results into a bounded reorder buffer; a merger re-emits strictly    *)
(*   in submit order.                                                     *)
(*                                                                         *)
(* This spec exists for the properties neither tier spec can state: the   *)
(* GLOBAL N-active-thread bound across both tiers, end-to-end no-loss     *)
(* from task to delivered solution, and stop propagation across the       *)
(* handoff. Tier-internal protocols (budget park re-check, dropped-skip,  *)
(* salvage order) are covered by AssemblyPool.tla and DisasmPool.tla and  *)
(* reused here in simplified form.                                        *)
(*                                                                         *)
(* NOT MODELED (deliberate gaps): task generation (atomic seed of K       *)
(* tasks; the stopped-generation class is SPEC-POOL-4), dynamic splits,  *)
(* abort() (covered in DisasmPool.tla), disassembly payloads, the merger  *)
(* callback body, GUI. Submit runs token-free: the C++ hadToken           *)
(* release/reacquire around the queue-space wait is abstracted to a       *)
(* release-then-reacquire with the same observable pairing, which is      *)
(* sound for bound, deadlock and ordering properties.                     *)
(*                                                                         *)
(* INVARIANT IDs:                                                         *)
(*   SPEC-PIPE-1 ..... global bound: tokens held by both tiers <= total   *)
(*   SPEC-PIPE-2 ..... end-to-end no-loss: task -> assembly -> delivery   *)
(*   SPEC-PIPE-3 ..... merger delivers strictly in submit order           *)
(*   SPEC-PIPE-4 ..... both buffers stay bounded (queue + reorder window) *)
(*   SPEC-PIPE-5 ..... clean shutdown on every stop/drain interleaving    *)
(***************************************************************************)
EXTENDS Naturals, Integers, Sequences, FiniteSets, TLC

CONSTANTS
  NA,           (* assembly workers (C++: assembler task pool)            *)
  ND,           (* disassembly workers (C++: disassemblerPool_c workers)  *)
  BudgetTotal,  (* ONE shared ThreadBudget total (C++: pool size)         *)
  K,            (* assembly tasks seeded (each yields one assembly)       *)
  MaxQ,         (* disassembly work queue bound (C++: max_queue_size)     *)
  MaxR          (* reorder window bound (C++: max_reorder_size)           *)

ASSUME NA \in Nat /\ NA > 0
ASSUME ND \in Nat /\ ND > 0
ASSUME BudgetTotal \in Nat /\ BudgetTotal > 0
ASSUME K \in Nat /\ K > 0
ASSUME MaxQ \in Nat /\ MaxQ > 0
ASSUME MaxR \in Nat /\ MaxR > 0

AWorkers == 1..NA
DWorkers == (NA + 1)..(NA + ND)
NoJob == -1

(* --algorithm Pipeline {

  variables
    \* Tier 1 state (AssemblyTaskPool, simplified: fixed seed, no splits)
    aq = [i \in 1..K |-> i],             (* assembly task queue             *)
    activeA = 0,
    completedA = 0,                      (* == nextSubmit: one assembly each *)
    adone = [w \in AWorkers |-> FALSE],
    holdsA = [w \in AWorkers |-> FALSE],
    hasTaskA = [w \in AWorkers |-> FALSE],
    \* Shared budget
    available = BudgetTotal,
    \* Tier 2 state (disassemblerPool_c, simplified: no abort)
    dq = <<>>,                           (* disassembly work queue (seqNos) *)
    nextSubmit = 0,
    nextMerge = 0,
    filed = {},
    dropped = {},
    skipped = 0,                         (* merger skips over dropped_      *)
    delivered = <<>>,
    salvagedSeq = <<>>,
    busy = [w \in DWorkers |-> NoJob],
    finished = FALSE,
    stopReq = FALSE,
    ddone = [w \in DWorkers |-> FALSE],
    holdsD = [w \in DWorkers |-> FALSE],
    mdone = FALSE,
    lastDelivered = -1;

  \* Tier 1 worker: pop holding a token, search, submit yielding the token
  \* across queue-space waits (the submit() hadToken pattern), finish.
  \* Budget-park machinery mirrors AssemblyPool.tla (PoolWait/Park/
  \* RecheckRel/Pop/Fin/ShedTok); the task body additionally submits.
  fair process (aworker \in 1..NA)
  variables atask = 0;
  {
  ALoop:
    while (~adone[self]) {
    AWait:
      await Len(aq) > 0 \/ activeA = 0 \/ stopReq;
      if (stopReq) {
        adone[self] := TRUE;
      } else if (Len(aq) = 0) {
        assert activeA = 0;
        adone[self] := TRUE;
      } else {
        if (~holdsA[self]) {
          if (available > 0) {
            available := available - 1;
            holdsA[self] := TRUE;
          } else {
          APark:
            await available > 0 \/ stopReq;
            if (stopReq) {
              adone[self] := TRUE;
            } else {
              available := available - 1;
              holdsA[self] := TRUE;
              if (Len(aq) = 0 \/ stopReq) {
            ARecheckRel:
                available := available + 1;
                holdsA[self] := FALSE;
                if (stopReq) {
                  adone[self] := TRUE;
                } else if (Len(aq) = 0 /\ activeA = 0) {
                  adone[self] := TRUE;
                };
              };
            };
          };
        };
      APop:
        if (~adone[self] /\ holdsA[self] /\ Len(aq) > 0) {
          atask := Head(aq);
          aq := Tail(aq);
          activeA := activeA + 1;
          hasTaskA[self] := TRUE;
        };
      ASubmit:
        \* Abstract search is instantaneous; the submit then paces on the
        \* bounded disassembly queue WITHOUT holding the token
        \* (disassemblerpool.cpp submit: hadToken yields across the space
        \* wait, because the drain waited for runs on tokens; it reacquires
        \* before enqueueing). Terminal submit requeues the task
        \* (C++ retry/salvage path: back to the queue, unit discharged).
        \* Counter pairing per step, so the inventory below is exact in
        \* every state: pop moves queue->active; requeue moves
        \* active->queue; submit moves active->completed (+disasm side).
        \* (C++ bumps its progress counter separately from active--; merging
        \* the two updates is sound for counting: their order is only
        \* observable via getFinished(), which is out of scope.)
        \* The space check is a LOOP (C++ while(!hasSpace())): the check
        \* and the enqueue are split by the lock-free budget wait, across
        \* which a sibling may fill the queue, so the slot must be
        \* re-verified after every reacquire. Exiting the loop without a
        \* submit would strand the owned task (the pool wait below assumes
        \* task-free workers), so every iteration ends in requeue, submit,
        \* or a token-free retry.
        if (hasTaskA[self] /\ holdsA[self]) {
          available := available + 1;
          holdsA[self] := FALSE;
        };
      SubmitLoop:
        while (hasTaskA[self]) {
          if (stopReq) {
          ARequeue:
            aq := Append(aq, atask);
            activeA := activeA - 1;
            atask := 0;
            hasTaskA[self] := FALSE;
          } else {
        ASpace:
          await (Len(dq) < MaxQ /\ nextSubmit - nextMerge < MaxR) \/ stopReq;
            if (stopReq) {
              aq := Append(aq, atask);
              activeA := activeA - 1;
              atask := 0;
              hasTaskA[self] := FALSE;
            } else {
          AReacquire:
              await available > 0;
              available := available - 1;
              holdsA[self] := TRUE;
              if (Len(dq) < MaxQ /\ nextSubmit - nextMerge < MaxR) {
                dq := Append(dq, nextSubmit);
                nextSubmit := nextSubmit + 1;
                completedA := completedA + 1;
                activeA := activeA - 1;
                atask := 0;
                hasTaskA[self] := FALSE;
              } else {
            ARetryRel:
                \* Slot lost to a sibling across the budget wait: release
                \* and retry token-free (C++ loops back to hasSpace()).
                available := available + 1;
                holdsA[self] := FALSE;
              };
            };
          };
        };
      AFin:
        \* finishTask: return the token (no-op unless held). Also sheds a
        \* reservation stranded by a label split between reserve and pop
        \* (see AssemblyPool.tla ShedTok): reaching here holding a token
        \* without a task is model-only, and must not survive the step.
        if (holdsA[self]) {
          available := available + 1;
          holdsA[self] := FALSE;
        };
      };
    };
  }

  \* Tier 2 worker: pop, hold one token across the job, file in order.
  fair process (dworker \in (NA + 1)..(NA + ND))
  variables seq = NoJob;
  {
  DLoop:
    while (~ddone[self]) {
    DWait:
      \* No stopReq in this predicate (faithful to worker_loop).
      await Len(dq) > 0 \/ finished;
      if (Len(dq) = 0) {
        if (finished) {
          ddone[self] := TRUE;
        };
      } else {
        seq := Head(dq);
        dq := Tail(dq);
        busy[self] := seq;
        \* Budget gate: one shared token across the job. The park has no
        \* terminal exit (faithful: finish()/requestStop() both require
        \* draining to proceed); every holder always completes because all
        \* work steps are atomic, so TLC proves the park always resolves.
        if (~holdsD[self]) {
          if (available > 0) {
            available := available - 1;
            holdsD[self] := TRUE;
          } else {
        DPark:
            await available > 0;
            available := available - 1;
            holdsD[self] := TRUE;
          };
        };
        \* Abstract disassembly, then file (window slot guaranteed).
      DFile:
        await Cardinality(filed) < MaxR;
        filed := filed \cup {seq};
        busy[self] := NoJob;
        seq := NoJob;
        if (holdsD[self]) {
          available := available + 1;
          holdsD[self] := FALSE;
        };
      };
    };
  }

  fair process (merger = 0)
  {
  MLoop:
    while (~mdone) {
    MWait:
      await (nextMerge \in filed) \/ (nextMerge \in dropped)
         \/ (finished /\ nextMerge = nextSubmit);
      if (nextMerge \in dropped) {
        dropped := dropped \ {nextMerge};
        skipped := skipped + 1;
        nextMerge := nextMerge + 1;
      } else if (nextMerge \in filed) {
        assert nextMerge > lastDelivered;
        filed := filed \ {nextMerge};
        delivered := Append(delivered, nextMerge);
        lastDelivered := nextMerge;
        nextMerge := nextMerge + 1;
      } else {
        assert finished /\ nextMerge = nextSubmit;
        mdone := TRUE;
      };
    };
  }

  \* requestStop(): salvage the disassembly queue (numbered, for skip);
  \* assembly tasks stay queued (C++ drain() preserves them for resume).
  process (stopper = -1)
  {
  StopDo:
    either {
      stopReq := TRUE;
    DSalvLoop:
      while (Len(dq) > 0) {
        salvagedSeq := Append(salvagedSeq, Head(dq));
        dropped := dropped \cup {Head(dq)};
        dq := Tail(dq);
      };
    } or {
      skip;
    };
  }

  \* finish() once Tier 1 is fully submitted, or after a stop once its
  \* workers are gone (C++ joins the assembler before draining Tier 2).
  fair process (finisher = -2)
  {
  FinWait:
    await completedA = K \/ (stopReq /\ (\A w \in AWorkers : adone[w]));
    finished := TRUE;
  }

} *)
\* BEGIN TRANSLATION (chksum(pcal) = "d905299f" /\ chksum(tla) = "8c8af07a")
VARIABLES aq, activeA, completedA, adone, holdsA, hasTaskA, available, dq, 
          nextSubmit, nextMerge, filed, dropped, skipped, delivered, 
          salvagedSeq, busy, finished, stopReq, ddone, holdsD, mdone, 
          lastDelivered, pc, atask, seq

vars == << aq, activeA, completedA, adone, holdsA, hasTaskA, available, dq, 
           nextSubmit, nextMerge, filed, dropped, skipped, delivered, 
           salvagedSeq, busy, finished, stopReq, ddone, holdsD, mdone, 
           lastDelivered, pc, atask, seq >>

ProcSet == (1..NA) \cup ((NA + 1)..(NA + ND)) \cup {0} \cup {-1} \cup {-2}

Init == (* Global variables *)
        /\ aq = [i \in 1..K |-> i]
        /\ activeA = 0
        /\ completedA = 0
        /\ adone = [w \in AWorkers |-> FALSE]
        /\ holdsA = [w \in AWorkers |-> FALSE]
        /\ hasTaskA = [w \in AWorkers |-> FALSE]
        /\ available = BudgetTotal
        /\ dq = <<>>
        /\ nextSubmit = 0
        /\ nextMerge = 0
        /\ filed = {}
        /\ dropped = {}
        /\ skipped = 0
        /\ delivered = <<>>
        /\ salvagedSeq = <<>>
        /\ busy = [w \in DWorkers |-> NoJob]
        /\ finished = FALSE
        /\ stopReq = FALSE
        /\ ddone = [w \in DWorkers |-> FALSE]
        /\ holdsD = [w \in DWorkers |-> FALSE]
        /\ mdone = FALSE
        /\ lastDelivered = -1
        (* Process aworker *)
        /\ atask = [self \in 1..NA |-> 0]
        (* Process dworker *)
        /\ seq = [self \in (NA + 1)..(NA + ND) |-> NoJob]
        /\ pc = [self \in ProcSet |-> CASE self \in 1..NA -> "ALoop"
                                        [] self \in (NA + 1)..(NA + ND) -> "DLoop"
                                        [] self = 0 -> "MLoop"
                                        [] self = -1 -> "StopDo"
                                        [] self = -2 -> "FinWait"]

ALoop(self) == /\ pc[self] = "ALoop"
               /\ IF ~adone[self]
                     THEN /\ pc' = [pc EXCEPT ![self] = "AWait"]
                     ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
               /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, 
                               hasTaskA, available, dq, nextSubmit, nextMerge, 
                               filed, dropped, skipped, delivered, salvagedSeq, 
                               busy, finished, stopReq, ddone, holdsD, mdone, 
                               lastDelivered, atask, seq >>

AWait(self) == /\ pc[self] = "AWait"
               /\ Len(aq) > 0 \/ activeA = 0 \/ stopReq
               /\ IF stopReq
                     THEN /\ adone' = [adone EXCEPT ![self] = TRUE]
                          /\ pc' = [pc EXCEPT ![self] = "ALoop"]
                          /\ UNCHANGED << holdsA, available >>
                     ELSE /\ IF Len(aq) = 0
                                THEN /\ Assert(activeA = 0, 
                                               "Failure of assertion at line 100, column 9.")
                                     /\ adone' = [adone EXCEPT ![self] = TRUE]
                                     /\ pc' = [pc EXCEPT ![self] = "ALoop"]
                                     /\ UNCHANGED << holdsA, available >>
                                ELSE /\ IF ~holdsA[self]
                                           THEN /\ IF available > 0
                                                      THEN /\ available' = available - 1
                                                           /\ holdsA' = [holdsA EXCEPT ![self] = TRUE]
                                                           /\ pc' = [pc EXCEPT ![self] = "APop"]
                                                      ELSE /\ pc' = [pc EXCEPT ![self] = "APark"]
                                                           /\ UNCHANGED << holdsA, 
                                                                           available >>
                                           ELSE /\ pc' = [pc EXCEPT ![self] = "APop"]
                                                /\ UNCHANGED << holdsA, 
                                                                available >>
                                     /\ adone' = adone
               /\ UNCHANGED << aq, activeA, completedA, hasTaskA, dq, 
                               nextSubmit, nextMerge, filed, dropped, skipped, 
                               delivered, salvagedSeq, busy, finished, stopReq, 
                               ddone, holdsD, mdone, lastDelivered, atask, seq >>

APop(self) == /\ pc[self] = "APop"
              /\ IF ~adone[self] /\ holdsA[self] /\ Len(aq) > 0
                    THEN /\ atask' = [atask EXCEPT ![self] = Head(aq)]
                         /\ aq' = Tail(aq)
                         /\ activeA' = activeA + 1
                         /\ hasTaskA' = [hasTaskA EXCEPT ![self] = TRUE]
                    ELSE /\ TRUE
                         /\ UNCHANGED << aq, activeA, hasTaskA, atask >>
              /\ pc' = [pc EXCEPT ![self] = "ASubmit"]
              /\ UNCHANGED << completedA, adone, holdsA, available, dq, 
                              nextSubmit, nextMerge, filed, dropped, skipped, 
                              delivered, salvagedSeq, busy, finished, stopReq, 
                              ddone, holdsD, mdone, lastDelivered, seq >>

ASubmit(self) == /\ pc[self] = "ASubmit"
                 /\ IF hasTaskA[self] /\ holdsA[self]
                       THEN /\ available' = available + 1
                            /\ holdsA' = [holdsA EXCEPT ![self] = FALSE]
                       ELSE /\ TRUE
                            /\ UNCHANGED << holdsA, available >>
                 /\ pc' = [pc EXCEPT ![self] = "SubmitLoop"]
                 /\ UNCHANGED << aq, activeA, completedA, adone, hasTaskA, dq, 
                                 nextSubmit, nextMerge, filed, dropped, 
                                 skipped, delivered, salvagedSeq, busy, 
                                 finished, stopReq, ddone, holdsD, mdone, 
                                 lastDelivered, atask, seq >>

SubmitLoop(self) == /\ pc[self] = "SubmitLoop"
                    /\ IF hasTaskA[self]
                          THEN /\ IF stopReq
                                     THEN /\ pc' = [pc EXCEPT ![self] = "ARequeue"]
                                     ELSE /\ pc' = [pc EXCEPT ![self] = "ASpace"]
                          ELSE /\ pc' = [pc EXCEPT ![self] = "AFin"]
                    /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, 
                                    hasTaskA, available, dq, nextSubmit, 
                                    nextMerge, filed, dropped, skipped, 
                                    delivered, salvagedSeq, busy, finished, 
                                    stopReq, ddone, holdsD, mdone, 
                                    lastDelivered, atask, seq >>

ARequeue(self) == /\ pc[self] = "ARequeue"
                  /\ aq' = Append(aq, atask[self])
                  /\ activeA' = activeA - 1
                  /\ atask' = [atask EXCEPT ![self] = 0]
                  /\ hasTaskA' = [hasTaskA EXCEPT ![self] = FALSE]
                  /\ pc' = [pc EXCEPT ![self] = "SubmitLoop"]
                  /\ UNCHANGED << completedA, adone, holdsA, available, dq, 
                                  nextSubmit, nextMerge, filed, dropped, 
                                  skipped, delivered, salvagedSeq, busy, 
                                  finished, stopReq, ddone, holdsD, mdone, 
                                  lastDelivered, seq >>

ASpace(self) == /\ pc[self] = "ASpace"
                /\ (Len(dq) < MaxQ /\ nextSubmit - nextMerge < MaxR) \/ stopReq
                /\ IF stopReq
                      THEN /\ aq' = Append(aq, atask[self])
                           /\ activeA' = activeA - 1
                           /\ atask' = [atask EXCEPT ![self] = 0]
                           /\ hasTaskA' = [hasTaskA EXCEPT ![self] = FALSE]
                           /\ pc' = [pc EXCEPT ![self] = "SubmitLoop"]
                      ELSE /\ pc' = [pc EXCEPT ![self] = "AReacquire"]
                           /\ UNCHANGED << aq, activeA, hasTaskA, atask >>
                /\ UNCHANGED << completedA, adone, holdsA, available, dq, 
                                nextSubmit, nextMerge, filed, dropped, skipped, 
                                delivered, salvagedSeq, busy, finished, 
                                stopReq, ddone, holdsD, mdone, lastDelivered, 
                                seq >>

AReacquire(self) == /\ pc[self] = "AReacquire"
                    /\ available > 0
                    /\ available' = available - 1
                    /\ holdsA' = [holdsA EXCEPT ![self] = TRUE]
                    /\ IF Len(dq) < MaxQ /\ nextSubmit - nextMerge < MaxR
                          THEN /\ dq' = Append(dq, nextSubmit)
                               /\ nextSubmit' = nextSubmit + 1
                               /\ completedA' = completedA + 1
                               /\ activeA' = activeA - 1
                               /\ atask' = [atask EXCEPT ![self] = 0]
                               /\ hasTaskA' = [hasTaskA EXCEPT ![self] = FALSE]
                               /\ pc' = [pc EXCEPT ![self] = "SubmitLoop"]
                          ELSE /\ pc' = [pc EXCEPT ![self] = "ARetryRel"]
                               /\ UNCHANGED << activeA, completedA, hasTaskA, 
                                               dq, nextSubmit, atask >>
                    /\ UNCHANGED << aq, adone, nextMerge, filed, dropped, 
                                    skipped, delivered, salvagedSeq, busy, 
                                    finished, stopReq, ddone, holdsD, mdone, 
                                    lastDelivered, seq >>

ARetryRel(self) == /\ pc[self] = "ARetryRel"
                   /\ available' = available + 1
                   /\ holdsA' = [holdsA EXCEPT ![self] = FALSE]
                   /\ pc' = [pc EXCEPT ![self] = "SubmitLoop"]
                   /\ UNCHANGED << aq, activeA, completedA, adone, hasTaskA, 
                                   dq, nextSubmit, nextMerge, filed, dropped, 
                                   skipped, delivered, salvagedSeq, busy, 
                                   finished, stopReq, ddone, holdsD, mdone, 
                                   lastDelivered, atask, seq >>

AFin(self) == /\ pc[self] = "AFin"
              /\ IF holdsA[self]
                    THEN /\ available' = available + 1
                         /\ holdsA' = [holdsA EXCEPT ![self] = FALSE]
                    ELSE /\ TRUE
                         /\ UNCHANGED << holdsA, available >>
              /\ pc' = [pc EXCEPT ![self] = "ALoop"]
              /\ UNCHANGED << aq, activeA, completedA, adone, hasTaskA, dq, 
                              nextSubmit, nextMerge, filed, dropped, skipped, 
                              delivered, salvagedSeq, busy, finished, stopReq, 
                              ddone, holdsD, mdone, lastDelivered, atask, seq >>

APark(self) == /\ pc[self] = "APark"
               /\ available > 0 \/ stopReq
               /\ IF stopReq
                     THEN /\ adone' = [adone EXCEPT ![self] = TRUE]
                          /\ pc' = [pc EXCEPT ![self] = "APop"]
                          /\ UNCHANGED << holdsA, available >>
                     ELSE /\ available' = available - 1
                          /\ holdsA' = [holdsA EXCEPT ![self] = TRUE]
                          /\ IF Len(aq) = 0 \/ stopReq
                                THEN /\ pc' = [pc EXCEPT ![self] = "ARecheckRel"]
                                ELSE /\ pc' = [pc EXCEPT ![self] = "APop"]
                          /\ adone' = adone
               /\ UNCHANGED << aq, activeA, completedA, hasTaskA, dq, 
                               nextSubmit, nextMerge, filed, dropped, skipped, 
                               delivered, salvagedSeq, busy, finished, stopReq, 
                               ddone, holdsD, mdone, lastDelivered, atask, seq >>

ARecheckRel(self) == /\ pc[self] = "ARecheckRel"
                     /\ available' = available + 1
                     /\ holdsA' = [holdsA EXCEPT ![self] = FALSE]
                     /\ IF stopReq
                           THEN /\ adone' = [adone EXCEPT ![self] = TRUE]
                           ELSE /\ IF Len(aq) = 0 /\ activeA = 0
                                      THEN /\ adone' = [adone EXCEPT ![self] = TRUE]
                                      ELSE /\ TRUE
                                           /\ adone' = adone
                     /\ pc' = [pc EXCEPT ![self] = "APop"]
                     /\ UNCHANGED << aq, activeA, completedA, hasTaskA, dq, 
                                     nextSubmit, nextMerge, filed, dropped, 
                                     skipped, delivered, salvagedSeq, busy, 
                                     finished, stopReq, ddone, holdsD, mdone, 
                                     lastDelivered, atask, seq >>

aworker(self) == ALoop(self) \/ AWait(self) \/ APop(self) \/ ASubmit(self)
                    \/ SubmitLoop(self) \/ ARequeue(self) \/ ASpace(self)
                    \/ AReacquire(self) \/ ARetryRel(self) \/ AFin(self)
                    \/ APark(self) \/ ARecheckRel(self)

DLoop(self) == /\ pc[self] = "DLoop"
               /\ IF ~ddone[self]
                     THEN /\ pc' = [pc EXCEPT ![self] = "DWait"]
                     ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
               /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, 
                               hasTaskA, available, dq, nextSubmit, nextMerge, 
                               filed, dropped, skipped, delivered, salvagedSeq, 
                               busy, finished, stopReq, ddone, holdsD, mdone, 
                               lastDelivered, atask, seq >>

DWait(self) == /\ pc[self] = "DWait"
               /\ Len(dq) > 0 \/ finished
               /\ IF Len(dq) = 0
                     THEN /\ IF finished
                                THEN /\ ddone' = [ddone EXCEPT ![self] = TRUE]
                                ELSE /\ TRUE
                                     /\ ddone' = ddone
                          /\ pc' = [pc EXCEPT ![self] = "DLoop"]
                          /\ UNCHANGED << available, dq, busy, holdsD, seq >>
                     ELSE /\ seq' = [seq EXCEPT ![self] = Head(dq)]
                          /\ dq' = Tail(dq)
                          /\ busy' = [busy EXCEPT ![self] = seq'[self]]
                          /\ IF ~holdsD[self]
                                THEN /\ IF available > 0
                                           THEN /\ available' = available - 1
                                                /\ holdsD' = [holdsD EXCEPT ![self] = TRUE]
                                                /\ pc' = [pc EXCEPT ![self] = "DFile"]
                                           ELSE /\ pc' = [pc EXCEPT ![self] = "DPark"]
                                                /\ UNCHANGED << available, 
                                                                holdsD >>
                                ELSE /\ pc' = [pc EXCEPT ![self] = "DFile"]
                                     /\ UNCHANGED << available, holdsD >>
                          /\ ddone' = ddone
               /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, 
                               hasTaskA, nextSubmit, nextMerge, filed, dropped, 
                               skipped, delivered, salvagedSeq, finished, 
                               stopReq, mdone, lastDelivered, atask >>

DFile(self) == /\ pc[self] = "DFile"
               /\ Cardinality(filed) < MaxR
               /\ filed' = (filed \cup {seq[self]})
               /\ busy' = [busy EXCEPT ![self] = NoJob]
               /\ seq' = [seq EXCEPT ![self] = NoJob]
               /\ IF holdsD[self]
                     THEN /\ available' = available + 1
                          /\ holdsD' = [holdsD EXCEPT ![self] = FALSE]
                     ELSE /\ TRUE
                          /\ UNCHANGED << available, holdsD >>
               /\ pc' = [pc EXCEPT ![self] = "DLoop"]
               /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, 
                               hasTaskA, dq, nextSubmit, nextMerge, dropped, 
                               skipped, delivered, salvagedSeq, finished, 
                               stopReq, ddone, mdone, lastDelivered, atask >>

DPark(self) == /\ pc[self] = "DPark"
               /\ available > 0
               /\ available' = available - 1
               /\ holdsD' = [holdsD EXCEPT ![self] = TRUE]
               /\ pc' = [pc EXCEPT ![self] = "DFile"]
               /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, 
                               hasTaskA, dq, nextSubmit, nextMerge, filed, 
                               dropped, skipped, delivered, salvagedSeq, busy, 
                               finished, stopReq, ddone, mdone, lastDelivered, 
                               atask, seq >>

dworker(self) == DLoop(self) \/ DWait(self) \/ DFile(self) \/ DPark(self)

MLoop == /\ pc[0] = "MLoop"
         /\ IF ~mdone
               THEN /\ pc' = [pc EXCEPT ![0] = "MWait"]
               ELSE /\ pc' = [pc EXCEPT ![0] = "Done"]
         /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, hasTaskA, 
                         available, dq, nextSubmit, nextMerge, filed, dropped, 
                         skipped, delivered, salvagedSeq, busy, finished, 
                         stopReq, ddone, holdsD, mdone, lastDelivered, atask, 
                         seq >>

MWait == /\ pc[0] = "MWait"
         /\    (nextMerge \in filed) \/ (nextMerge \in dropped)
            \/ (finished /\ nextMerge = nextSubmit)
         /\ IF nextMerge \in dropped
               THEN /\ dropped' = dropped \ {nextMerge}
                    /\ skipped' = skipped + 1
                    /\ nextMerge' = nextMerge + 1
                    /\ UNCHANGED << filed, delivered, mdone, lastDelivered >>
               ELSE /\ IF nextMerge \in filed
                          THEN /\ Assert(nextMerge > lastDelivered, 
                                         "Failure of assertion at line 268, column 9.")
                               /\ filed' = filed \ {nextMerge}
                               /\ delivered' = Append(delivered, nextMerge)
                               /\ lastDelivered' = nextMerge
                               /\ nextMerge' = nextMerge + 1
                               /\ mdone' = mdone
                          ELSE /\ Assert(finished /\ nextMerge = nextSubmit, 
                                         "Failure of assertion at line 274, column 9.")
                               /\ mdone' = TRUE
                               /\ UNCHANGED << nextMerge, filed, delivered, 
                                               lastDelivered >>
                    /\ UNCHANGED << dropped, skipped >>
         /\ pc' = [pc EXCEPT ![0] = "MLoop"]
         /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, hasTaskA, 
                         available, dq, nextSubmit, salvagedSeq, busy, 
                         finished, stopReq, ddone, holdsD, atask, seq >>

merger == MLoop \/ MWait

StopDo == /\ pc[-1] = "StopDo"
          /\ \/ /\ stopReq' = TRUE
                /\ pc' = [pc EXCEPT ![-1] = "DSalvLoop"]
             \/ /\ TRUE
                /\ pc' = [pc EXCEPT ![-1] = "Done"]
                /\ UNCHANGED stopReq
          /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, hasTaskA, 
                          available, dq, nextSubmit, nextMerge, filed, dropped, 
                          skipped, delivered, salvagedSeq, busy, finished, 
                          ddone, holdsD, mdone, lastDelivered, atask, seq >>

DSalvLoop == /\ pc[-1] = "DSalvLoop"
             /\ IF Len(dq) > 0
                   THEN /\ salvagedSeq' = Append(salvagedSeq, Head(dq))
                        /\ dropped' = (dropped \cup {Head(dq)})
                        /\ dq' = Tail(dq)
                        /\ pc' = [pc EXCEPT ![-1] = "DSalvLoop"]
                   ELSE /\ pc' = [pc EXCEPT ![-1] = "Done"]
                        /\ UNCHANGED << dq, dropped, salvagedSeq >>
             /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, hasTaskA, 
                             available, nextSubmit, nextMerge, filed, skipped, 
                             delivered, busy, finished, stopReq, ddone, holdsD, 
                             mdone, lastDelivered, atask, seq >>

stopper == StopDo \/ DSalvLoop

FinWait == /\ pc[-2] = "FinWait"
           /\ completedA = K \/ (stopReq /\ (\A w \in AWorkers : adone[w]))
           /\ finished' = TRUE
           /\ pc' = [pc EXCEPT ![-2] = "Done"]
           /\ UNCHANGED << aq, activeA, completedA, adone, holdsA, hasTaskA, 
                           available, dq, nextSubmit, nextMerge, filed, 
                           dropped, skipped, delivered, salvagedSeq, busy, 
                           stopReq, ddone, holdsD, mdone, lastDelivered, atask, 
                           seq >>

finisher == FinWait

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == merger \/ stopper \/ finisher
           \/ (\E self \in 1..NA: aworker(self))
           \/ (\E self \in (NA + 1)..(NA + ND): dworker(self))
           \/ Terminating

Spec == /\ Init /\ [][Next]_vars
        /\ \A self \in 1..NA : WF_vars(aworker(self))
        /\ \A self \in (NA + 1)..(NA + ND) : WF_vars(dworker(self))
        /\ WF_vars(merger)
        /\ WF_vars(finisher)

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION 

(***************************************************************************)
(* Checkable properties.                                                   *)
(***************************************************************************)
HeldA == {w \in AWorkers : holdsA[w]}
HeldD == {w \in DWorkers : holdsD[w]}
TaskHoldersA == {w \in AWorkers : hasTaskA[w]}
InflightD == Cardinality({w \in DWorkers : busy[w] /= NoJob})

TypeOK ==
  /\ aq \in Seq(Nat)
  /\ activeA \in Nat /\ completedA \in Nat
  /\ available \in 0..BudgetTotal
  /\ dq \in Seq(Nat)
  /\ nextSubmit \in Nat /\ nextMerge \in Nat /\ skipped \in Nat
  /\ filed \subseteq Nat /\ dropped \subseteq Nat
  /\ \A i \in 1..Len(delivered) : delivered[i] \in Nat
  /\ \A i \in 1..Len(salvagedSeq) : salvagedSeq[i] \in Nat
  /\ \A w \in DWorkers : busy[w] = NoJob \/ busy[w] \in Nat
  /\ lastDelivered \in Int
  /\ finished \in BOOLEAN /\ stopReq \in BOOLEAN

(* SPEC-PIPE-1: one shared budget across both tiers. Takes/returns pair
   up globally, so working threads (one token each) never exceed total.
   Note what is NOT bounded: open assembly tasks plus in-flight
   disassembly CAN exceed total -- an assembler yielded in its submit
   wait (token released, task still open) while a disassembler works is
   the designed pipeline overlap. The bound is on holders, i.e. threads
   actually executing work. *)
BudgetConservation ==
  available + Cardinality(HeldA) + Cardinality(HeldD) = BudgetTotal
WorkingBounded ==
  Cardinality(HeldA) + Cardinality(HeldD) <= BudgetTotal

(* Tier 1 pairing + inventory (cf. SPEC-POOL-1/2, simplified: no splits).
   Every step conserves by construction (see ASubmit comment). *)
PairingA == activeA = Cardinality(TaskHoldersA)
AssemblyConservation == completedA + activeA + Len(aq) = K

(* Each completion submits exactly one assembly, in the same step. *)
SubmittedEqualsCompleted == nextSubmit = completedA

(* Tier 2 inventory (cf. SPEC-DIS-5, simplified: no abort/refuse paths). *)
DisasmConservation ==
  nextSubmit = Len(delivered) + Len(dq) + InflightD
             + Cardinality(filed) + Cardinality(dropped) + skipped

(* SPEC-PIPE-3: ordered delivery. *)
OrderedDelivery ==
  \A i \in 1..Len(delivered) :
    \A j \in 1..Len(delivered) :
      i < j => delivered[i] < delivered[j]

(* SPEC-PIPE-4: both buffers stay bounded. *)
QueueBounded == Len(dq) <= MaxQ
ReorderBounded == Cardinality(filed) <= MaxR
WindowBounded == nextSubmit - nextMerge <= MaxR

(* SPEC-PIPE-5: clean shutdown. *)
CleanShutdown ==
  /\ \A w \in AWorkers : adone[w] => (~hasTaskA[w] /\ ~holdsA[w])
  /\ \A w \in DWorkers : ddone[w] => busy[w] = NoJob
  /\ mdone => filed = {} /\ dropped = {}

(* Liveness: every run ends -- full submission and drain, or stop with
   salvaged queue drained through skips and in-flight completion. *)
AllTerminate ==
  <>((\A w \in AWorkers : adone[w]) /\ (\A w \in DWorkers : ddone[w]) /\ mdone)
=============================================================================
