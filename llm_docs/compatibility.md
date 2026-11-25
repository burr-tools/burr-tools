# Compatibility is only between released versions

- "Released" means a tagged version (`v0.8.0`, ...), not whatever is on `master`. File formats, saved search state (assembler `ASSEMBLER_VERSION`s), CLI behaviour and APIs only need to stay compatible with what a release shipped. Changing a format introduced after the last release needs no version bump, no reader for the intermediate layout and no migration; bump only when the released format changes. Check with `git show <tag>:<file>` which version a release actually carried.
- Saved *search state* is stricter still: a paused search resumes only with the version that saved it (README, "Pausing a search and continuing later").
