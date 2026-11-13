// removal file.xmpuzzle snapshot play,bevel...: the two pieces of a solution file's snapshot (the last two of L 36 / L 42):
// can the second leave the first by a motion in the plane of one axis followed by a straight slide, along that axis or
// in the plane? rotationGeometry::decideRemoval per model, axis and slide direction, the answer certified with
// certifyRemoval. The pivot is the moving piece's centre, the motion turns by up to MAXANGLE degrees (90) either way
// and shifts by up to MAXSHIFT (0.75) cells; PIVOT=hx,hy,hz (doubled) sets the pivot; MAXBOXES (decideSettings_t's), SWAP=1 moves the first piece instead,
// AXES=xyz limits the axes, PLANE=0 skips the slides in the plane, FIRST=1 goes to the next model after the first
// removal found.
// Build: see "Reproducing" in design/2026-10-19-rotation-moves.md.
#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembly.h"
#include "lib/rotationgeometry.h"
#include "lib/solution.h"
#include "lib/voxel.h"
#include "lib/gridtype.h"
#include "tools/xml.h"
#include "tools/gzstream.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace rotationGeometry;
static std::vector<cell_t> place(const gridType_c * gt, const voxel_c * shape, unsigned trans, int x, int y, int z) {
  std::unique_ptr<voxel_c> v(gt->getVoxel(shape));
  v->transform(trans);
  std::vector<cell_t> c;
  for (unsigned a = 0; a < v->getX(); a++) for (unsigned b = 0; b < v->getY(); b++) for (unsigned d = 0; d < v->getZ(); d++)
    if (v->getState(a, b, d) != voxel_c::VX_EMPTY) c.push_back({ (int)a + x - v->getHx(), (int)b + y - v->getHy(), (int)d + z - v->getHz() });
  return c;
}
int main(int argc, char ** argv) {
  if (argc < 4) { fprintf(stderr, "removal file snapshot play,bevel...\n"); return 2; }
  auto str = openGzFile(argv[1]);
  xmlParser_c pars(*str);
  puzzle_c puz(pars);
  problem_c * pr = puz.getProblem(0);
  const assembly_c * a = pr->getSavedSolution(atoi(argv[2]))->getAssembly();
  std::vector<std::vector<cell_t>> pc;
  std::string names;
  for (unsigned i = 0; i < a->placementCount(); i++)
    if (a->isPlaced(i)) {
      pc.push_back(place(puz.getGridType(), pr->getPartShape(pr->getPartIdToPieceId(i)), a->getTransformation(i), a->getX(i), a->getY(i), a->getZ(i)));
      names += char('a' + i);
    }
  if (pc.size() != 2) { printf("%zu pieces, not 2\n", pc.size()); return 1; }
  const bool swap = getenv("SWAP") && atoi(getenv("SWAP"));
  const std::vector<cell_t> & st = pc[swap ? 1 : 0], & mv = pc[swap ? 0 : 1];
  printf("%s snapshot %s: piece %c (%zu cubes) moves, %c (%zu cubes) stays\n", argv[1], argv[2], names[swap ? 0 : 1], mv.size(),
         names[swap ? 1 : 0], st.size());
  decideSettings_t ds;
  if (getenv("MAXSHIFT")) ds.maxShift = atof(getenv("MAXSHIFT"));
  if (getenv("MAXBOXES")) ds.maxBoxes = atol(getenv("MAXBOXES"));
  removalGoal_t base;
  if (getenv("MAXANGLE")) base.maxAngle = atof(getenv("MAXANGLE")) * std::numbers::pi / 180;
  const bool plane = !getenv("PLANE") || atoi(getenv("PLANE"));
  const char * axes = getenv("AXES") ? getenv("AXES") : "xyz";
  for (int m = 3; m < argc; m++) {
    model_t md;
    sscanf(argv[m], "%lf,%lf", &md.play, &md.chamfer);
    for (unsigned axis = 0; axis < 3; axis++) {
      if (!strchr(axes, "xyz"[axis])) continue;
      int umin = INT_MAX, umax = INT_MIN, vmin = INT_MAX, vmax = INT_MIN;
      for (const cell_t & c : mv) { int u, v; inPlaneDoubled(c.x, c.y, c.z, axis, &u, &v); umin = std::min(umin, u); umax = std::max(umax, u); vmin = std::min(vmin, v); vmax = std::max(vmax, v); }
      int hx, hy, hz;
      pivotOf(axis, ((umin + umax) / 2) & ~1, ((vmin + vmax) / 2) & ~1, &hx, &hy, &hz);
      if (getenv("PIVOT")) sscanf(getenv("PIVOT"), "%d,%d,%d", &hx, &hy, &hz);
      struct g_t { const char * name; removalGoal_t g; };
      std::vector<g_t> goals;
      const char * an = axis == 0 ? "x" : axis == 1 ? "y" : "z";
      removalGoal_t g = base;
      g.axial = true; g.sign = 1; goals.push_back({ "+axis", g });
      g.sign = -1; goals.push_back({ "-axis", g });
      if (plane) {
        const double r = std::numbers::sqrt2 / 2;
        const double dirs[8][2] = { { 1, 0 }, { r, r }, { 0, 1 }, { -r, r }, { -1, 0 }, { -r, -r }, { 0, -1 }, { r, -r } };
        const char * dn[8] = { "+u", "+u+v", "+v", "-u+v", "-u", "-u-v", "-v", "+u-v" };
        for (int d = 0; d < 8; d++) { g.axial = false; g.wu = dirs[d][0]; g.wv = dirs[d][1]; goals.push_back({ dn[d], g }); }
      }
      for (const g_t & gg : goals) {
        auto t0 = std::chrono::steady_clock::now();
        turnDecision_t t = decideRemoval(mv, st, axis, hx, hy, hz, md, gg.g, ds);
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::string extra;
        if (t.answer == TURN_POSSIBLE) {
          const verdict_e v = certifyRemoval(mv, st, axis, hx, hy, hz, md, gg.g, t.path);
          const pose_t & e = t.path.back();
          char buf[200];
          snprintf(buf, sizeof buf, ", ends at %.1f deg shift (%.3f, %.3f), %zu waypoints, certified: %s", e.theta * 180 / std::numbers::pi, e.du, e.dv,
                   t.path.size(), v == PATH_CLEAR ? "clear" : v == PATH_BLOCKED ? "BLOCKED" : "undecided");
          extra = buf;
        }
        printf("%s about %s (pivot %d %d %d), slide %s %s: %s (%zu boxes, %u rounds%s, %.2f s)%s\n", argv[m], an, hx, hy, hz, gg.name, gg.g.axial ? "along the axis" : "in the plane",
               t.answer == TURN_POSSIBLE ? "POSSIBLE" : t.answer == TURN_IMPOSSIBLE ? "impossible" : "undecided", t.boxes, t.rounds,
               t.gaveUp ? ", gave up" : "", sec, extra.c_str());
        fflush(stdout);
        if (getenv("FIRST") && t.answer == TURN_POSSIBLE) goto nextModel;
      }
    }
    nextModel: ;
  }
}
