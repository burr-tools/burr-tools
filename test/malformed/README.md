# Malformed-input regression fixtures

Each file here is a crafted `.xmpuzzle` that triggered a memory-safety bug in the
loader before the accompanying fix. In a release build (`-Db_ndebug=true`) the
`bt_assert` bounds checks are compiled out, so these produced real out-of-bounds
accesses rather than clean exceptions. After the fix each one is rejected with a
parser exception.

| file | bug | fix |
|------|-----|-----|
| `too_many_voxels.xmpuzzle` | voxel content had one more state char than the space holds; `setState(idx++)` wrote out of bounds *before* the count check | bounds-check moved ahead of the write in `voxel.cpp` |
| `oversized_dimensions.xmpuzzle` | `x*y*z` overflowed the 32-bit size field, under-allocating the space | dimensions validated before the multiply in `voxel.cpp` |
| `separation_before_assembly.xmpuzzle` | a `<solution>` whose `<separation>` preceded its `<assembly>` dereferenced an uninitialized `assembly` pointer | `assembly` initialized to 0 in `solution.cpp` |

Reproduce (release-like sanitizer build):

    meson setup build-asan --buildtype=debugoptimized -Db_sanitize=address,undefined -Db_ndebug=true
    ninja -C build-asan burrTxt
    ./build-asan/burrTxt -q test/malformed/too_many_voxels.xmpuzzle

Expected after the fix: an `xmlParserException_c` (e.g. "too many voxels defined
for voxelspace"), and no AddressSanitizer report.
