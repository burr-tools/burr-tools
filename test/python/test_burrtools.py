import unittest
import os
import burrtools

class TestBurrTools(unittest.TestCase):

    def setUp(self):
        self.puzzle_path = os.path.join(
            os.path.dirname(__file__), "..", "..", "examples", "PelikanBurr.xmpuzzle"
        )
        self.assertTrue(os.path.exists(self.puzzle_path), f"File not found: {self.puzzle_path}")

    def test_load_puzzle(self):
        puzzle = burrtools.load(self.puzzle_path)
        self.assertGreater(puzzle.num_shapes, 0)
        self.assertEqual(puzzle.num_problems, 1)
        self.assertEqual(len(puzzle.problems), 1)

        prob = puzzle.problems[0]
        self.assertEqual(prob.index, 0)
        self.assertGreater(prob.num_pieces, 0)

    def test_solve_with_disassembly_iterator(self):
        puzzle = burrtools.load(self.puzzle_path)
        prob = puzzle.problems[0]

        solutions = []
        for sol in prob.solve(disassemble=True):
            solutions.append(sol)

        self.assertEqual(len(solutions), 1)
        sol = solutions[0]
        self.assertTrue(sol.has_disassembly)
        self.assertEqual(sol.moves_text, "98.2.4.2")
        self.assertEqual(sol.level, 98)
        self.assertGreater(sol.total_moves, 0)
        self.assertEqual(len(sol.placements), 7)

    def test_solve_assemblies_only(self):
        puzzle = burrtools.load(self.puzzle_path)
        prob = puzzle.problems[0]

        assemblies = list(prob.solve(disassemble=False))
        self.assertEqual(len(assemblies), 12)
        for asm in assemblies:
            self.assertFalse(asm.has_disassembly)
            self.assertEqual(len(asm.placements), 7)

    def test_early_termination(self):
        puzzle = burrtools.load(self.puzzle_path)
        prob = puzzle.problems[0]

        collected = []
        for sol in prob.solve(disassemble=False):
            collected.append(sol)
            if len(collected) == 3:
                break

        self.assertEqual(len(collected), 3)

    def test_nonexistent_file(self):
        with self.assertRaises(RuntimeError):
            burrtools.load("does_not_exist_xyz.xmpuzzle")

    def test_create_puzzle_programmatically(self):
        import tempfile
        puzzle = burrtools.Puzzle()
        puzzle.comment = "Programmatic 2x2x2 puzzle"
        self.assertEqual(puzzle.comment, "Programmatic 2x2x2 puzzle")

        # Add target shape (2x2x2 filled cube)
        target = puzzle.add_shape(2, 2, 2, name="cube")
        target.fill([(x, y, z) for x in range(2) for y in range(2) for z in range(2)])
        self.assertEqual(target.count_filled(), 8)
        self.assertEqual(target.name, "cube")
        self.assertEqual(target.dimensions, (2, 2, 2))

        # Add piece shape (1x2x2 slab)
        piece = puzzle.add_shape(1, 2, 2, name="slab")
        piece.fill([(0, y, z) for y in range(2) for z in range(2)])
        self.assertEqual(piece.count_filled(), 4)
        self.assertEqual(piece.name, "slab")

        # Add problem
        problem = puzzle.add_problem(name="assemble 2x2x2")
        self.assertEqual(problem.name, "assemble 2x2x2")
        problem.set_result(target)
        self.assertEqual(problem.result_shape_index, target.index)
        problem.set_piece_count(piece, 2)
        self.assertEqual(problem.get_piece_min(piece.index), 2)
        self.assertEqual(problem.get_piece_max(piece.index), 2)

        # Solve in-memory
        solutions = list(problem.solve(disassemble=False))
        self.assertGreater(len(solutions), 0)
        for sol in solutions:
            self.assertEqual(len(sol.placements), 2)
            for p in sol.placements:
                self.assertTrue(p.is_placed)

        # Save and reload
        with tempfile.NamedTemporaryFile(suffix=".xmpuzzle", delete=False) as f:
            tmp_path = f.name
        try:
            puzzle.save(tmp_path)
            loaded = burrtools.load(tmp_path)
            self.assertEqual(loaded.comment, "Programmatic 2x2x2 puzzle")
            self.assertEqual(loaded.num_shapes, 2)
            self.assertEqual(loaded.num_problems, 1)
            loaded_prob = loaded.problems[0]
            self.assertEqual(loaded_prob.name, "assemble 2x2x2")
            loaded_solutions = list(loaded_prob.solve(disassemble=False))
            self.assertEqual(len(loaded_solutions), len(solutions))
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)

    def test_immediate_stop(self):
        """Stopping ends the search promptly and the iterator terminates.

        This used to assert that next() raises StopIteration immediately,
        which is racy and failed intermittently on CI. The worker thread
        starts solving when the iterator is CONSTRUCTED, not when it is
        first consumed, and stop() does not discard what the worker has
        already queued. So whether anything comes back depends purely on
        which thread wins -- measured locally, inserting a 1ms delay before
        stop() flipped it from 0/100 results to 99/100.

        Returning already-found results is the defensible behaviour: the
        search was stopped, but what it had produced is not thrown away.
        So the guarantee worth asserting is not "nothing comes back", it is
        that the search really stopped -- the iterator terminates, and it
        does not run to completion.

        That assertion is safe whatever the timing, because the worker's
        queue is bounded (MAX_QUEUE_SIZE = 4) and it blocks once full. With
        no consumer running it therefore cannot get past four results, and
        this fixture has twelve. Measured across delays from 0 to 500ms
        before stop(), the count after stopping is 0 or 4 and never 12.
        """
        puzzle = burrtools.load(self.puzzle_path)
        prob = puzzle.problems[0]

        # how many assemblies a complete run of this problem yields
        total = len(list(prob.solve(disassemble=False)))
        self.assertGreater(total, 1, "fixture must have room to stop early")

        it = prob.solve(disassemble=False)
        it.stop()

        # terminates rather than hanging, and yields at most what was
        # already queued -- never the whole search
        produced = list(it)
        self.assertLess(len(produced), total)

        # and it stays exhausted
        with self.assertRaises(StopIteration):
            next(it)

    def test_iterator_iterations_live_and_finished(self):
        puzzle = burrtools.load(self.puzzle_path)
        prob = puzzle.problems[0]
        it = prob.solve(disassemble=False)
        sol = next(it)
        self.assertIsNotNone(sol)
        live_iters = it.iterations
        self.assertGreater(live_iters, 0)
        for _ in it:
            pass
        self.assertGreaterEqual(it.iterations, live_iters)

    def test_solve_threads(self):
        puzzle = burrtools.load(self.puzzle_path)
        prob = puzzle.problems[0]

        # Default (auto / all threads)
        asms_default = list(prob.solve(disassemble=False))
        self.assertEqual(len(asms_default), 12)

        # 1 thread
        asms_single = list(prob.solve(disassemble=False, threads=1))
        self.assertEqual(len(asms_single), 12)

        # 4 threads
        asms_multi = list(prob.solve(disassemble=False, threads=4))
        self.assertEqual(len(asms_multi), 12)

if __name__ == "__main__":
    unittest.main()


