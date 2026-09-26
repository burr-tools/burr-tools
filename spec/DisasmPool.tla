---- MODULE DisasmPool -------------------------------------------------------
(***************************************************************************)
(* Formal protocol model of BurrTools' disassembly side:                  *)
(*                                                                         *)
(*   - disassemblerPool_c ... src/lib/disassemblerpool.{h,cpp}            *)
(*                                                                         *)
(* SCOPE. The pool protocol only: bounded work queue, worker pickup,      *)
(* bounded reorder buffer, merger's seqNo-ordered re-emit, and the        *)
(* finish / abort / requestStop lifecycle with salvage + dropped-skip.    *)
(* Assemblies and separations are opaque; disassembly itself is one       *)
(* atomic step. The merger callback (on_result into solveThread_c) is     *)
(* abstracted as appending to `delivered`.                                *)
(*                                                                         *)
(* NOT MODELED (deliberate gaps, see README.md):                          *)
(*   - Inline mode (num_threads == 1 / BURRTOOLS_NO_DISASM_POOL):          *)
(*     sequential under inline_mutex, ordered by construction.             *)
(*   - Shared ThreadBudget contention: workers process one job at a time  *)
(*     (capacity M by construction). Tying the budget to the assembly     *)
(*     side is the Pipeline composition spec's job.                       *)
(*   - Exceptions: worker_exception/abort-on-throw is covered as an       *)
(*     arbitrary-time abort() (same observable protocol).                 *)
(*   - C++ locks/mutexes and the C++ memory model (as in AssemblyPool).   *)
(*   - Payload content: placementCount()==1 shortcuts, movement analysis. *)
(*                                                                         *)
(* INVARIANT IDs (shared with code comments, stable across refactors):    *)
(*   SPEC-DIS-1 ...... merger delivers strictly in submit order           *)
(*   SPEC-DIS-2 ...... work queue stays bounded                           *)
(*   SPEC-DIS-3 ...... reorder buffer stays bounded                       *)
(*   SPEC-DIS-4 ...... submit window bounds outstanding work (this is what*)
(*                     guarantees a filing worker always finds a slot)    *)
(*   SPEC-DIS-5 ...... every offered assembly is delivered, queued, in   *)
(*                     flight, buffered, salvaged, or discarded -- never  *)
(*                     lost, never duplicated                             *)
(*   SPEC-DIS-6 ...... salvaged assemblies keep submit order              *)
(*   SPEC-DIS-7 ...... clean shutdown: no stranded jobs or results        *)
(***************************************************************************)
EXTENDS Naturals, Integers, Sequences, FiniteSets, TLC

CONSTANTS
  M,          (* disassembler worker count (C++: num_threads)              *)
  NSubmits,   (* assemblies the submitter offers (C++: unbounded stream)  *)
  MaxQ,       (* work queue bound (C++: max_queue_size, 64)                *)
  MaxR        (* reorder window bound (C++: max_reorder_size, 64)          *)

ASSUME M \in Nat /\ M > 0
ASSUME NSubmits \in Nat /\ NSubmits > 0
ASSUME MaxQ \in Nat /\ MaxQ > 0
ASSUME MaxR \in Nat /\ MaxR > 0

Workers == 1..M
NoJob == -1  (* busy[w] = -1 means worker w holds no job; seqNos start at 0 *)

(* --algorithm DisasmPool {

  variables
    queue = <<>>,                        (* work_queue (seqNos, FIFO)       *)
    nextSubmit = 0,                      (* next_submit_seq                 *)
    nextMerge = 0,                       (* next_merge_seq                  *)
    filed = {},                          (* reorder_buffer keys (seqNos)    *)
    dropped = {},                        (* salvaged seqNos to skip         *)
    delivered = <<>>,                    (* on_result calls, in order       *)
    salvagedSeq = <<>>,                  (* numbered salvage, submit order  *)
    refused = 0,                         (* terminal-time refuses (no seq)  *)
    skipped = 0,                         (* merger skips over dropped_      *)
    discarded = 0,                       (* jobs destroyed by abort()       *)
    offered = 0,                         (* assemblies offered so far       *)
    lastDelivered = -1,                  (* SPEC-DIS-1 witness              *)
    lastSalvaged = -1,                   (* SPEC-DIS-6 witness              *)
    busy = [w \in Workers |-> NoJob],    (* checked-out job per worker      *)
    finished = FALSE,
    aborted = FALSE,
    stopReq = FALSE,                     (* stop_requested                  *)
    sdone = FALSE,
    wdone = [w \in Workers |-> FALSE],
    mdone = FALSE;

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
        \* no seqNo assigned, merger never expects it).
        refused := refused + 1;
        offered := offered + 1;
      } else {
      SSpace:
        await (Len(queue) < MaxQ /\ nextSubmit - nextMerge < MaxR)
           \/ finished \/ aborted \/ stopReq;
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
        \* Abstract disassembly. C++ files the result even for stopped or
        \* trivial assemblies; the budget gate is uncapped in this spec
        \* (see header: shared-budget contention is the Pipeline spec).
        \* Reorder backpressure (C++ cv_reorder wait): TLC proves WFile
        \* never actually blocks -- SPEC-DIS-4 guarantees a free slot
        \* whenever a job is in flight.
      WFile:
        await Cardinality(filed) < MaxR \/ aborted;
        if (aborted) {
          \* C++ returns here WITHOUT filing (token already returned);
          \* the aborter owns clearing busy[self].
          wdone[self] := TRUE;
        } else {
          filed := filed \cup {seq};
          busy[self] := NoJob;
          seq := NoJob;
        };
      };
    };
  }

  fair process (merger = 0)
  {
  MLoop:
    while (~mdone) {
    MWait:
      await (nextMerge \in filed) \/ (nextMerge \in dropped) \/ aborted
         \/ (finished /\ nextMerge = nextSubmit);
      if (aborted) {
        mdone := TRUE;
      } else if (nextMerge \in dropped) {
        \* Salvaged for the next run: skip without a callback
        \* (disassemblerpool.cpp merger_loop).
        dropped := dropped \ {nextMerge};
        skipped := skipped + 1;
        nextMerge := nextMerge + 1;
      } else if (nextMerge \in filed) {
        assert nextMerge > lastDelivered;  (* SPEC-DIS-1 *)
        filed := filed \ {nextMerge};
        delivered := Append(delivered, nextMerge);
        lastDelivered := nextMerge;
        nextMerge := nextMerge + 1;
      } else {
        \* Nothing filed, nothing dropped, nothing outstanding.
        assert finished /\ nextMerge = nextSubmit;
        mdone := TRUE;
      };
    };
  }

  \* GUI/orchestrator: requestStop() at an arbitrary point. Salvages the
  \* queued (never-started) assemblies under one atomic step -- in C++ this
  \* holds queue_mutex across the whole drain; the per-step interleave with
  \* worker pops below is sound (a popped-then-delivered task equals the
  \* pop-just-before-stop linearisation, and a task is never both popped
  \* and salvaged). Unfair: TLC also covers never-stopping.
  process (stopper = -1)
  {
  StopDo:
    either {
      stopReq := TRUE;
    SalvLoop:
      while (Len(queue) > 0) {
        assert Head(queue) > lastSalvaged;  (* SPEC-DIS-6 *)
        salvagedSeq := Append(salvagedSeq, Head(queue));
        lastSalvaged := Head(queue);
        dropped := dropped \cup {Head(queue)};
        queue := Tail(queue);
      };
    } or {
      skip;
    };
  }

  \* Emergency teardown (C++ abort(): destructor, worker_exception path).
  \* Discards everything in one atomic step; workers/merger exit on the
  \* flag. Unfair: arbitrary-time abort, or never.
  process (aborter = -2)
  {
  AbortDo:
    either {
      aborted := TRUE;
      discarded := discarded + Len(queue)
                 + Cardinality(filed) + Cardinality(dropped)
                 + Cardinality({w \in Workers : busy[w] /= NoJob});
      queue := <<>>;
      filed := {};
      dropped := {};
      busy := [w \in Workers |-> NoJob];
    } or {
      skip;
    };
  }

  \* Normal teardown (C++ finish()): once every offer resolved. Fair, so a
  \* never-stop-never-abort run still ends. Harmless after abort (workers
  \* and merger already exit on the flag).
  fair process (finisher = -3)
  {
  FinWait:
    await offered = NSubmits;
    if (~aborted) {
      finished := TRUE;
    };
  }

} *)
\* BEGIN TRANSLATION (chksum(pcal) = "53b839fb" /\ chksum(tla) = "b6f58725")
VARIABLES queue, nextSubmit, nextMerge, filed, dropped, delivered, 
          salvagedSeq, refused, skipped, discarded, offered, lastDelivered, 
          lastSalvaged, busy, finished, aborted, stopReq, sdone, wdone, mdone, 
          pc, seq

vars == << queue, nextSubmit, nextMerge, filed, dropped, delivered, 
           salvagedSeq, refused, skipped, discarded, offered, lastDelivered, 
           lastSalvaged, busy, finished, aborted, stopReq, sdone, wdone, 
           mdone, pc, seq >>

ProcSet == {M + 1} \cup (1..M) \cup {0} \cup {-1} \cup {-2} \cup {-3}

Init == (* Global variables *)
        /\ queue = <<>>
        /\ nextSubmit = 0
        /\ nextMerge = 0
        /\ filed = {}
        /\ dropped = {}
        /\ delivered = <<>>
        /\ salvagedSeq = <<>>
        /\ refused = 0
        /\ skipped = 0
        /\ discarded = 0
        /\ offered = 0
        /\ lastDelivered = -1
        /\ lastSalvaged = -1
        /\ busy = [w \in Workers |-> NoJob]
        /\ finished = FALSE
        /\ aborted = FALSE
        /\ stopReq = FALSE
        /\ sdone = FALSE
        /\ wdone = [w \in Workers |-> FALSE]
        /\ mdone = FALSE
        (* Process worker *)
        /\ seq = [self \in 1..M |-> NoJob]
        /\ pc = [self \in ProcSet |-> CASE self = M + 1 -> "SLoop"
                                        [] self \in 1..M -> "WLoop"
                                        [] self = 0 -> "MLoop"
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
         /\ UNCHANGED << queue, nextSubmit, nextMerge, filed, dropped, 
                         delivered, salvagedSeq, skipped, discarded, 
                         lastDelivered, lastSalvaged, busy, finished, aborted, 
                         stopReq, sdone, wdone, mdone, seq >>

SSpace == /\ pc[M + 1] = "SSpace"
          /\    (Len(queue) < MaxQ /\ nextSubmit - nextMerge < MaxR)
             \/ finished \/ aborted \/ stopReq
          /\ IF finished \/ aborted \/ stopReq
                THEN /\ refused' = refused + 1
                     /\ offered' = offered + 1
                     /\ UNCHANGED << queue, nextSubmit >>
                ELSE /\ queue' = Append(queue, nextSubmit)
                     /\ nextSubmit' = nextSubmit + 1
                     /\ offered' = offered + 1
                     /\ UNCHANGED refused
          /\ pc' = [pc EXCEPT ![M + 1] = "SLoop"]
          /\ UNCHANGED << nextMerge, filed, dropped, delivered, salvagedSeq, 
                          skipped, discarded, lastDelivered, lastSalvaged, 
                          busy, finished, aborted, stopReq, sdone, wdone, 
                          mdone, seq >>

SDone == /\ pc[M + 1] = "SDone"
         /\ sdone' = TRUE
         /\ pc' = [pc EXCEPT ![M + 1] = "Done"]
         /\ UNCHANGED << queue, nextSubmit, nextMerge, filed, dropped, 
                         delivered, salvagedSeq, refused, skipped, discarded, 
                         offered, lastDelivered, lastSalvaged, busy, finished, 
                         aborted, stopReq, wdone, mdone, seq >>

submitter == SLoop \/ SSpace \/ SDone

WLoop(self) == /\ pc[self] = "WLoop"
               /\ IF ~wdone[self]
                     THEN /\ pc' = [pc EXCEPT ![self] = "WWait"]
                     ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
               /\ UNCHANGED << queue, nextSubmit, nextMerge, filed, dropped, 
                               delivered, salvagedSeq, refused, skipped, 
                               discarded, offered, lastDelivered, lastSalvaged, 
                               busy, finished, aborted, stopReq, sdone, wdone, 
                               mdone, seq >>

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
                                     /\ pc' = [pc EXCEPT ![self] = "WFile"]
                                     /\ wdone' = wdone
               /\ UNCHANGED << nextSubmit, nextMerge, filed, dropped, 
                               delivered, salvagedSeq, refused, skipped, 
                               discarded, offered, lastDelivered, lastSalvaged, 
                               finished, aborted, stopReq, sdone, mdone >>

WFile(self) == /\ pc[self] = "WFile"
               /\ Cardinality(filed) < MaxR \/ aborted
               /\ IF aborted
                     THEN /\ wdone' = [wdone EXCEPT ![self] = TRUE]
                          /\ UNCHANGED << filed, busy, seq >>
                     ELSE /\ filed' = (filed \cup {seq[self]})
                          /\ busy' = [busy EXCEPT ![self] = NoJob]
                          /\ seq' = [seq EXCEPT ![self] = NoJob]
                          /\ wdone' = wdone
               /\ pc' = [pc EXCEPT ![self] = "WLoop"]
               /\ UNCHANGED << queue, nextSubmit, nextMerge, dropped, 
                               delivered, salvagedSeq, refused, skipped, 
                               discarded, offered, lastDelivered, lastSalvaged, 
                               finished, aborted, stopReq, sdone, mdone >>

worker(self) == WLoop(self) \/ WWait(self) \/ WFile(self)

MLoop == /\ pc[0] = "MLoop"
         /\ IF ~mdone
               THEN /\ pc' = [pc EXCEPT ![0] = "MWait"]
               ELSE /\ pc' = [pc EXCEPT ![0] = "Done"]
         /\ UNCHANGED << queue, nextSubmit, nextMerge, filed, dropped, 
                         delivered, salvagedSeq, refused, skipped, discarded, 
                         offered, lastDelivered, lastSalvaged, busy, finished, 
                         aborted, stopReq, sdone, wdone, mdone, seq >>

MWait == /\ pc[0] = "MWait"
         /\    (nextMerge \in filed) \/ (nextMerge \in dropped) \/ aborted
            \/ (finished /\ nextMerge = nextSubmit)
         /\ IF aborted
               THEN /\ mdone' = TRUE
                    /\ UNCHANGED << nextMerge, filed, dropped, delivered, 
                                    skipped, lastDelivered >>
               ELSE /\ IF nextMerge \in dropped
                          THEN /\ dropped' = dropped \ {nextMerge}
                               /\ skipped' = skipped + 1
                               /\ nextMerge' = nextMerge + 1
                               /\ UNCHANGED << filed, delivered, lastDelivered, 
                                               mdone >>
                          ELSE /\ IF nextMerge \in filed
                                     THEN /\ Assert(nextMerge > lastDelivered, 
                                                    "Failure of assertion at line 166, column 9.")
                                          /\ filed' = filed \ {nextMerge}
                                          /\ delivered' = Append(delivered, nextMerge)
                                          /\ lastDelivered' = nextMerge
                                          /\ nextMerge' = nextMerge + 1
                                          /\ mdone' = mdone
                                     ELSE /\ Assert(finished /\ nextMerge = nextSubmit, 
                                                    "Failure of assertion at line 173, column 9.")
                                          /\ mdone' = TRUE
                                          /\ UNCHANGED << nextMerge, filed, 
                                                          delivered, 
                                                          lastDelivered >>
                               /\ UNCHANGED << dropped, skipped >>
         /\ pc' = [pc EXCEPT ![0] = "MLoop"]
         /\ UNCHANGED << queue, nextSubmit, salvagedSeq, refused, discarded, 
                         offered, lastSalvaged, busy, finished, aborted, 
                         stopReq, sdone, wdone, seq >>

merger == MLoop \/ MWait

StopDo == /\ pc[-1] = "StopDo"
          /\ \/ /\ stopReq' = TRUE
                /\ pc' = [pc EXCEPT ![-1] = "SalvLoop"]
             \/ /\ TRUE
                /\ pc' = [pc EXCEPT ![-1] = "Done"]
                /\ UNCHANGED stopReq
          /\ UNCHANGED << queue, nextSubmit, nextMerge, filed, dropped, 
                          delivered, salvagedSeq, refused, skipped, discarded, 
                          offered, lastDelivered, lastSalvaged, busy, finished, 
                          aborted, sdone, wdone, mdone, seq >>

SalvLoop == /\ pc[-1] = "SalvLoop"
            /\ IF Len(queue) > 0
                  THEN /\ Assert(Head(queue) > lastSalvaged, 
                                 "Failure of assertion at line 192, column 9.")
                       /\ salvagedSeq' = Append(salvagedSeq, Head(queue))
                       /\ lastSalvaged' = Head(queue)
                       /\ dropped' = (dropped \cup {Head(queue)})
                       /\ queue' = Tail(queue)
                       /\ pc' = [pc EXCEPT ![-1] = "SalvLoop"]
                  ELSE /\ pc' = [pc EXCEPT ![-1] = "Done"]
                       /\ UNCHANGED << queue, dropped, salvagedSeq, 
                                       lastSalvaged >>
            /\ UNCHANGED << nextSubmit, nextMerge, filed, delivered, refused, 
                            skipped, discarded, offered, lastDelivered, busy, 
                            finished, aborted, stopReq, sdone, wdone, mdone, 
                            seq >>

stopper == StopDo \/ SalvLoop

AbortDo == /\ pc[-2] = "AbortDo"
           /\ \/ /\ aborted' = TRUE
                 /\ discarded' =   discarded + Len(queue)
                                 + Cardinality(filed) + Cardinality(dropped)
                                 + Cardinality({w \in Workers : busy[w] /= NoJob})
                 /\ queue' = <<>>
                 /\ filed' = {}
                 /\ dropped' = {}
                 /\ busy' = [w \in Workers |-> NoJob]
              \/ /\ TRUE
                 /\ UNCHANGED <<queue, filed, dropped, discarded, busy, aborted>>
           /\ pc' = [pc EXCEPT ![-2] = "Done"]
           /\ UNCHANGED << nextSubmit, nextMerge, delivered, salvagedSeq, 
                           refused, skipped, offered, lastDelivered, 
                           lastSalvaged, finished, stopReq, sdone, wdone, 
                           mdone, seq >>

aborter == AbortDo

FinWait == /\ pc[-3] = "FinWait"
           /\ offered = NSubmits
           /\ IF ~aborted
                 THEN /\ finished' = TRUE
                 ELSE /\ TRUE
                      /\ UNCHANGED finished
           /\ pc' = [pc EXCEPT ![-3] = "Done"]
           /\ UNCHANGED << queue, nextSubmit, nextMerge, filed, dropped, 
                           delivered, salvagedSeq, refused, skipped, discarded, 
                           offered, lastDelivered, lastSalvaged, busy, aborted, 
                           stopReq, sdone, wdone, mdone, seq >>

finisher == FinWait

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == submitter \/ merger \/ stopper \/ aborter \/ finisher
           \/ (\E self \in 1..M: worker(self))
           \/ Terminating

Spec == /\ Init /\ [][Next]_vars
        /\ WF_vars(submitter)
        /\ \A self \in 1..M : WF_vars(worker(self))
        /\ WF_vars(merger)
        /\ WF_vars(finisher)

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION

(***************************************************************************)
(* Checkable properties. Names double as the SPEC IDs quoted in code.     *)
(***************************************************************************)
InflightCount == Cardinality({w \in Workers : busy[w] /= NoJob})

TypeOK ==
  /\ nextSubmit \in Nat /\ nextMerge \in Nat
  /\ offered \in Nat /\ refused \in Nat /\ skipped \in Nat /\ discarded \in Nat
  /\ lastDelivered \in Int /\ lastSalvaged \in Int
  /\ \A i \in 1..Len(queue) : queue[i] \in Nat
  /\ filed \subseteq Nat /\ dropped \subseteq Nat
  /\ \A i \in 1..Len(delivered) : delivered[i] \in Nat
  /\ \A i \in 1..Len(salvagedSeq) : salvagedSeq[i] \in Nat
  /\ \A w \in Workers : busy[w] = NoJob \/ busy[w] \in Nat
  /\ finished \in BOOLEAN /\ aborted \in BOOLEAN /\ stopReq \in BOOLEAN

(* SPEC-DIS-1: ordered delivery. The per-delivery assert
   (nextMerge > lastDelivered) carries the proof; this states the shape. *)
OrderedDelivery ==
  \A i \in 1..Len(delivered) :
    \A j \in 1..Len(delivered) :
      i < j => delivered[i] < delivered[j]

(* SPEC-DIS-2/3: both buffers stay bounded. *)
QueueBounded == Len(queue) <= MaxQ
ReorderBounded == Cardinality(filed) <= MaxR

(* SPEC-DIS-4: the submit window bounds outstanding work. Load-bearing:
   with at most MaxR seqNos outstanding, a filing worker always finds a
   slot, so reorder backpressure can never stall the pipeline. *)
WindowBounded == nextSubmit - nextMerge <= MaxR

(* SPEC-DIS-5: no-loss / no-duplication accounting. Every offered assembly
   is either numbered (then delivered, queued, in flight, buffered, queued
   for skip, skipped, or destroyed by abort) or refused into salvage. *)
OfferConservation == offered = nextSubmit + refused
SeqConservation ==
  nextSubmit = Len(delivered) + Len(queue) + InflightCount
             + Cardinality(filed) + Cardinality(dropped) + skipped + discarded

(* SPEC-DIS-6: salvage keeps submit order (needed for deterministic
   re-submission on the next run). The per-salvage assert carries it. *)
SalvageOrdered ==
  \A i \in 1..Len(salvagedSeq) :
    \A j \in 1..Len(salvagedSeq) :
      i < j => salvagedSeq[i] < salvagedSeq[j]

(* SPEC-DIS-7: clean shutdown -- exiting workers hold nothing, and a
   finished merger leaves no stranded results or skips. *)
CleanShutdown ==
  /\ \A w \in Workers : wdone[w] => busy[w] = NoJob
  /\ mdone => filed = {} /\ dropped = {}

(* Liveness: every run ends with submitter, workers and merger done --
   via drain, via stop-then-drain, or via abort. Fairness comes from the
   fair worker/merger/submitter/finisher processes; stopper and aborter
   are unfair so TLC covers all four combinations. *)
AllTerminate == <>(sdone /\ (\A w \in Workers : wdone[w]) /\ mdone) 
=============================================================================
