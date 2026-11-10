// Rotation study, written 2026-11-03 (against rotations at 2eaddbb7).
// What one turn needs: need scene.txt bevel [play] prints requiredPlay at
// that bevel (planner, shifts up to 0.5) and the planner's clearance at the
// play (default 0.03) for a range of bevels. Scenes are diag's DUMP format.
//
// REFINE=n: polish the planner's widest path at (play, bevel) on a finer grid
// (0.005 cells, 0.25°, shifts up to 0.5): resample it to n waypoints, hill-climb them on the
// sampled smallest clearance, then certify (minClearance). On 2026-11-03 this
// took the blocked turns of TIC 28, Clutch 4 and Clutch 3 from about -0.005
// (the planner's grid) to certified clear at play 0.0126, bevel 0.032.
#include "lib/rotationgeometry.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cmath>
using namespace rotationGeometry;
int main(int argc, char ** argv) {
  FILE * in = fopen(argv[1], "r");
  double bevel = atof(argv[2]);
  unsigned axis = 0, sense = 0; int hx = 0, hy = 0, hz = 0;
  std::vector<cell_t> mv, st;
  char line[1 << 16];
  auto cells = [](const char * l) { std::vector<cell_t> v; int x, y, z, n;
    while ((l = strchr(l, '{')) && sscanf(l, "{%d,%d,%d}%n", &x, &y, &z, &n) == 3) { v.push_back({ x, y, z }); l += n; } return v; };
  while (fgets(line, sizeof line, in)) {
    if (!strncmp(line, "axis", 4)) sscanf(line, "axis %u sense %u pivot %d %d %d", &axis, &sense, &hx, &hy, &hz);
    else if (!strncmp(line, "moving:", 7)) mv = cells(line);
    else if (!strncmp(line, "static:", 7)) st = cells(line);
  }
  const double play = argc > 3 ? atof(argv[3]) : 0.03;
  if (const char * r = getenv("REFINE")) {
    planarScene_c sc(mv, st, axis, hx, hy, hz, { play, bevel });
    planSettings_t fine; fine.maxShift = 0.5; fine.shift = 0.005; fine.steps = 360;
    turnPlan_t t = planTurn(sc, sense, fine);
    printf("fine planner: found %d clearance %.5f upper %.5f\n", t.found, t.clearance, t.upper);
    if (!t.found) return 1;
    auto sampled = [&](const path_t & p) {
      double m = 1e9;
      for (size_t i = 0; i + 1 < p.size(); i++) {
        const pose_t & a = p[i], & b = p[i + 1];
        double L = std::hypot(b.du - a.du, b.dv - a.dv) + sc.radius() * std::fabs(b.theta - a.theta);
        int n = std::max(1, (int)std::ceil(L / 0.001));
        for (int k = 0; k <= n; k++) {
          double f = (double)k / n;
          m = std::min(m, sc.clearanceAbove({ a.theta + f * (b.theta - a.theta), a.du + f * (b.du - a.du), a.dv + f * (b.dv - a.dv) }, m));
        }
      }
      return m;
    };
    const int W = atoi(r);
    path_t p;
    for (int i = 0; i <= W; i++) p.push_back(poseAt(t.path, (double)i / W));
    p.front() = t.path.front(); p.back() = t.path.back();
    double best = sampled(p), dl = 0.01;
    for (int round = 0; round < 4000 && dl > 1e-5; round++) {
      bool improved = false;
      for (int i = 1; i < W; i++) for (int c = 0; c < 3; c++) for (double sg : { 1.0, -1.0 }) {
        path_t q = p;
        (c == 0 ? q[i].theta : (c == 1 ? q[i].du : q[i].dv)) += sg * dl;
        double v = sampled(q);
        if (v > best) { best = v; p = q; improved = true; }
      }
      if (!improved) dl /= 2;
    }
    double cert = sc.minClearance(p, 1e-5);
    printf("refined (%d waypoints): sampled %.5f certified %.5f, %s at play %g bevel %g\n", W, best, cert,
           sc.certifyPath(p) == PATH_CLEAR ? "CLEAR" : "not clear", play, bevel);
    return 0;
  }
  planSettings_t ps;
  playBound_t b = requiredPlay(mv, st, axis, hx, hy, hz, sense, { play, bevel }, ps);
  printf("%s: at bevel %g needs play: found %d, upper %.4f lower %.4f\n", argv[1], bevel, b.found, b.upper, b.lower);
  for (double k : { 0.04, 0.06, 0.08, 0.1, 0.12, 0.15 }) {
    planarScene_c sc(mv, st, axis, hx, hy, hz, { play, k });
    turnPlan_t t = planTurn(sc, sense, ps);
    printf("  play %g bevel %.2f: planner clearance %.4f\n", play, k, t.found ? t.clearance : -1.0);
  }
}
