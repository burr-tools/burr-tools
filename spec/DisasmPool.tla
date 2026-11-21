---- MODULE DisasmPool -------------------------------------------------------
(***************************************************************************)
(* Formal protocol model of BurrTools' disassembly side:                  *)
(*                                                                         *)
(*   - disassemblerPool_c ... src/lib/disassemblerpool.{h,cpp}            *)
(*                                                                         *)
(* SCOPE. The pool protocol only: bounded work queue, worker pickup,      *)
(* direct delivery by the worker that finished a job (in any order), and  *)
(* the finish / abort / requestStop lifecycle with salvage. Assemblies    *)
(* and separations are opaque; disassembly itself is one atomic step.     *)
(* The callback (on_result into solverSession_c) is abstracted as         *)
(* appending to `delivered`.                                              *)
(*                                                                         *)
(* NOT MODELED (deliberate gaps, see README.md):                          *)
(*   - Inline mode (num_threads == 1 / BURRTOOLS_NO_DISASM_POOL):          *)
(*     sequential under inline_mutex, gap-free by construction.            *)
(*   - Shared ThreadBudget contention: workers process one job at a time  *)
(*     (capacity M by construction). Tying the budget to the assembly     *)
(*     side is the Pipeline composition spec's job.                       *)
(*   - Exceptions: worker_exception/abort-on-throw is covered as an       *)
(*     arbitrary-time abort() (same observable protocol).                 *)
(*   - C++ locks/mutexes and the C++ memory model (as in AssemblyPool).   *)
(*   - Payload content: placementCount()==1 shortcuts, movement analysis. *)
(*   - Backpressure from the callback (the session's bounded queue): a    *)
(*     delivering worker is one step here; SolverSession.tla covers it.   *)
(*   - abort() clearing salvaged_: the model keeps salvagedSeq across     *)
(*     abort (SPEC-DIS-5 "never lost" is stronger than the code there),   *)
(*     fine by design since abort runs only on the assert path and in the *)
(*     destructor after takeSalvaged().                                   *)
(*                                                                         *)
(* INVARIANT IDs (shared with code comments, stable across refactors):    *)
(*   SPEC-DIS-1 ...... delivered seqNos are unique and gap-free: a stop   *)
(*                     salvages a suffix of the submit numbering, so a    *)
(*                     drained pool delivered exactly 0 .. n-1 (in any    *)
(*                     order; delivery order is unspecified)              *)
(*   SPEC-DIS-2 ...... work queue stays bounded                           *)
(*   SPEC-DIS-3 ...... retired 2026-11-20 (reorder buffer bound: the      *)
(*                     ordered merger and its buffer are gone)            *)
(*   SPEC-DIS-4 ...... retired 2026-11-20 (submit window, ditto)          *)
(*   SPEC-DIS-5 ...... every offered assembly is delivered, queued, in   *)
(*                     flight, salvaged, or discarded -- never lost,      *)
(*                     never duplicated                                   *)
(*   SPEC-DIS-6 ...... salvaged assemblies keep submit order              *)
(*   SPEC-DIS-7 ...... clean shutdown: no stranded jobs                   *)
(***************************************************************************)
EXTENDS Naturals, Integers, Sequences, FiniteSets, TLC

CONSTANTS
  M,          (* disassembler worker count (C++: num_threads)              *)
  NSubmits,   (* assemblies the submitter offers (C++: unbounded stream)  *)
  MaxQ        (* work queue bound (C++: max_queue_size, 64)                *)

ASSUME M \in Nat /\ M > 0
ASSUME NSubmits \in Nat /\ NSubmits > 0
ASSUME MaxQ \in Nat /\ MaxQ > 0

Workers == 1..M
NoJob == -1  (* busy[w] = -1 means worker w holds no job; seqNos start at 0 *)

(* --algorithm DisasmPool {

  variables
    queue = <<>>,                        (* work_queue (seqNos, FIFO)       *)
    nextSubmit = 0,                      (* next_submit_seq                 *)
    delivered = <<>>,                    (* on_result calls, in order       *)
    salvagedSeq = <<>>,                  (* numbered salvage, submit order  *)
    refused = 0,                         (* terminal-time refuses (no seq)  *)
    discarded = 0,                       (* jobs destroyed by abort()       *)
    offered = 0,                         (* assemblies offered so far       *)
    busy = [w \in Workers |-> NoJob],    (* checked-out job per worker      *)
    finished = FALSE,
    aborted = FALSE,
    stopReq = FALSE,                     (* stop_requested                  *)
    sdone = FALSE,
    wdone = [w \in Workers |-> FALSE];

  \* Assembler side: offers NSubmits assemblies, each either enqueued with
  \* the next seqNo or refused into salvage once terminal. In C++ submits
  \* come from many assembler threads serialized under queue_mutex; one
  \* submitter is protocol-equivalent.
  fair process (submitter = M + 1)
  {
  SLoop:
    while (offered < NSubmits) {
      if (finished \/ aborted \/ stopReq) {
        \* Terminal refuse (disassemblerpool.cpp submit: salvageAssembly,
        \* no seqNo assigned, so the numbering stays gap-free).
        refused := refused + 1;
        offered := offered + 1;
      } else {
      SSpace:
        \* Queue room (hasSpace): the only backpressure (SPEC-DIS-2).
        await Len(queue) < MaxQ \/ finished \/ aborted \/ stopReq;
        if (finished \/ aborted \/ stopReq) {
          refused := refused + 1;
          offered := offered + 1;
        } else {
          queue := Append(queue, nextSubmit);
          nextSubmit := nextSubmit + 1;
          offered := offered + 1;
        };
      };
    };
  SDone:
    sdone := TRUE;
  }

  fair process (worker \in 1..M)
  variables seq = NoJob;
  {
  WLoop:
    while (~wdone[self]) {
    WWait:
      \* NOTE: no stopReq in this predicate (disassemblerpool.cpp
      \* worker_loop). requestStop() wakes parked workers, which re-wait
      \* when the salvaged queue is empty and finish() has not run yet.
      await Len(queue) > 0 \/ finished \/ aborted;
      if (aborted) {
        wdone[self] := TRUE;
      } else if (Len(queue) = 0) {
        \* Spurious/terminal wake with nothing to do: finish or re-wait.
        if (finished) {
          wdone[self] := TRUE;
        };
      } else {
        seq := Head(queue);
        queue := Tail(queue);
        busy[self] := seq;
        \* Abstract disassembly; the budget gate is uncapped in this spec
        \* (see header: shared-budget contention is the Pipeline spec).
      WDeliver:
        \* The worker hands its result to on_result itself, in whatever
        \* order the jobs finish. A job in flight at abort() is dropped
        \* (the aborter owns clearing busy[self]).
        if (aborted) {
          wdone[self] := TRUE;
        } else {
          delivered := Append(delivered, seq);
          busy[self] := NoJob;
          seq := NoJob;
        };
      };
    };
  }

  \* GUI/orchestrator: requestStop() at an arbitrary point. C++ sets
  \* stop_requested first, then salvages the queued (never-started)
  \* assemblies holding queue_mutex across the whole drain: one atomic
  \* step here (Salv). That atomicity is load-bearing for SPEC-DIS-1: an
  \* item-by-item salvage interleaved with worker pops would let a worker
  \* deliver a seqNo above one already salvaged, leaving a gap
  \* (mutation-checked). Fair with a skip branch: skipping covers
  \* never-stopping, fairness guarantees a started stop salvages.
  fair process (stopper = -1)
  {
  StopDo:
    either {
      stopReq := TRUE;
    Salv:
      salvagedSeq := salvagedSeq \o queue;
      queue := <<>>;
    } or {
      skip;
    };
  }

  \* Emergency teardown (C++ abort(): destructor, worker_exception path).
  \* Discards everything in one atomic step; workers exit on the flag.
  \* Unfair: arbitrary-time abort, or never.
  process (aborter = -2)
  {
  AbortDo:
    either {
      aborted := TRUE;
      discarded := discarded + Len(queue)
                 + Cardinality({w \in Workers : busy[w] /= NoJob});
      queue := <<>>;
      busy := [w \in Workers |-> NoJob];
    } or {
      skip;
    };
  }

  \* Normal teardown (C++ finish()): once every offer resolved. Fair, so a
  \* never-stop-never-abort run still ends. Harmless after abort (workers
  \* already exit on the flag).
  fair process (finisher = -3)
  {
  FinWait:
    await offered = NSubmits;
    if (~aborted) {
      finished := TRUE;
    };
  }

} *)
\* BEGIN TRANSLATION
VARIABLES queue, nextSubmit, delivered, salvagedSeq, refused, discarded, 
          offered, busy, finished, aborted, stopReq, sdone, wdone, pc, seq

vars == << queue, nextSubmit, delivered, salvagedSeq, refused, discarded, 
           offered, busy, finished, aborted, stopReq, sdone, wdone, pc, seq
        >>

ProcSet == {M + 1} \cup (1..M) \cup {-1} \cup {-2} \cup {-3}

Init == (* Global variables *)
        /\ queue = <<>>
        /\ nextSubmit = 0
        /\ delivered = <<>>
        /\ salvagedSeq = <<>>
        /\ refused = 0
        /\ discarded = 0
        /\ offered = 0
        /\ busy = [w \in Workers |-> NoJob]
        /\ finished = FALSE
        /\ aborted = FALSE
        /\ stopReq = FALSE
        /\ sdone = FALSE
        /\ wdone = [w \in Workers |-> FALSE]
        (* Process worker *)
        /\ seq = [self \in 1..M |-> NoJob]
        /\ pc = [self \in ProcSet |-> CASE self = M + 1 -> "SLoop"
                                        [] self \in 1..M -> "WLoop"
                                        [] self = -1 -> "StopDo"
                                        [] self = -2 -> "AbortDo"
                                        [] self = -3 -> "FinWait"]

SLoop == /\ pc[M + 1] = "SLoop"
         /\ IF offered < NSubmits
               THEN /\ IF finished \/ aborted \/ stopReq
                          THEN /\ refused' = refused + 1
                               /\ offered' = offered + 1
                               /\ pc' = [pc EXCEPT ![M + 1] = "SLoop"]
                          ELSE /\ pc' = [pc EXCEPT ![M + 1] = "SSpace"]
                               /\ UNCHANGED << refused, offered >>
               ELSE /\ pc' = [pc EXCEPT ![M + 1] = "SDone"]
                    /\ UNCHANGED << refused, offered >>
         /\ UNCHANGED << queue, nextSubmit, delivered, salvagedSeq, discarded, 
                         busy, finished, aborted, stopReq, sdone, wdone, seq >>

SSpace == /\ pc[M + 1] = "SSpace"
          /\ Len(queue) < MaxQ \/ finished \/ aborted \/ stopReq
          /\ IF finished \/ aborted \/ stopReq
                THEN /\ refused' = refused + 1
                     /\ offered' = offered + 1
                     /\ UNCHANGED << queue, nextSubmit >>
                ELSE /\ queue' = Append(queue, nextSubmit)
                     /\ nextSubmit' = nextSubmit + 1
                     /\ offered' = offered + 1
                     /\ UNCHANGED refused
          /\ pc' = [pc EXCEPT ![M + 1] = "SLoop"]
          /\ UNCHANGED << delivered, salvagedSeq, discarded, busy, finished, 
                          aborted, stopReq, sdone, wdone, seq >>

SDone == /\ pc[M + 1] = "SDone"
         /\ sdone' = TRUE
         /\ pc' = [pc EXCEPT ![M + 1] = "Done"]
         /\ UNCHANGED << queue, nextSubmit, delivered, salvagedSeq, refused, 
                         discarded, offered, busy, finished, aborted, stopReq, 
                         wdone, seq >>

submitter == SLoop \/ SSpace \/ SDone

WLoop(self) == /\ pc[self] = "WLoop"
               /\ IF ~wdone[self]
                     THEN /\ pc' = [pc EXCEPT ![self] = "WWait"]
                     ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
               /\ UNCHANGED << queue, nextSubmit, delivered, salvagedSeq, 
                               refused, discarded, offered, busy, finished, 
                               aborted, stopReq, sdone, wdone, seq >>

WWait(self) == /\ pc[self] = "WWait"
               /\ Len(queue) > 0 \/ finished \/ aborted
               /\ IF aborted
                     THEN /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                          /\ pc' = [pc EXCEPT ![self] = "WLoop"]
                          /\ UNCHANGED << queue, busy, seq >>
                     ELSE /\ IF Len(queue) = 0
                                THEN /\ IF finished
                                           THEN /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                                           ELSE /\ TRUE
                                                /\ wdone' = wdone
                                     /\ pc' = [pc EXCEPT ![self] = "WLoop"]
                                     /\ UNCHANGED << queue, busy, seq >>
                                ELSE /\ seq' = [seq EXCEPT ![self] = Head(queue)]
                                     /\ queue' = Tail(queue)
                                     /\ busy' = [busy EXCEPT ![self] = seq'[self]]
                                     /\ pc' = [pc EXCEPT ![self] = "WDeliver"]
                                     /\ wdone' = wdone
               /\ UNCHANGED << nextSubmit, delivered, salvagedSeq, refused, 
                               discarded, offered, finished, aborted, stopReq, 
                               sdone >>

WDeliver(self) == /\ pc[self] = "WDeliver"
                  /\ IF aborted
                        THEN /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                             /\ UNCHANGED << delivered, busy, seq >>
                        ELSE /\ delivered' = Append(delivered, seq[self])
                             /\ busy' = [busy EXCEPT ![self] = NoJob]
                             /\ seq' = [seq EXCEPT ![self] = NoJob]
                             /\ wdone' = wdone
                  /\ pc' = [pc EXCEPT ![self] = "WLoop"]
                  /\ UNCHANGED << queue, nextSubmit, salvagedSeq, refused, 
                                  discarded, offered, finished, aborted, 
                                  stopReq, sdone >>

worker(self) == WLoop(self) \/ WWait(self) \/ WDeliver(self)

StopDo == /\ pc[-1] = "StopDo"
          /\ \/ /\ stopReq' = TRUE
                /\ pc' = [pc EXCEPT ![-1] = "Salv"]
             \/ /\ TRUE
                /\ pc' = [pc EXCEPT ![-1] = "Done"]
                /\ UNCHANGED stopReq
          /\ UNCHANGED << queue, nextSubmit, delivered, salvagedSeq, refused, 
                          discarded, offered, busy, finished, aborted, sdone, 
                          wdone, seq >>

Salv == /\ pc[-1] = "Salv"
        /\ salvagedSeq' = salvagedSeq \o queue
        /\ queue' = <<>>
        /\ pc' = [pc EXCEPT ![-1] = "Done"]
        /\ UNCHANGED << nextSubmit, delivered, refused, discarded, offered, 
                        busy, finished, aborted, stopReq, sdone, wdone, seq >>

stopper == StopDo \/ Salv

AbortDo == /\ pc[-2] = "AbortDo"
           /\ \/ /\ aborted' = TRUE
                 /\ discarded' =   discarded + Len(queue)
                                 + Cardinality({w \in Workers : busy[w] /= NoJob})
                 /\ queue' = <<>>
                 /\ busy' = [w \in Workers |-> NoJob]
              \/ /\ TRUE
                 /\ UNCHANGED <<queue, discarded, busy, aborted>>
           /\ pc' = [pc EXCEPT ![-2] = "Done"]
           /\ UNCHANGED << nextSubmit, delivered, salvagedSeq, refused, 
                           offered, finished, stopReq, sdone, wdone, seq >>

aborter == AbortDo

FinWait == /\ pc[-3] = "FinWait"
           /\ offered = NSubmits
           /\ IF ~aborted
                 THEN /\ finished' = TRUE
                 ELSE /\ TRUE
                      /\ UNCHANGED finished
           /\ pc' = [pc EXCEPT ![-3] = "Done"]
           /\ UNCHANGED << queue, nextSubmit, delivered, salvagedSeq, refused, 
                           discarded, offered, busy, aborted, stopReq, sdone, 
                           wdone, seq >>

finisher == FinWait

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == submitter \/ stopper \/ aborter \/ finisher
           \/ (\E self \in 1..M: worker(self))
           \/ Terminating

Spec == /\ Init /\ [][Next]_vars
        /\ WF_vars(submitter)
        /\ \A self \in 1..M : WF_vars(worker(self))
        /\ WF_vars(stopper)
        /\ WF_vars(finisher)

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION

(***************************************************************************)
(* Checkable properties. Names double as the SPEC IDs quoted in code.     *)
(***************************************************************************)
InflightCount == Cardinality({w \in Workers : busy[w] /= NoJob})
Range(f) == {f[i] : i \in DOMAIN f}

TypeOK ==
  /\ nextSubmit \in Nat
  /\ offered \in Nat /\ refused \in Nat /\ discarded \in Nat
  /\ \A i \in 1..Len(queue) : queue[i] \in Nat
  /\ \A i \in 1..Len(delivered) : delivered[i] \in Nat
  /\ \A i \in 1..Len(salvagedSeq) : salvagedSeq[i] \in Nat
  /\ \A w \in Workers : busy[w] = NoJob \/ busy[w] \in Nat
  /\ finished \in BOOLEAN /\ aborted \in BOOLEAN /\ stopReq \in BOOLEAN

(* SPEC-DIS-1: delivered seqNos are unique and gap-free; delivery order is
   unspecified. SalvageSuffix: everything salvaged lies above everything
   delivered or in flight, so a stop leaves no hole in the delivered
   numbering. GapFree: once the workers are done (finish() joined them)
   without an abort, the delivered numbers are exactly 0 .. n-1. *)
NoDuplicateDelivery ==
  \A i \in 1..Len(delivered) :
    \A j \in 1..Len(delivered) :
      i /= j => delivered[i] /= delivered[j]

SalvageSuffix ==
  \A s \in Range(salvagedSeq) :
    /\ \A d \in Range(delivered) : d < s
    /\ \A w \in Workers : busy[w] /= NoJob => busy[w] < s

GapFree ==
  (~aborted /\ \A w \in Workers : wdone[w]) =>
    Range(delivered) = 0..(Len(delivered) - 1)

(* SPEC-DIS-2: the work queue stays bounded. *)
QueueBounded == Len(queue) <= MaxQ

(* SPEC-DIS-5: no-loss / no-duplication accounting. Every offered assembly
   is either numbered (then delivered, queued, in flight, salvaged, or
   destroyed by abort) or refused into salvage. *)
OfferConservation == offered = nextSubmit + refused
SeqConservation ==
  nextSubmit = Len(delivered) + Len(queue) + InflightCount
             + Len(salvagedSeq) + discarded

(* SPEC-DIS-6: salvage keeps submit order (re-submitted first on the next
   run, so it takes the lowest numbers of that run). *)
SalvageOrdered ==
  \A i \in 1..Len(salvagedSeq) :
    \A j \in 1..Len(salvagedSeq) :
      i < j => salvagedSeq[i] < salvagedSeq[j]

(* SPEC-DIS-7: clean shutdown -- exiting workers hold nothing, and a
   finished run leaves nothing queued. *)
CleanShutdown ==
  /\ \A w \in Workers : wdone[w] => busy[w] = NoJob
  /\ (\A w \in Workers : wdone[w]) => queue = <<>>

(* Liveness: every run ends with submitter and workers done -- via drain,
   via stop-then-drain, or via abort. Fairness comes from the fair
   worker/submitter/finisher processes; the stopper (fair, with a skip
   branch) and the unfair aborter let TLC cover all four stop/abort
   combinations. *)
AllTerminate == <>(sdone /\ (\A w \in Workers : wdone[w]))
=============================================================================
