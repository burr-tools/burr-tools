/* Diagnose one step of a Crowell solution file that our rotation search
 * cannot make: diag <solutionfile> <k> examines snapshot k-1 -> k.
 *
 * Decomposes the step into a static and a moving group, finds the quarter
 * turn (axis, sense, pivot, axial residual), and checks it against our
 * generator's conditions, our path family, the pure turn, and a
 * configuration-space grid search (sound, resolution limited).
 */
#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembly.h"
#include "lib/rotationtables.h"
#include "lib/rotationgeometry.h"
#include "lib/solution.h"
#include "lib/voxel.h"
#include "lib/gridtype.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>
#include <numbers>
#include <chrono>

using namespace rotationGeometry;
typedef std::array<int, 3> pt;
typedef std::vector<pt> cells;
typedef std::map<unsigned int, cells> config;

static std::vector<std::array<int, 9>> rots;
static void makeRots() {
  int perm[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
  for (auto & p : perm)
    for (int s = 0; s < 8; s++) {
      std::array<int, 9> m{};
      for (int r = 0; r < 3; r++) m[r*3 + p[r]] = (s >> r & 1) ? -1 : 1;
      int det = m[0]*(m[4]*m[8]-m[5]*m[7]) - m[1]*(m[3]*m[8]-m[5]*m[6]) + m[2]*(m[3]*m[7]-m[4]*m[6]);
      if (det == 1) rots.push_back(m);
    }
}
static pt rotApply(const std::array<int, 9> & m, const pt & a) {
  return { m[0]*a[0]+m[1]*a[1]+m[2]*a[2], m[3]*a[0]+m[4]*a[1]+m[5]*a[2], m[6]*a[0]+m[7]*a[1]+m[8]*a[2] };
}
static cells sorted(cells c) { std::sort(c.begin(), c.end()); return c; }

static cells placeShape(const gridType_c * gt, const voxel_c * shape, unsigned int trans, int x, int y, int z) {
  std::unique_ptr<voxel_c> v(gt->getVoxel(shape));
  if (!v->transform(trans)) { fprintf(stderr, "transform failed\n"); exit(1); }
  cells c;
  for (unsigned int a = 0; a < v->getX(); a++)
    for (unsigned int b = 0; b < v->getY(); b++)
      for (unsigned int d = 0; d < v->getZ(); d++)
        if (v->getState(a, b, d) != voxel_c::VX_EMPTY)
          c.push_back({ (int)a + x - v->getHx(), (int)b + y - v->getHy(), (int)d + z - v->getHz() });
  return sorted(c);
}
static config assemblyConfig(const problem_c & pr, const assembly_c * a) {
  config c;
  const gridType_c * gt = pr.getPuzzle().getGridType();
  for (unsigned int i = 0; i < a->placementCount(); i++)
    if (a->isPlaced(i))
      c[i] = placeShape(gt, pr.getPartShape(pr.getPartIdToPieceId(i)), a->getTransformation(i), a->getX(i), a->getY(i), a->getZ(i));
  return c;
}

struct motion_t { int r; pt t; bool operator==(const motion_t & o) const { return r == o.r && t == o.t; } };

static std::vector<motion_t> motionsOf(const cells & a, const cells & b) {
  std::vector<motion_t> res;
  for (int ri = 0; ri < (int)rots.size(); ri++) {
    cells r;
    for (auto & p : a) r.push_back(rotApply(rots[ri], p));
    r = sorted(r);
    pt t = { b[0][0]-r[0][0], b[0][1]-r[0][1], b[0][2]-r[0][2] };
    bool ok = true;
    for (size_t i = 0; i < r.size() && ok; i++)
      ok = r[i][0]+t[0] == b[i][0] && r[i][1]+t[1] == b[i][1] && r[i][2]+t[2] == b[i][2];
    if (ok) res.push_back({ ri, t });
  }
  return res;
}

static std::vector<cell_t> toCells(const cells & c) {
  std::vector<cell_t> r;
  for (auto & p : c) r.push_back({ p[0], p[1], p[2] });
  return r;
}

/* configuration-space grid search over (theta, du, dv); sound: a cell is free
 * if its centre clearance exceeds the largest displacement within it */
static std::vector<pose_t> lastPath;
static bool plan(const planarScene_c & sc, unsigned int sense, double h, int nth, double W, double * maxShift) {
  const int nd = (int)std::lround(W / h);
  const int side = 2 * nd + 1;
  const double dth = 0.5 * std::numbers::pi / nth;
  const double margin = h / std::numbers::sqrt2 + sc.radius() * dth / 2;
  const double sgn = sense == 0 ? 1 : -1;
  auto idx = [&](int k, int a, int b) { return ((size_t)k * side + (a + nd)) * side + (b + nd); };
  std::vector<signed char> state((size_t)(nth + 1) * side * side, -1);   // -1 unknown, 0 blocked, 1 free
  std::vector<int> from(state.size(), -1);
  auto isFree = [&](int k, int a, int b) {
    signed char & s = state[idx(k, a, b)];
    if (s < 0) s = sc.clearance({ sgn * k * dth, a * h, b * h }) >= margin ? 1 : 0;
    return s == 1;
  };
  if (!isFree(0, 0, 0)) return false;
  std::queue<std::array<int, 3>> q;
  q.push({ 0, 0, 0 });
  from[idx(0, 0, 0)] = -2;
  while (!q.empty()) {
    auto [k, a, b] = q.front(); q.pop();
    if (k == nth && a == 0 && b == 0) {
      double m = 0;
      size_t i = idx(k, a, b);
      lastPath.clear();
      while (true) {
        int bb = (int)(i % side) - nd, aa = (int)((i / side) % side) - nd, kk = (int)(i / side / side);
        m = std::max(m, std::hypot(aa * h, bb * h));
        lastPath.push_back({ sgn * kk * dth, aa * h, bb * h });
        if (from[i] == -2) break;
        i = from[i];
      }
      std::reverse(lastPath.begin(), lastPath.end());
      *maxShift = m;
      return true;
    }
    const int nb[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (auto & d : nb) {
      int k2 = k + d[0], a2 = a + d[1], b2 = b + d[2];
      if (k2 < 0 || k2 > nth || a2 < -nd || a2 > nd || b2 < -nd || b2 > nd) continue;
      size_t i2 = idx(k2, a2, b2);
      if (from[i2] != -1) continue;
      if (!isFree(k2, a2, b2)) continue;
      from[i2] = (int)idx(k, a, b);
      q.push({ k2, a2, b2 });
    }
  }
  return false;
}

static std::unique_ptr<puzzle_c> load(const char * fn) {
  auto str = openGzFile(fn);
  if (!str) { fprintf(stderr, "cannot open %s\n", fn); exit(2); }
  xmlParser_c pars(*str);
  return std::make_unique<puzzle_c>(pars);
}

int main(int argc, char ** argv) {
  if (argc < 3) { fprintf(stderr, "diag solutionfile k\n"); return 2; }
  makeRots();
  auto puz = load(argv[1]);
  problem_c * pr = puz->getProblem(0);
  unsigned int k = atoi(argv[2]);
  config A = assemblyConfig(*pr, pr->getSavedSolution(k-1)->getAssembly());
  config B = assemblyConfig(*pr, pr->getSavedSolution(k)->getAssembly());

  std::vector<unsigned int> common;
  for (auto & kv : A) if (B.count(kv.first)) common.push_back(kv.first);
  std::map<unsigned int, std::vector<motion_t>> mot;
  for (unsigned int p : common) mot[p] = motionsOf(A[p], B[p]);

  /* the static group: the motion of the lowest piece shared by most pieces */
  motion_t m1{}; uint32_t g1 = 0; int best = -1;
  for (auto & m : mot[common[0]]) {
    uint32_t g = 0; int n = 0;
    for (unsigned int p : common)
      if (std::find(mot[p].begin(), mot[p].end(), m) != mot[p].end()) { g |= 1u << p; n++; }
    if (n > best) { best = n; g1 = g; m1 = m; }
  }
  std::vector<unsigned int> moving;
  for (unsigned int p : common) if (!(g1 & (1u << p))) moving.push_back(p);
  printf("step %u: pieces", k);
  for (unsigned int p : common) printf(" %c", 'a' + p);
  printf(", moving:");
  for (unsigned int p : moving) printf(" %c", 'a' + p);
  printf("\n");
  if (moving.empty()) { printf("  nothing moves relative to the rest\n"); return 0; }

  /* bring B into the static group's frame: B' = R1^T (B - t1) */
  auto inv = [&](const pt & p) {
    pt q = { p[0]-m1.t[0], p[1]-m1.t[1], p[2]-m1.t[2] };
    const auto & m = rots[m1.r];
    return pt{ m[0]*q[0]+m[3]*q[1]+m[6]*q[2], m[1]*q[0]+m[4]*q[1]+m[7]*q[2], m[2]*q[0]+m[5]*q[1]+m[8]*q[2] };
  };
  cells Am, Bm, St;
  for (unsigned int p : moving) {
    for (auto & c : A[p]) Am.push_back(c);
    for (auto & c : B[p]) Bm.push_back(inv(c));
  }
  for (unsigned int p : common) if (g1 & (1u << p)) for (auto & c : A[p]) St.push_back(c);
  Am = sorted(Am); Bm = sorted(Bm);

  /* do the moving pieces move together? */
  bool together = true;
  {
    std::vector<motion_t> m = motionsOf(Am, Bm);
    together = !m.empty();
  }
  if (!together) { printf("  moving pieces do not move as one rigid group (two moves in one snapshot)\n"); return 0; }

  /* find axis, sense, doubled pivot, axial residual */
  int fAxis = -1, fSense = -1, fu = 0, fv = 0, residual = 0;
  for (unsigned int axis = 0; axis < 3 && fAxis < 0; axis++)
    for (unsigned int sense = 0; sense < 2 && fAxis < 0; sense++)
      for (int u = -60; u <= 60 && fAxis < 0; u++)
        for (int v = -60; v <= 60 && fAxis < 0; v++) {
          if ((u ^ v) & 1) continue;
          int hx, hy, hz;
          pivotOf(axis, u, v, &hx, &hy, &hz);
          cells t;
          for (auto & c : Am) { int x = c[0], y = c[1], z = c[2]; turnAbout(&x, &y, &z, hx, hy, hz, axis, sense); t.push_back({ x, y, z }); }
          t = sorted(t);
          pt d = { Bm[0][0]-t[0][0], Bm[0][1]-t[0][1], Bm[0][2]-t[0][2] };
          int du, dv;
          inPlaneDoubled(d[0], d[1], d[2], axis, &du, &dv);
          if (du || dv) continue;
          bool ok = true;
          for (size_t i = 0; i < t.size() && ok; i++)
            ok = t[i][0]+d[0] == Bm[i][0] && t[i][1]+d[1] == Bm[i][1] && t[i][2]+d[2] == Bm[i][2];
          if (!ok) continue;
          fAxis = axis; fSense = sense; fu = u; fv = v; residual = axialCoordinate(d[0], d[1], d[2], axis);
        }
  if (fAxis < 0) { printf("  not a single quarter turn (half turn or turn plus in-plane shift)\n"); return 0; }

  /* generator conditions: pivot within the moving group's footprint +-1 */
  int gumin = 1 << 30, gumax = -(1 << 30), gvmin = 1 << 30, gvmax = -(1 << 30);
  for (auto & c : Am) { int u, v; inPlaneDoubled(c[0], c[1], c[2], fAxis, &u, &v); gumin = std::min(gumin, u); gumax = std::max(gumax, u); gvmin = std::min(gvmin, v); gvmax = std::max(gvmax, v); }
  /* and the complement's */
  int sumin = 1 << 30, sumax = -(1 << 30), svmin = 1 << 30, svmax = -(1 << 30);
  for (auto & c : St) { int u, v; inPlaneDoubled(c[0], c[1], c[2], fAxis, &u, &v); sumin = std::min(sumin, u); sumax = std::max(sumax, u); svmin = std::min(svmin, v); svmax = std::max(svmax, v); }
  bool inFoot = !(fu < gumin - 1 || fu > gumax + 1 || fv < gvmin - 1 || fv > gvmax + 1);
  bool inFootS = !(fu < sumin - 1 || fu > sumax + 1 || fv < svmin - 1 || fv > svmax + 1);
  printf("  turn %c90 about %c, pivot (%g, %g) %s, axial residual %d, pivot in moving footprint: %s, in static footprint: %s\n",
         fSense ? '-' : '+', "XYZ"[fAxis], 0.5 * fu, 0.5 * fv, (fu & 1) ? "corner" : "centre", residual,
         inFoot ? "yes" : "NO", inFootS ? "yes" : "NO");
  int extU = std::max(std::abs(gumax - fu), std::abs(gumin - fu)), extV = std::max(std::abs(gvmax - fv), std::abs(gvmin - fv));
  printf("  moving cubes reach %g / %g cells from the pivot; table radius %d\n", 0.5 * extU, 0.5 * extV, rotationRadiusFor(*pr));

  /* end pose of the pure turn free (without the axial residual)? */
  int hx, hy, hz;
  pivotOf(fAxis, fu, fv, &hx, &hy, &hz);
  if (residual) {
    std::map<pt, int> occ;
    for (auto & c : St) occ[c] = 1;
    bool endFree = true, preFree = true;
    for (auto & c : Am) {
      int x = c[0], y = c[1], z = c[2]; turnAbout(&x, &y, &z, hx, hy, hz, fAxis, fSense);
      if (occ.count({ x, y, z })) endFree = false;
      pt s = c; s[fAxis] += residual;
      if (occ.count(s)) preFree = false;
    }
    printf("  turn then shift: end of turn %s; shift then turn: shifted start %s\n", endFree ? "free" : "OCCUPIED", preFree ? "free" : "OCCUPIED");
  }

  std::vector<cell_t> mv = toCells(Am), st = toCells(St);
  if (getenv("DUMP")) {
    printf("axis %d sense %d pivot %d %d %d\nmoving:", fAxis, fSense, hx, hy, hz);
    for (auto & c : mv) printf(" {%d,%d,%d},", c.x, c.y, c.z);
    printf("\nstatic:");
    for (auto & c : st) printf(" {%d,%d,%d},", c.x, c.y, c.z);
    printf("\n");
    return 0;
  }
  if (getenv("REQ")) {
    planSettings_t ps;
    ps.shift = getenv("H") ? atof(getenv("H")) : 0.01;
    ps.steps = getenv("NTH") ? atoi(getenv("NTH")) : 180;
    auto t0 = std::chrono::steady_clock::now();
    playBound_t pb = requiredPlay(mv, st, fAxis, hx, hy, hz, fSense, { 0.081, 0, 0 }, ps);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!pb.found) { printf("  requiredPlay: none up to 0.4 (%.0f ms)\n", ms); return 0; }
    planarScene_c sc(mv, st, fAxis, hx, hy, hz, { pb.needs, 0, 0 });
    double maxs = 0; for (auto & p : pb.path) maxs = std::max(maxs, std::hypot(p.du, p.dv));
    printf("  requiredPlay: needs %.4f (proven impossible below %.4f), %zu waypoints, max shift %.3f, recertified %s, %.0f ms\n",
           pb.needs, pb.atLeast, pb.path.size(), maxs, sc.certifyPath(pb.path) == PATH_CLEAR ? "clear" : "NOT CLEAR", ms);
    return 0;
  }
  if (getenv("PLAN")) {
    planSettings_t ps;
    ps.shift = getenv("H") ? atof(getenv("H")) : 0.01;
    ps.steps = getenv("NTH") ? atoi(getenv("NTH")) : 180;
    ps.upper = !getenv("FEAS");
    if (getenv("FEAS")) { ps.widest = false; ps.floor = 0; }
    const double p0 = getenv("P0") ? atof(getenv("P0")) : 0.081;
    planarScene_c sc(mv, st, fAxis, hx, hy, hz, { p0, 0, 0 });
    auto t0 = std::chrono::steady_clock::now();
    turnPlan_t tp = planTurn(sc, fSense, ps);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("  planTurn: %s, clearance %.4f (upper %.4f) -> needs play in [%.4f, %.4f], %zu waypoints, %.0f ms",
           tp.found ? "found" : "none", tp.clearance, tp.upper, p0 - tp.upper, p0 - tp.clearance, tp.path.size(), ms);
    if (tp.found) {
      printf(", at p0: certify %s minClearance %.4f", sc.certifyPath(tp.path) == PATH_CLEAR ? "clear" : "NOT", sc.minClearance(tp.path));
      for (double pp : { 0.07, 0.06, 0.04, 0.02 }) { planarScene_c s3(mv, st, fAxis, hx, hy, hz, { pp, 0, 0 }); printf(" | %.2f: %.4f", pp, s3.minClearance(tp.path)); }
      double worst = 1e9; for (int k = 0; k <= 90; k++) worst = std::min(worst, sc.clearance({ (fSense ? -1 : 1) * k * std::numbers::pi / 180, 0, 0 }));
      printf(" | pure worst %.3f", worst);
      planarScene_c sc2(mv, st, fAxis, hx, hy, hz, { std::max(0.001, p0 - tp.clearance + 1e-4), 0, 0 });
      printf(", recertified at that play: %s", sc2.certifyPath(tp.path) == PATH_CLEAR ? "clear" : "NOT CLEAR");
    }
    if (!tp.found) { double worst = 1e9; for (int k = 0; k <= 90; k++) worst = std::min(worst, sc.clearance({ (fSense ? -1 : 1) * k * std::numbers::pi / 180, 0, 0 })); printf(" pure worst %.3f", worst); }
    printf("\n");
    return 0;
  }
  if (getenv("FINE")) {
    double h = atof(getenv("FINE")); int nth = atoi(getenv("NTH")); double W = atof(getenv("W"));
    double play = getenv("PLAY") ? atof(getenv("PLAY")) : 0.081;
    planarScene_c sc(mv, st, fAxis, hx, hy, hz, { play, 0, 0 });
    double ms = 0;
    bool planned = plan(sc, fSense, h, nth, W, &ms);
    printf("  fine search play %.3f h %g nth %d W %g: %s", play, h, nth, W, planned ? "FOUND" : "none");
    if (planned) {
      printf(" max shift %.3f, certified %s\n", ms, sc.certifyPath(lastPath) == PATH_CLEAR ? "yes" : "NO");
      /* print the path every 5 degrees */
      int lastDeg = -100;
      for (auto & p : lastPath) {
        int deg = (int)std::lround(std::fabs(p.theta) * 180 / std::numbers::pi);
        if (deg % 5 == 0 && deg != lastDeg) { printf("    %3d deg  (%+.3f, %+.3f)\n", deg, p.du, p.dv); lastDeg = deg; }
      }
    } else printf("\n");
    return 0;
  }
  const double plays[] = { 0.081, 0.1, 0.12, 0.15, 0.2, 0.25, 0.3 };
  auto settings = defaultRotationSettings();
  for (double play : plays) {
    model_t model = { play, 0, 0 };
    planarScene_c sc(mv, st, fAxis, hx, hy, hz, model);
    verdict_e pure = sc.certifyQuarterTurn(fSense);
    rotationSettings_t s2 = settings; s2.model = model;
    auto tables = sharedRotationTables(s2, std::max(rotationRadiusFor(*pr), (std::max(extU, extV) + 1) / 2 + 1));
    int fc = tables->firstClear(mv, st, fAxis, hx, hy, hz, fSense);
    double ms = 0;
    bool planned = plan(sc, fSense, 0.02, 120, 0.8, &ms);
    printf("  play %.3f: pure %s, family %s, grid search %s", play,
           pure == PATH_CLEAR ? "clear" : pure == PATH_BLOCKED ? "blocked" : "undecided",
           fc < 0 ? "blocked" : ("path " + std::to_string(fc)).c_str(), planned ? "FOUND" : "none");
    if (fc >= 0) {
      const pathSpec_t & sp = tables->table(fSense, fc).getSpec();
      printf(" [family: kind %d dir (%d,%d) amount %g]", sp.kind, sp.dirU, sp.dirV, sp.amount);
    }
    if (planned) printf(" [max shift %.3f]", ms);
    printf("\n");
    if (planned && play == 0.081) { }
  }
  return 0;
}
