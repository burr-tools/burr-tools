# Solver rules: thread safety and test invariants

## Thread safety

- BurrTools solvers run worker threads in the background while the FLTK GUI polls metrics on the main thread.
- Any state shared across threads (e.g., `iterations`, `finished`, solutions list) must use `std::atomic` or be guarded with explicit mutex locks (`pr->lockSolutions()`). Never strip concurrency synchronization.

## Test invariants

- Test solver correctness using rotation- and permutation-invariant properties: assembly count, disassemblable solution count, disassembly move levels (`da->movesText()`), and piece placement counts.
- Result order is unspecified (the disassembler pool delivers as its workers finish): compare assemblies and solutions as multisets or counts, never as sequences. Sequence numbers are unique and gap-free, so check them sorted (`0..n-1`), not in delivery order.
- **Do not assert exact iteration counts in regression tests**; iterations measure internal search-tree node visits and will break on valid search heuristic optimizations.
