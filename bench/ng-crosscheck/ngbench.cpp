// Headless benchmark driver for the BurrTools NG solver library.
// usage: ngbench file.xmpuzzle threads disassemble(0|1) [problemIndex]
#include "grid.h"
#include "import.h"
#include "loadstore.h"
#include "multithreadedsolver.h"
#include "symmetry.h"
#include "transformation.h"
#include "nggrids.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

using namespace BurrTools::PuzzleLibrary;



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
    if (argc < 4) { fprintf(stderr, "usage: ngbench file threads disasm [problem]\n"); return 2; }
    const int threads = atoi(argv[2]);
    const bool disasm = atoi(argv[3]) != 0;
    const size_t pidx = argc > 4 ? atoi(argv[4]) : 0;

    TRotation::init_tables();
    RotationalSymmetryGroup::init_tables();
    if (auto e = nggrids::registerAll(); !e.empty()) { fprintf(stderr, "grid: %s\n", e.c_str()); return 1; }

    const char * storePath = getenv("NGB_STORE");
    auto db = storePath ? PuzzleDb::Creator(storePath)() : PuzzleDb::Creator()();
    if (!db) { fprintf(stderr, "no db\n"); return 1; }

    TreeIndex analysis;
    {
        auto t = db->startTransaction(true, "import");
        auto err = importBurrToolsXMLFile(argv[1], t, PuzzleDb::Root);
        if (!err.empty()) { fprintf(stderr, "import: %s\n", err.c_str()); return 1; }
        std::vector<TreeIndex> problems;
        findProblems(t, PuzzleDb::Root, problems);
        if (pidx >= problems.size()) { fprintf(stderr, "only %zu problems\n", problems.size()); return 1; }
        analysis = storeTreeNode(t, problems[pidx], PuzzleTreeNodeType::ProblemAnalysis);
        t.commit();
    }

    std::atomic<uint64_t> assemblies = 0, solutions = 0;
    using S = MultiThreadedSolver<int32_t, uint8_t>;
    S solver(analysis, [&](const Assembly<int32_t> & assm, S & s) -> std::tuple<bool, std::optional<Disassembly<int32_t>>> {
        assemblies++;
        if (!storePath && disasm && s.disassemble(assm, false).has_value()) solutions++;
        if (!storePath) return std::make_tuple(false, std::optional<Disassembly<int32_t>>{});
        if (!disasm) { return std::make_tuple(true, std::optional<Disassembly<int32_t>>{}); }
        auto d = s.disassemble(assm, false);
        if (d) solutions++;
        return std::make_tuple(d.has_value(), std::move(d));
    });

    // database thread: runs the solver's database actions one at a time
    std::mutex m;
    std::condition_variable cv;
    std::queue<std::function<void()>> q;
    bool quit = false;
    std::thread dbthread([&] {
        std::unique_lock l(m);
        while (true)
        {
            cv.wait(l, [&] { return quit || !q.empty(); });
            if (q.empty()) return;
            auto fn = std::move(q.front());
            q.pop();
            l.unlock();
            fn();
            l.lock();
        }
    });

    auto start = std::chrono::steady_clock::now();
    solver.solve(false, threads, [&](bool now, auto fn) {
        std::mutex dm;
        std::condition_variable dcv;
        bool done = false;
        {
            std::lock_guard _(m);
            if (now)
                q.push([&, fn] {
                    { auto t = db->startTransaction(true, "solver"); fn(t); }
                    { std::lock_guard _(dm); done = true; }
                    dcv.notify_one();
                });
            else
                q.push([&db, fn] { auto t = db->startTransaction(true, "solver"); fn(t); });
        }
        cv.notify_one();
        if (now) { std::unique_lock l(dm); dcv.wait(l, [&] { return done; }); }
    });
    auto secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    { std::lock_guard _(m); quit = true; }
    cv.notify_one();
    dbthread.join();

    printf("state=%d assemblies=%llu solutions=%llu time=%.3f\n", (int)solver.getState(), (unsigned long long)assemblies.load(),
           (unsigned long long)solutions.load(), secs);
    return 0;
}
