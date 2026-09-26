---- MODULE AssemblyPool ----------------------------------------------------
(***************************************************************************)
(* Formal protocol model of BurrTools' assembly-side concurrency:          *)
(*                                                                         *)
(*   - ThreadBudget ......... src/lib/thread_budget.h                     *)
(*   - AssemblyTaskPool ..... src/lib/assembler_pool.h                    *)
(*                                                                         *)
(* SCOPE. Only the coordination protocol is modeled: task generation   *)
(* (stoppable, with discard of partial lists), token accounting,        *)
(* pop/finish pairing, quiescence, dynamic splits, requestStop. Task    *)
(* bodies (exact-cover search), voxel caches, the disassembler pool and *)
(* the GUI thread are abstracted away. Tasks are opaque IDs.            *)
(*                                                                         *)
(* NOT MODELED (deliberate gaps, see README.md):                          *)
(*   - C++ locks/mutexes: every region the C++ holds under the pool mutex *)
(*     is one atomic PlusCal step here. The load-bearing C++ rule "every  *)
(*     wait happens lock-free and token-free" (thread_budget.h) is        *)
(*     enforced BY CONSTRUCTION: both awaits sit outside any               *)
(*     token-holding / task-owning region.                                 *)
(*   - C++ memory model: atomics are sequentially consistent here.         *)
(*   - abort()/drain(): only requestStop() (stop flag, queue kept) is     *)
(*     modeled. abort() discards queued work by design; drain() moves it  *)
(*     out for in-session resume. Both are future extensions.              *)
(*   - The null-budget path: BudgetTotal = N behaves like uncapped        *)
(*     (tryAcquire always succeeds), so no second code path is needed.    *)
(*                                                                         *)
(* INVARIANT IDs (shared with code comments, stable across refactors):    *)
(*   SPEC-BUDGET-1 .. token conservation: available + holders = total     *)
(*   SPEC-POOL-1 ..... pop/finish pairing: active = task holders          *)
(*   SPEC-POOL-2 ..... task conservation: no task lost or duplicated      *)
(*   SPEC-POOL-3 ..... clean exit: terminated workers hold nothing        *)
(*   SPEC-POOL-4 ..... stopped generation leaves no partial resumable    *)
(*                     state (PR #118: discard list, mark interrupted)    *)
(***************************************************************************)
EXTENDS Naturals, Integers, Sequences, FiniteSets, TLC

CONSTANTS
  N,            (* worker count (C++: effective thread count, <= MAX_THREADS) *)
  BudgetTotal,  (* ThreadBudget total (C++: pool size; = N means uncapped)    *)
  NumTasks,     (* generation target; pool is seeded 1..NumTasks on success *)
  MaxPush       (* bound on dynamic splits (C++: unbounded; TLC needs a bound)*)

ASSUME N \in Nat /\ N > 0
ASSUME BudgetTotal \in 1..N
ASSUME NumTasks \in Nat /\ NumTasks > 0
ASSUME MaxPush \in Nat

Workers == 1..N

(* --algorithm AssemblyPool {

  variables
    queue = <<>>,                             (* seeded by master on success  *)
    genList = <<>>,                           (* tasks generated so far       *)
    genDone = FALSE,                          (* generation complete          *)
    genStopped = FALSE,                       (* C++: parallelInterrupted     *)
    active = 0,                              (* active_workers, guarded by mtx *)
    stopRequested = FALSE,                   (* C++: atomic stop_requested    *)
    available = BudgetTotal,                 (* ThreadBudget::available_       *)
    holdsToken = [w \in Workers |-> FALSE],  (* thread_local heldBudget ~= self*)
    hasTask = [w \in Workers |-> FALSE],     (* owns one active_workers unit  *)
    terminated = [w \in Workers |-> FALSE],
    completed = 0,
    nextId = NumTasks + 1,                   (* fresh ids for pushed splits   *)
    pushesLeft = MaxPush,
    splitOpen = TRUE;                       (* splitter still willing        *)

  fair process (worker \in 1..N)
  variables task = 0;
  {
    WLoop:
    while (~terminated[self]) {
      \* Pool wait, token-free (assembler_pool.h pop_task, cv.wait + pred).
      \* Progress depends only on token holders finishing or on stop.
      PoolWait:
        \* New work once seeded, quiescence, discarded generation, or stop.
        \* Workers must not proceed on an empty queue before genDone (they
        \* would quiescence-terminate mid-generation); the genDone conjunct
        \* is load-bearing, not cosmetic.
        await (genDone /\ (Len(queue) > 0 \/ active = 0)) \/ genStopped \/ stopRequested;
        if (genStopped \/ stopRequested) {
          terminated[self] := TRUE;
        } else if (Len(queue) = 0) {
          \* Quiescence: an empty queue with no stop implies nobody active.
          assert active = 0;
          terminated[self] := TRUE;
        } else {
          \* Budget gate (assembler_pool.h: reservation inside the pool
          \* lock; thread_budget.h: tryAcquire / blocking acquire).
          if (~holdsToken[self]) {
            if (available > 0) {
              available := available - 1;
              holdsToken[self] := TRUE;
            } else {
              \* Park token-free on the budget CV (C++ unlocks the pool
              \* mutex first, so no lock ordering can arise).
              Park:
                await available > 0 \/ stopRequested;
                if (stopRequested) {
                  terminated[self] := TRUE;
                } else {
                  available := available - 1;
                  holdsToken[self] := TRUE;
                  \* Re-check under the pool lock (assembler_pool.h):
                  \* a sibling may have drained the queue while parked.
                  \* Waiting on an empty queue while holding a token would
                  \* starve token-gated consumers, so return the token and
                  \* wait token-free instead.
                  if (Len(queue) = 0 \/ stopRequested) {
              RecheckRel:
                    available := available + 1;
                    holdsToken[self] := FALSE;
                    if (stopRequested) {
                      terminated[self] := TRUE;
                    } else if (Len(queue) = 0 /\ active = 0) {
                      terminated[self] := TRUE;
                    };
                    \* else: siblings are still active; fall through to Pop
                    \* below, whose guards route back to the token-free
                    \* pool wait (C++: continue).
                  };
                };
            };
          };
          \* Pop + abstract work + finish (pop_task success path,
          \* split via push_tasks, finishTask = releaseBudget + task_done).
          \* Guards make every path sound: Pop fires only holding a token
          \* on a nonempty queue (C++ reserves atomically with that check);
          \* ShedTok drops a reservation that a model-only interleave left
          \* dangling, so the next pool wait is always token-free.
      Pop:
          if (~terminated[self] /\ holdsToken[self] /\ Len(queue) > 0) {
            task := Head(queue);
            queue := Tail(queue);
            active := active + 1;
            hasTask[self] := TRUE;
          };
          Fin:
            \* finishTask pairs with a successful pop ONLY (assembler_pool.h:
            \* "every successful pop_task() MUST be paired with exactly one
            \* finishTask()"). hasTask[self] is that pairing bit: a skipped
            \* pop (queue drained by a sibling across the label split, which
            \* C++ excludes by reserving inside the pool lock) must flow to
            \* ShedTok below, never through task_done accounting.
            if (hasTask[self]) {
              if (holdsToken[self]) {
                available := available + 1;
                holdsToken[self] := FALSE;
              };
              active := active - 1;
              hasTask[self] := FALSE;
              task := 0;
              completed := completed + 1;
              \* Last-active-worker drain broadcast (task_done notify_all)
              \* is implicit: TLC re-evaluates every await predicate.
            };
      ShedTok:
          \* A reservation without a task is impossible in C++ (reserved
          \* atomically with the nonempty check, or shed in the parked
          \* re-check); a label split can strand one here when a sibling
          \* drains the queue between reserve and pop. Shed it so no token
          \* is ever held across the next pool wait or at termination.
          \* Keyed on task ownership, not queue state: a refill between
          \* the skipped pop and this step must not retain the token.
          if (~terminated[self] /\ ~hasTask[self] /\ holdsToken[self]) {
            available := available + 1;
            holdsToken[self] := FALSE;
          };
          \* else: queue refilled after a shed reservation, or nothing to
          \* shed; either way loop back to the token-free pool wait.
        };
    };

  }  \* end worker

  \* Task generation (assembler_0/1 generateTasksAtDepth /
  \* generateSubtreeTasks): the master emits subtasks one per step; workers
  \* start only once the list is complete. A stop mid-generation discards
  \* the partial list and marks interrupted (the PR #118 fix: a save after
  \* this refuses instead of persisting a partial, silently lossy resume
  \* state; in-session continue regenerates, i.e. a fresh spec run). Fair,
  \* so an unstopped generation always completes.
  fair process (master = -1)
  {
  GenLoop:
    while (~genDone /\ ~genStopped) {
      if (stopRequested) {
      GenDrop:
        genList := <<>>;
        genStopped := TRUE;
      } else if (Len(genList) < NumTasks) {
      GenOne:
        genList := Append(genList, Len(genList) + 1);
      } else {
      GenSeed:
        queue := genList;
        genDone := TRUE;
      };
    };
  }

  \* Environment: the GUI/orchestrator may request a stop at any point,
  \* or never. TLC explores both branches: the skip branch exercises the
  \* normal drain-to-quiescence path, the stop branch requestStop().
  process (stopper = 0)
  {
    StopChoice:
      either {
        stopRequested := TRUE;
      } or {
        skip;
      };
  };

  \* Dynamic splits (C++ push_tasks, assembler_pool.h): a worker that sees
  \* starving siblings splits its task and pushes child tasks. Modeled as
  \* environment nondeterminism: the splitter may append fresh tasks while
  \* the search runs, or stop splitting at any point (the pushesLeft := 0
  \* branch). Declining to split is always sound -- the owner then searches
  \* the whole subtree -- and push/pop are separate lock acquisitions in
  \* C++, so the interleave with worker pops is faithful. Unfair: TLC also
  \* covers runs where it never acts. (An earlier revision put the bounded
  \* split counter inside the worker's pop step; TLC caught a TOCTOU on the
  \* bound check. Keeping the counter in one process keeps guard and update
  \* in one atomic step.) Splits only exist once searching has started, so
  \* the splitter waits for seeding (on stopped generation it never acts).
  process (splitter = N + 1)
  {
  SplitStart:
    await genDone \/ genStopped;
    \* Splits only exist once searching has started; on stopped
    \* generation there is nothing to split, so exit.
    if (genDone) {
  SplitLoop:
      while (pushesLeft > 0 /\ splitOpen) {
        either {
          queue := Append(queue, nextId);
          nextId := nextId + 1;
          pushesLeft := pushesLeft - 1;
        } or {
          \* Decline further splits (must not spend the push budget: every
          \* decrement of pushesLeft is exactly one pushed task).
          splitOpen := FALSE;
        };
      };
    };
  };

} *)
\* BEGIN TRANSLATION (chksum(pcal) = "156bac06" /\ chksum(tla) = "74615cd4")
VARIABLES queue, genList, genDone, genStopped, active, stopRequested, 
          available, holdsToken, hasTask, terminated, completed, nextId, 
          pushesLeft, splitOpen, pc, task

vars == << queue, genList, genDone, genStopped, active, stopRequested, 
           available, holdsToken, hasTask, terminated, completed, nextId, 
           pushesLeft, splitOpen, pc, task >>

ProcSet == (1..N) \cup {-1} \cup {0} \cup {N + 1}

Init == (* Global variables *)
        /\ queue = <<>>
        /\ genList = <<>>
        /\ genDone = FALSE
        /\ genStopped = FALSE
        /\ active = 0
        /\ stopRequested = FALSE
        /\ available = BudgetTotal
        /\ holdsToken = [w \in Workers |-> FALSE]
        /\ hasTask = [w \in Workers |-> FALSE]
        /\ terminated = [w \in Workers |-> FALSE]
        /\ completed = 0
        /\ nextId = NumTasks + 1
        /\ pushesLeft = MaxPush
        /\ splitOpen = TRUE
        (* Process worker *)
        /\ task = [self \in 1..N |-> 0]
        /\ pc = [self \in ProcSet |-> CASE self \in 1..N -> "WLoop"
                                        [] self = -1 -> "GenLoop"
                                        [] self = 0 -> "StopChoice"
                                        [] self = N + 1 -> "SplitStart"]

WLoop(self) == /\ pc[self] = "WLoop"
               /\ IF ~terminated[self]
                     THEN /\ pc' = [pc EXCEPT ![self] = "PoolWait"]
                     ELSE /\ pc' = [pc EXCEPT ![self] = "Done"]
               /\ UNCHANGED << queue, genList, genDone, genStopped, active, 
                               stopRequested, available, holdsToken, hasTask, 
                               terminated, completed, nextId, pushesLeft, 
                               splitOpen, task >>

PoolWait(self) == /\ pc[self] = "PoolWait"
                  /\ (genDone /\ (Len(queue) > 0 \/ active = 0)) \/ genStopped \/ stopRequested
                  /\ IF genStopped \/ stopRequested
                        THEN /\ terminated' = [terminated EXCEPT ![self] = TRUE]
                             /\ pc' = [pc EXCEPT ![self] = "WLoop"]
                             /\ UNCHANGED << available, holdsToken >>
                        ELSE /\ IF Len(queue) = 0
                                   THEN /\ Assert(active = 0, 
                                                  "Failure of assertion at line 85, column 11.")
                                        /\ terminated' = [terminated EXCEPT ![self] = TRUE]
                                        /\ pc' = [pc EXCEPT ![self] = "WLoop"]
                                        /\ UNCHANGED << available, holdsToken >>
                                   ELSE /\ IF ~holdsToken[self]
                                              THEN /\ IF available > 0
                                                         THEN /\ available' = available - 1
                                                              /\ holdsToken' = [holdsToken EXCEPT ![self] = TRUE]
                                                              /\ pc' = [pc EXCEPT ![self] = "Pop"]
                                                         ELSE /\ pc' = [pc EXCEPT ![self] = "Park"]
                                                              /\ UNCHANGED << available, 
                                                                              holdsToken >>
                                              ELSE /\ pc' = [pc EXCEPT ![self] = "Pop"]
                                                   /\ UNCHANGED << available, 
                                                                   holdsToken >>
                                        /\ UNCHANGED terminated
                  /\ UNCHANGED << queue, genList, genDone, genStopped, active, 
                                  stopRequested, hasTask, completed, nextId, 
                                  pushesLeft, splitOpen, task >>

Pop(self) == /\ pc[self] = "Pop"
             /\ IF ~terminated[self] /\ holdsToken[self] /\ Len(queue) > 0
                   THEN /\ task' = [task EXCEPT ![self] = Head(queue)]
                        /\ queue' = Tail(queue)
                        /\ active' = active + 1
                        /\ hasTask' = [hasTask EXCEPT ![self] = TRUE]
                   ELSE /\ TRUE
                        /\ UNCHANGED << queue, active, hasTask, task >>
             /\ pc' = [pc EXCEPT ![self] = "Fin"]
             /\ UNCHANGED << genList, genDone, genStopped, stopRequested, 
                             available, holdsToken, terminated, completed, 
                             nextId, pushesLeft, splitOpen >>

Fin(self) == /\ pc[self] = "Fin"
             /\ IF hasTask[self]
                   THEN /\ IF holdsToken[self]
                              THEN /\ available' = available + 1
                                   /\ holdsToken' = [holdsToken EXCEPT ![self] = FALSE]
                              ELSE /\ TRUE
                                   /\ UNCHANGED << available, holdsToken >>
                        /\ active' = active - 1
                        /\ hasTask' = [hasTask EXCEPT ![self] = FALSE]
                        /\ task' = [task EXCEPT ![self] = 0]
                        /\ completed' = completed + 1
                   ELSE /\ TRUE
                        /\ UNCHANGED << active, available, holdsToken, hasTask, 
                                        completed, task >>
             /\ pc' = [pc EXCEPT ![self] = "ShedTok"]
             /\ UNCHANGED << queue, genList, genDone, genStopped, 
                             stopRequested, terminated, nextId, pushesLeft, 
                             splitOpen >>

ShedTok(self) == /\ pc[self] = "ShedTok"
                 /\ IF ~terminated[self] /\ ~hasTask[self] /\ holdsToken[self]
                       THEN /\ available' = available + 1
                            /\ holdsToken' = [holdsToken EXCEPT ![self] = FALSE]
                       ELSE /\ TRUE
                            /\ UNCHANGED << available, holdsToken >>
                 /\ pc' = [pc EXCEPT ![self] = "WLoop"]
                 /\ UNCHANGED << queue, genList, genDone, genStopped, active, 
                                 stopRequested, hasTask, terminated, completed, 
                                 nextId, pushesLeft, splitOpen, task >>

Park(self) == /\ pc[self] = "Park"
              /\ available > 0 \/ stopRequested
              /\ IF stopRequested
                    THEN /\ terminated' = [terminated EXCEPT ![self] = TRUE]
                         /\ pc' = [pc EXCEPT ![self] = "Pop"]
                         /\ UNCHANGED << available, holdsToken >>
                    ELSE /\ available' = available - 1
                         /\ holdsToken' = [holdsToken EXCEPT ![self] = TRUE]
                         /\ IF Len(queue) = 0 \/ stopRequested
                               THEN /\ pc' = [pc EXCEPT ![self] = "RecheckRel"]
                               ELSE /\ pc' = [pc EXCEPT ![self] = "Pop"]
                         /\ UNCHANGED terminated
              /\ UNCHANGED << queue, genList, genDone, genStopped, active, 
                              stopRequested, hasTask, completed, nextId, 
                              pushesLeft, splitOpen, task >>

RecheckRel(self) == /\ pc[self] = "RecheckRel"
                    /\ available' = available + 1
                    /\ holdsToken' = [holdsToken EXCEPT ![self] = FALSE]
                    /\ IF stopRequested
                          THEN /\ terminated' = [terminated EXCEPT ![self] = TRUE]
                          ELSE /\ IF Len(queue) = 0 /\ active = 0
                                     THEN /\ terminated' = [terminated EXCEPT ![self] = TRUE]
                                     ELSE /\ TRUE
                                          /\ UNCHANGED terminated
                    /\ pc' = [pc EXCEPT ![self] = "Pop"]
                    /\ UNCHANGED << queue, genList, genDone, genStopped, 
                                    active, stopRequested, hasTask, completed, 
                                    nextId, pushesLeft, splitOpen, task >>

worker(self) == WLoop(self) \/ PoolWait(self) \/ Pop(self) \/ Fin(self)
                   \/ ShedTok(self) \/ Park(self) \/ RecheckRel(self)

GenLoop == /\ pc[-1] = "GenLoop"
           /\ IF ~genDone /\ ~genStopped
                 THEN /\ IF stopRequested
                            THEN /\ pc' = [pc EXCEPT ![-1] = "GenDrop"]
                            ELSE /\ IF Len(genList) < NumTasks
                                       THEN /\ pc' = [pc EXCEPT ![-1] = "GenOne"]
                                       ELSE /\ pc' = [pc EXCEPT ![-1] = "GenSeed"]
                 ELSE /\ pc' = [pc EXCEPT ![-1] = "Done"]
           /\ UNCHANGED << queue, genList, genDone, genStopped, active, 
                           stopRequested, available, holdsToken, hasTask, 
                           terminated, completed, nextId, pushesLeft, 
                           splitOpen, task >>

GenDrop == /\ pc[-1] = "GenDrop"
           /\ genList' = <<>>
           /\ genStopped' = TRUE
           /\ pc' = [pc EXCEPT ![-1] = "GenLoop"]
           /\ UNCHANGED << queue, genDone, active, stopRequested, available, 
                           holdsToken, hasTask, terminated, completed, nextId, 
                           pushesLeft, splitOpen, task >>

GenOne == /\ pc[-1] = "GenOne"
          /\ genList' = Append(genList, Len(genList) + 1)
          /\ pc' = [pc EXCEPT ![-1] = "GenLoop"]
          /\ UNCHANGED << queue, genDone, genStopped, active, stopRequested, 
                          available, holdsToken, hasTask, terminated, 
                          completed, nextId, pushesLeft, splitOpen, task >>

GenSeed == /\ pc[-1] = "GenSeed"
           /\ queue' = genList
           /\ genDone' = TRUE
           /\ pc' = [pc EXCEPT ![-1] = "GenLoop"]
           /\ UNCHANGED << genList, genStopped, active, stopRequested, 
                           available, holdsToken, hasTask, terminated, 
                           completed, nextId, pushesLeft, splitOpen, task >>

master == GenLoop \/ GenDrop \/ GenOne \/ GenSeed

StopChoice == /\ pc[0] = "StopChoice"
              /\ \/ /\ stopRequested' = TRUE
                 \/ /\ TRUE
                    /\ UNCHANGED stopRequested
              /\ pc' = [pc EXCEPT ![0] = "Done"]
              /\ UNCHANGED << queue, genList, genDone, genStopped, active, 
                              available, holdsToken, hasTask, terminated, 
                              completed, nextId, pushesLeft, splitOpen, task >>

stopper == StopChoice

SplitStart == /\ pc[N + 1] = "SplitStart"
              /\ genDone \/ genStopped
              /\ IF genDone
                    THEN /\ pc' = [pc EXCEPT ![N + 1] = "SplitLoop"]
                    ELSE /\ pc' = [pc EXCEPT ![N + 1] = "Done"]
              /\ UNCHANGED << queue, genList, genDone, genStopped, active, 
                              stopRequested, available, holdsToken, hasTask, 
                              terminated, completed, nextId, pushesLeft, 
                              splitOpen, task >>

SplitLoop == /\ pc[N + 1] = "SplitLoop"
             /\ IF pushesLeft > 0 /\ splitOpen
                   THEN /\ \/ /\ queue' = Append(queue, nextId)
                              /\ nextId' = nextId + 1
                              /\ pushesLeft' = pushesLeft - 1
                              /\ UNCHANGED splitOpen
                           \/ /\ splitOpen' = FALSE
                              /\ UNCHANGED <<queue, nextId, pushesLeft>>
                        /\ pc' = [pc EXCEPT ![N + 1] = "SplitLoop"]
                   ELSE /\ pc' = [pc EXCEPT ![N + 1] = "Done"]
                        /\ UNCHANGED << queue, nextId, pushesLeft, splitOpen >>
             /\ UNCHANGED << genList, genDone, genStopped, active, 
                             stopRequested, available, holdsToken, hasTask, 
                             terminated, completed, task >>

splitter == SplitStart \/ SplitLoop

(* Allow infinite stuttering to prevent deadlock on termination. *)
Terminating == /\ \A self \in ProcSet: pc[self] = "Done"
               /\ UNCHANGED vars

Next == master \/ stopper \/ splitter
           \/ (\E self \in 1..N: worker(self))
           \/ Terminating

Spec == /\ Init /\ [][Next]_vars
        /\ \A self \in 1..N : WF_vars(worker(self))
        /\ WF_vars(master)

Termination == <>(\A self \in ProcSet: pc[self] = "Done")

\* END TRANSLATION

(***************************************************************************)
(* Checkable properties. Names double as the SPEC IDs quoted in code.     *)
(***************************************************************************)
TokenHolders == {w \in Workers : holdsToken[w]}
TaskHolders == {w \in Workers : hasTask[w]}
(* Pool inventory: seeded tasks (all NumTasks at once, or none) plus splits.
   Generation keeps unseeded tasks in genList, outside this equation. *)
PoolSpawned == (IF genDone THEN NumTasks ELSE 0) + (MaxPush - pushesLeft)

TypeOK ==
  /\ queue \in Seq(Nat)
  /\ genList \in Seq(Nat)
  /\ genDone \in BOOLEAN /\ genStopped \in BOOLEAN
  /\ active \in 0..N
  /\ available \in 0..BudgetTotal
  /\ completed \in Nat
  /\ nextId \in Nat
  /\ pushesLeft \in 0..MaxPush
  /\ splitOpen \in BOOLEAN

(* SPEC-BUDGET-1: token conservation (thread_budget.h take/return pair up) *)
BudgetConservation == available + Cardinality(TokenHolders) = BudgetTotal

(* N-active-thread bound (design/2026-09-21-cpp20-concurrency.md sec. 1):
   every in-flight task's thread holds a token, so working threads <= total *)
ActiveBounded == active <= Cardinality(TokenHolders)

(* SPEC-POOL-1: pop/finish pairing (assembler_pool.h: every successful
   pop_task is paired with exactly one finishTask) *)
Pairing == active = Cardinality(TaskHolders)

(* SPEC-POOL-2: no task lost or duplicated, incl. across dynamic splits *)
TaskConservation == completed + active + Len(queue) = PoolSpawned

(* Generation shape: tasks are 1..k in order; a complete list is 1..NumTasks *)
GenShape == genList = [i \in 1..Len(genList) |-> i]

(* SPEC-POOL-4 (PR #118): a stop mid-generation discards the partial list
   and marks interrupted -- no partial state may look resumable. Since
   workers only start from a complete list, nothing was ever searched. *)
NoPartialResume ==
  genStopped => /\ ~genDone
                /\ Len(genList) = 0
                /\ Len(queue) = 0
                /\ active = 0
                /\ completed = 0

(* SPEC-POOL-3: quiescence and stop exits leak neither tasks nor tokens *)
CleanExit == \A w \in Workers : terminated[w] => (~hasTask[w] /\ ~holdsToken[w])

(* Liveness: the search always ends, via drain-to-quiescence or via stop.
   Fairness comes from `fair process worker`; the stopper is unfair, so TLC
   covers both the never-stops and the stops-eventually behaviors. *)
AllTerminate == <>(\A w \in Workers : terminated[w])
=============================================================================
