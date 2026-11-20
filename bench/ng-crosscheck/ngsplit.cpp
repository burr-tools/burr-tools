// Headless assembly-only driver for the BurrTools NG solver library, with a prototype
// "split on demand" scheduler. Needs the patched dancingcell.cpp (g_shallowSplit).
// usage: ngsplit file.xmpuzzle threads [problemIndex] [slice]; env NG_DEEP=1 uses NG's original split
#include "grid.h"
#include "import.h"
#include "loadstore.h"
#include "multithreadedsolver.h"
#include "assembler.h"
#include "symmetry.h"
#include "transformation.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

using namespace BurrTools::PuzzleLibrary;
namespace BurrTools::PuzzleLibrary { extern bool g_shallowSplit; }
#define EQT sqrtf(0.75f)

static std::array<Grid::RegisterInformation, 2> someGrids{{
    {
        .storageId             = {0},
        .name                  = "Cubes",
        .translationalSymmetry = {1, 0, 0, 0, 1, 0, 0, 0, 1},
        .physicalTranslations  = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0},
        .movementVectors       = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
        .physicalShapes        = {prism({{-0.5, -0.5, -0.5}, {-0.5, 0.5, -0.5}, {0.5, 0.5, -0.5}, {0.5, -0.5, -0.5}}, {0.0, 0.0, 1.0})},
        .basePositions         = {{0, 0, 0}},
        .gridSnaps             = {},
    },
    {
        .storageId             = {4},
        .name                  = "Triangle prisms",
        .translationalSymmetry = {2, 1, 1, -1, 1, 1, -1, -2, 1},
        .physicalTranslations  = {1.0, 0.5, 0.0, 0.0, EQT, 0.0, 0.0, 0.0, 1.0},
        .movementVectors       = {{1, 1, 1}, {2, -1, -1}, {1, 1, -2}, {-1, 2, -1}},
        .physicalShapes        = {prism({{0.0, 0.0, -0.5}, {0.5, EQT, -0.5}, {1.0, 0.0, -0.5}}, {0.0, 0.0, 1.0}),
                                  prism({{1.0, 0.0, -0.5}, {0.5, EQT, -0.5}, {1.5, EQT, -0.5}}, {0.0, 0.0, 1.0})},
        .basePositions         = {{1, 0, -1}, {2, 0, -2}},
        .gridSnaps             = {},
    },
}};

static void findProblems(const PuzzleTransaction & t, TreeIndex id, std::vector<TreeIndex> & out)
{
    for (auto & [cid, ctype, clabel] : loadChildren(t, id))
    {
        if (ctype == PuzzleTreeNodeType::Problem) out.push_back(cid);
        else if (ctype == PuzzleTreeNodeType::Directory) findProblems(t, cid, out);
    }
}

int main(int argc, char ** argv)
{
    if (argc < 3) { fprintf(stderr, "usage: ngsplit file threads [problem] [slice]\n"); return 2; }
    const int threads = atoi(argv[2]);
    const size_t pidx = argc > 3 ? atoi(argv[3]) : 0;
    const uint32_t slice = argc > 4 ? atoi(argv[4]) : 2000;

    g_shallowSplit = getenv("NG_DEEP") == nullptr;
    TRotation::init_tables();
    RotationalSymmetryGroup::init_tables();
    for (auto & g : someGrids)
        if (auto e = Grid::registerGrid(std::move(g)); !e.empty()) { fprintf(stderr, "grid: %s\n", e.c_str()); return 1; }

    auto db = PuzzleDb::Creator()();
    Assembler<uint8_t, int32_t> a;
    auto t0 = std::chrono::steady_clock::now();
    {
        auto t = db->startTransaction(true, "import");
        auto err = importBurrToolsXMLFile(argv[1], t, PuzzleDb::Root);
        if (!err.empty()) { fprintf(stderr, "import: %s\n", err.c_str()); return 1; }
        std::vector<TreeIndex> problems;
        findProblems(t, PuzzleDb::Root, problems);
        if (pidx >= problems.size()) return 1;
        auto analysis = storeTreeNode(t, problems[pidx], PuzzleTreeNodeType::ProblemAnalysis);
        t0 = std::chrono::steady_clock::now();
        auto r = a.prepare(t, analysis,
            [](Voxel<uint8_t> piece, Voxel<uint8_t> result) { return piece.color() == 0 || result.color() == 0 || piece.color() == result.color(); },
            [](TreeIndex, const auto &, TreeIndex, auto) { return true; });
        if (r != Assembler<uint8_t, int32_t>::PrepareResult::Ok) { fprintf(stderr, "prepare failed %d\n", (int)r); return 1; }
    }
    auto t1 = std::chrono::steady_clock::now();

    // split on demand: a busy worker splits its own node when another worker is idle
    std::atomic<uint64_t> assemblies = 0, splits = 0;
    std::mutex m;
    std::condition_variable cv;
    std::vector<size_t> work{0};
    int idle = 0;
    bool finished = false;
    std::atomic<int> hungry = 0;

    auto worker = [&] {
        while (true)
        {
            size_t idx;
            {
                std::unique_lock l(m);
                idle++; hungry = idle;
                if (idle == threads && work.empty()) { finished = true; cv.notify_all(); return; }
                cv.wait(l, [&] { return finished || !work.empty(); });
                if (work.empty()) return;
                idx = work.back(); work.pop_back();
                idle--; hungry = idle;
            }
            while (true)
            {
                a.solve(idx, slice, [&](Assembly<int32_t> &&) { assemblies++; });
                if (a.finished(idx)) break;
                if (hungry.load(std::memory_order_relaxed) > 0 && a.depth(idx) >= 2)
                {
                    size_t n = a.split(idx);
                    splits++;
                    { std::lock_guard _(m); work.push_back(n); }
                    cv.notify_one();
                }
            }
        }
    };
    std::vector<std::thread> ths;
    for (int i = 0; i < threads; i++) ths.emplace_back(worker);
    for (auto & th : ths) th.join();
    auto t2 = std::chrono::steady_clock::now();
    printf("assemblies=%llu splits=%llu prepare=%.3f search=%.3f\n", (unsigned long long)assemblies.load(), (unsigned long long)splits.load(),
           std::chrono::duration<double>(t1 - t0).count(), std::chrono::duration<double>(t2 - t1).count());
    return 0;
}
