# Coverage Tooling (PR 0) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add coverage measurement to BurrTools so every subsequent test PR can report a real before/after number.

**Architecture:** A separate `build-cov` Meson directory configured with the builtin `-Db_coverage=true` option, driven by a new `just coverage` recipe that builds, runs the Catch2 suite, and reports with `gcovr`. Filters restrict reporting to `src/lib`, `src/tools`, and `src/halfedge`. A non-blocking CI job prints the same summary on every PR.

**Tech Stack:** Meson 1.12, Ninja, just, Catch2 v3, gcovr, gcov (Linux/gcc) or `xcrun llvm-cov gcov` (macOS/Apple Clang).

**Spec:** `docs/superpowers/specs/2026-09-13-test-coverage-stack-design.md`

## Global Constraints

- Never edit files under `subprojects/` or `src/lua/` (AGENTS.md rule 5).
- Coverage reporting filters to `src/lib`, `src/tools`, `src/halfedge` only.
- This PR changes **no production code**. Only `justfile`, `AGENTS.md`, `.github/workflows/build-and-release.yml`, and `.gitignore`.
- The existing `build/` directory and its recipes must keep working unchanged; coverage uses a separate `build-cov/`.
- Branch: `coverage/tooling`, based on `master`.
- The CI coverage job must be non-blocking (`continue-on-error: true`) — it reports, it does not gate.

---

### Task 1: Confirm gcovr works against this project

This task is a spike before any file is edited. If gcovr cannot read Apple Clang's coverage data, the fallback in the spec (source-based coverage) applies and the rest of the plan changes. Find out first.

**Files:**
- Create: none (throwaway build directory only)
- Modify: none

**Interfaces:**
- Consumes: nothing
- Produces: a verified `gcovr` invocation string that later tasks paste into the justfile

- [ ] **Step 1: Install gcovr**

```bash
brew install gcovr
gcovr --version
```

Expected: a version number, 7.0 or later.

- [ ] **Step 2: Configure a coverage build**

```bash
cd /Users/tburns/code/burr-tools/.claude/worktrees/coverage-stack
meson setup build-cov -Db_coverage=true
```

Expected: configuration succeeds. This compiles the fltk/manifold/libpng subprojects again, so allow several minutes.

- [ ] **Step 3: Build and run the suite under coverage**

```bash
ninja -C build-cov
./build-cov/test_burrtools
```

Expected: `All tests passed (5040 assertions in 22 test cases)`. The assertion count may differ if master has moved; what matters is that it passes.

- [ ] **Step 4: Produce a report**

```bash
gcovr --root . \
      --filter 'src/lib/' --filter 'src/tools/' --filter 'src/halfedge/' \
      --exclude 'src/lua/' \
      --gcov-executable "xcrun llvm-cov gcov" \
      --print-summary build-cov
```

Expected: a per-file table and a `TOTAL` line with a percentage. **Record that percentage — it is the baseline for the whole stack.**

If gcovr errors with parse failures on `.gcda` files, stop and report. The fallback is `-Dcpp_args=-fprofile-instr-generate -fcoverage-mapping` read via `xcrun llvm-cov report`, which diverges from the CI path and needs a decision before continuing.

- [ ] **Step 5: Record the baseline**

Write the TOTAL percentage into your notes. No commit in this task — nothing was changed.

---

### Task 2: Add the `just coverage` recipe

**Files:**
- Modify: `justfile` (append new recipes after the existing `check-all` recipe)
- Modify: `.gitignore` (ignore `build-cov/` and `coverage-html/`)

**Interfaces:**
- Consumes: the verified gcovr invocation from Task 1
- Produces: `just coverage` (terminal summary) and `just coverage-html` (HTML report), used by every later PR in the stack and by CI

- [ ] **Step 1: Verify the recipe does not already exist**

```bash
grep -n 'coverage' justfile
```

Expected: no output. If `coverage` already exists, stop and report — the plan assumed a clean justfile.

- [ ] **Step 2: Add the recipes to the justfile**

Append after the `check-all` recipe. The `gcov_exe` variable handles the macOS/Linux split: Apple Clang needs the `llvm-cov gcov` shim, gcc does not.

```just
# Configure the coverage build directory if not already set up
setup-cov:
    @if [ ! -d "build-cov" ]; then meson setup build-cov -Db_coverage=true; fi

# Report test coverage for BurrTools sources (excludes subprojects and lua)
coverage: setup-cov
    ninja -C build-cov
    ./build-cov/test_burrtools
    gcovr --root . \
          --filter 'src/lib/' --filter 'src/tools/' --filter 'src/halfedge/' \
          --exclude 'src/lua/' \
          {{ if os() == "macos" { '--gcov-executable "xcrun llvm-cov gcov"' } else { "" } }} \
          --print-summary build-cov

# Write an HTML coverage report to coverage-html/index.html
coverage-html: setup-cov
    ninja -C build-cov
    ./build-cov/test_burrtools
    mkdir -p coverage-html
    gcovr --root . \
          --filter 'src/lib/' --filter 'src/tools/' --filter 'src/halfedge/' \
          --exclude 'src/lua/' \
          {{ if os() == "macos" { '--gcov-executable "xcrun llvm-cov gcov"' } else { "" } }} \
          --html-details coverage-html/index.html
```

- [ ] **Step 3: Verify the recipe runs**

```bash
just coverage
```

Expected: builds, runs the suite, prints the `TOTAL` line. The percentage must match the Task 1 baseline.

- [ ] **Step 4: Verify the HTML recipe runs**

```bash
just coverage-html && test -f coverage-html/index.html && echo "html ok"
```

Expected: `html ok`.

- [ ] **Step 5: Ignore the generated directories**

Append to `.gitignore`:

```
build-cov/
coverage-html/
```

- [ ] **Step 6: Verify nothing generated is staged**

```bash
git status --porcelain
```

Expected: only `justfile` and `.gitignore` modified. No `build-cov/` or `coverage-html/` entries.

- [ ] **Step 7: Commit**

```bash
git add justfile .gitignore
git commit -m "build: add just coverage recipes backed by gcovr

Coverage is reported from a separate build-cov directory configured with
Meson's builtin b_coverage option, filtered to src/lib, src/tools and
src/halfedge so vendored code in subprojects and src/lua stays out of the
numbers. On macOS gcovr is pointed at 'xcrun llvm-cov gcov' because Apple
Clang emits data plain gcov cannot parse.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 3: Add the non-blocking CI coverage job

**Files:**
- Modify: `.github/workflows/build-and-release.yml` (add a `coverage` job after `build-linux`)

**Interfaces:**
- Consumes: `just coverage` from Task 2
- Produces: a CI job named `coverage` whose log contains the summary, plus an uploaded HTML artifact

- [ ] **Step 1: Read the existing build-linux job to match its setup steps**

```bash
sed -n '1,40p' .github/workflows/build-and-release.yml
```

Note the dependency-install line and the `extractions/setup-just@v2` step — the new job reuses both.

- [ ] **Step 2: Add the coverage job**

Insert as a new top-level job, sibling to `build-linux`. Indentation must match the existing jobs (two spaces for the job name).

```yaml
  coverage:
    runs-on: ubuntu-latest
    continue-on-error: true
    steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        fetch-depth: 0
        submodules: recursive

    - name: Update package list
      run: sudo apt-get update

    - name: Install dependencies
      run: sudo apt-get install -y libboost-all-dev meson ninja-build build-essential freeglut3-dev libgl-dev libglu1-mesa-dev libpng-dev mesa-common-dev zlib1g-dev gcovr

    - name: Install just
      uses: extractions/setup-just@v2

    - name: Report coverage
      run: just coverage

    - name: Generate HTML report
      run: just coverage-html

    - name: Upload HTML coverage report
      uses: actions/upload-artifact@v4
      with:
        name: coverage-html
        path: coverage-html/
```

- [ ] **Step 3: Verify the YAML parses**

```bash
python3 -c "import yaml,sys; d=yaml.safe_load(open('.github/workflows/build-and-release.yml')); print(sorted(d['jobs'].keys()))"
```

Expected: a list of job names including `coverage`. If `yaml` is missing, `pip3 install pyyaml` first.

- [ ] **Step 4: Verify the job is non-blocking**

```bash
python3 -c "import yaml; d=yaml.safe_load(open('.github/workflows/build-and-release.yml')); print(d['jobs']['coverage']['continue-on-error'])"
```

Expected: `True`.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/build-and-release.yml
git commit -m "ci: report test coverage on every pull request

The job is non-blocking: it prints a gcovr summary and uploads an HTML
report as an artifact, but a coverage drop does not fail the build. The
stack that follows uses these numbers to show what each PR adds.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 4: Document the recipes and open the PR

**Files:**
- Modify: `AGENTS.md` (section 1, the command list)

**Interfaces:**
- Consumes: everything above
- Produces: the merged-ready `coverage/tooling` branch

- [ ] **Step 1: Add the commands to AGENTS.md**

In section 1's fenced command block, after the `just check-analyzer` line, add:

```
just coverage       # Report test coverage for BurrTools sources (gcovr)
just coverage-html  # Write an HTML coverage report to coverage-html/index.html
```

- [ ] **Step 2: Note the local prerequisite**

Immediately after that fenced block, add:

```markdown
Coverage requires `gcovr` (`brew install gcovr` on macOS, `apt-get install gcovr` on Linux).
On macOS the recipes pass `--gcov-executable "xcrun llvm-cov gcov"` automatically, because
Apple Clang emits coverage data that plain `gcov` cannot parse.
```

- [ ] **Step 3: Run the full verification suite**

```bash
just build && just test && just check
```

Expected: build succeeds, all tests pass, cppcheck reports nothing. Per AGENTS.md rule 6 all three must be clean before finishing.

- [ ] **Step 4: Commit**

```bash
git add AGENTS.md
git commit -m "docs: document the coverage recipes in AGENTS.md

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

- [ ] **Step 5: Push and open the PR**

Confirm with the user before pushing — this is the first outward-facing step in the stack.

```bash
git push -u origin coverage/tooling
gh pr create --base master --head coverage/tooling \
  --title "Add test coverage measurement" \
  --body "$(cat <<'EOF'
## What

Adds `just coverage` and `just coverage-html`, backed by gcovr against a
separate `build-cov` directory configured with Meson's builtin
`b_coverage` option. Adds a non-blocking CI job that prints the summary
and uploads an HTML report.

No production code changes.

## Baseline

Coverage of `src/lib`, `src/tools` and `src/halfedge` at this commit:
**<TOTAL from Task 1>%**

This is PR 0 of a five-PR stack that adds unit tests for voxel/symmetry,
serialization, disassembly internals, and the puzzle/problem model. Each
later PR reports its own before/after against this baseline.

Design: `docs/superpowers/specs/2026-09-13-test-coverage-stack-design.md`

## Notes

- macOS needs the `xcrun llvm-cov gcov` shim; the recipe applies it automatically.
- The CI job is `continue-on-error: true` — it reports, it does not gate.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht
EOF
)"
```

Replace `<TOTAL from Task 1>` with the real number before running.

---

## Self-Review

**Spec coverage.** The spec's PR 0 section asks for: a justfile recipe using `-Db_coverage=true` (Task 2), gcovr filtered to the three source directories excluding `src/lua` and `subprojects` (Task 2), the macOS `llvm-cov gcov` shim (Tasks 1 and 2), terminal plus HTML output (Task 2), a non-blocking CI job (Task 3), AGENTS.md documentation (Task 4), and a recorded baseline in the PR body (Tasks 1 and 4). All covered. The spec also says this PR carries the design document — already committed as `eddc904` on this branch.

**Placeholder scan.** One intentional placeholder remains: `<TOTAL from Task 1>` in the PR body, which cannot be known until Task 1 runs. Task 4 Step 5 flags it explicitly.

**Type consistency.** Recipe names are `coverage`, `coverage-html`, and `setup-cov` throughout Tasks 2, 3, and 4. The build directory is `build-cov` in every task. The filter set is identical in Tasks 1 and 2.
