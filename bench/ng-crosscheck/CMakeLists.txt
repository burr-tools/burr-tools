# Headless drivers that run BurrTools NG's solver library, for cross-checking
# against this repository's solver. See README.md.
cmake_minimum_required(VERSION 3.20)
project(ngcrosscheck C CXX)

set(NG "" CACHE PATH "BurrTools NG source checkout")
set(GSLDIR "" CACHE PATH "Microsoft GSL include directory")
option(NG_SHALLOW_SPLIT_PROTOTYPE
       "Also build ngsplit, using a patched copy of NG's dancingcell.cpp" OFF)
option(NG_CLOSURE_PATCH
       "Also build ngbench_fw, with closure-floyd-warshall.patch applied to a copy of NG's linearmovementanalyzer.h" OFF)

if(NOT NG OR NOT EXISTS "${NG}/src/lib/dancingcell.cpp")
  message(FATAL_ERROR "Set -DNG=<path to a BurrTools NG checkout> (src/lib/dancingcell.cpp not found there).")
endif()
if(NOT GSLDIR OR NOT EXISTS "${GSLDIR}/gsl/gsl")
  message(FATAL_ERROR "Set -DGSLDIR=<path to the Microsoft GSL include directory> (gsl/gsl not found there).")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
add_compile_options(-O3)

add_library(sqlitelib STATIC ${NG}/external/sqlite-lib/sqlite3.c)
target_compile_options(sqlitelib PRIVATE -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0
  -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_LIKE_DOESNT_MATCH_BLOBS -DSQLITE_OMIT_DEPRECATED
  -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_USE_ALLOCA -w)
target_include_directories(sqlitelib PUBLIC ${NG}/external/sqlite-lib)
target_link_libraries(sqlitelib PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# NG sources other than dancingcell.cpp, shared by both variants.
set(NG_COMMON_SOURCES
  ${NG}/src/lib/dancinglink.cpp ${NG}/src/lib/dbsort.cpp ${NG}/src/lib/disassemblymove.cpp
  ${NG}/src/lib/grid.cpp ${NG}/src/lib/import.cpp ${NG}/src/lib/loadstore.cpp
  ${NG}/src/lib/polygen.cpp ${NG}/src/lib/polyhedron.cpp ${NG}/src/lib/puzzle.cpp
  ${NG}/src/lib/symmetry.cpp ${NG}/src/lib/transformation.cpp
  ${NG}/src/tools/aligned_alloc.cpp ${NG}/src/tools/systeminfo.cpp
  ${NG}/src/tools/userdatadirectory.cpp)

function(add_ng_library name dancingcell_source)
  add_library(${name} STATIC ${dancingcell_source} ${NG_COMMON_SOURCES})
  target_include_directories(${name} PUBLIC ${NG}/src/lib ${NG}/src/tools ${NG}/src/octree
    ${NG}/src/maths ${NG}/external/crc-lib ${NG}/external/pssst
    ${NG}/external/sqlite-modern-cpp-lib ${GSLDIR})
  target_link_libraries(${name} PUBLIC sqlitelib ZLIB::ZLIB Threads::Threads)
endfunction()

# ngbench: NG's unmodified solver.
add_ng_library(nglib ${NG}/src/lib/dancingcell.cpp)
add_executable(ngbench ngbench.cpp)
target_link_libraries(ngbench PRIVATE nglib)

# nggriddump: the checks of step G0 of design/2026-11-19-ng-merge-plan.md.
add_executable(nggriddump ngGridDump.cpp)
target_link_libraries(nggriddump PRIVATE nglib)

# ngsplit: needs `g_shallowSplit`, defined only by the patched dancingcell.cpp.
if(NG_SHALLOW_SPLIT_PROTOTYPE)
  find_program(PATCH_EXECUTABLE patch REQUIRED)
  set(PATCHED_DIR ${CMAKE_BINARY_DIR}/patched-ng)
  file(MAKE_DIRECTORY ${PATCHED_DIR})
  set(PATCHED_SRC ${PATCHED_DIR}/dancingcell.cpp)
  add_custom_command(
    OUTPUT ${PATCHED_SRC}
    COMMAND ${CMAKE_COMMAND} -E copy ${NG}/src/lib/dancingcell.cpp ${PATCHED_SRC}
    # -p0 with an explicit file: the patch names a/src/lib/dancingcell.cpp.
    COMMAND ${PATCH_EXECUTABLE} --quiet ${PATCHED_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/dancingcell-shallow-split.patch
    DEPENDS ${NG}/src/lib/dancingcell.cpp ${CMAKE_CURRENT_SOURCE_DIR}/dancingcell-shallow-split.patch
    COMMENT "Patching a copy of NG's dancingcell.cpp (shallow split prototype)")
  add_ng_library(nglib_split ${PATCHED_SRC})
  add_executable(ngsplit ngsplit.cpp)
  target_link_libraries(ngsplit PRIVATE nglib_split)
endif()

# ngbench_fw: ngbench with one Floyd-Warshall pass for the movement closure
# (closure-floyd-warshall.patch). The header is only compiled into the driver
# (through multithreadedsolver.h and disassembler.h), so the library is shared
# with ngbench. NG's headers include each other with quotes, which find their
# own directory first, so the driver gets a copy of all of src/lib's headers,
# patched, first on its include path.
if(NG_CLOSURE_PATCH)
  find_program(PATCH_EXECUTABLE patch REQUIRED)
  set(FW_DIR ${CMAKE_BINARY_DIR}/patched-fw)
  file(GLOB NG_LIB_HEADERS ${NG}/src/lib/*.h)
  set(FW_HDR ${FW_DIR}/linearmovementanalyzer.h)
  add_custom_command(
    OUTPUT ${FW_HDR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${FW_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${NG_LIB_HEADERS} ${FW_DIR}
    COMMAND ${PATCH_EXECUTABLE} --quiet ${FW_HDR} ${CMAKE_CURRENT_SOURCE_DIR}/closure-floyd-warshall.patch
    DEPENDS ${NG_LIB_HEADERS} ${CMAKE_CURRENT_SOURCE_DIR}/closure-floyd-warshall.patch
    COMMENT "Copying NG's headers and patching linearmovementanalyzer.h (Floyd-Warshall closure)")
  add_custom_target(patched_fw_headers DEPENDS ${FW_HDR})
  add_executable(ngbench_fw ngbench.cpp)
  add_dependencies(ngbench_fw patched_fw_headers)
  target_include_directories(ngbench_fw BEFORE PRIVATE ${FW_DIR})
  target_link_libraries(ngbench_fw PRIVATE nglib)
endif()
