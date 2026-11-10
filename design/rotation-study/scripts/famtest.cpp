// Rotation study, written 2026-11-03 (against rotations at 2eaddbb7): which simple fixed paths clear the turns the
// wide family misses? Reads scenes in diag's DUMP format and tries, per scene,
// wiggles and slides with amounts up to 1.0 and ramps with every pair of hold
// angles (multiples of 15°), in 16 directions; prints which candidate clears
// which scene, then a greedy cover.
//
// famtest play bevel scene.txt...
// TWO=1: two-leg paths instead (out to A, on to B, back; 16 directions,
//   amounts 0.1-0.6, times multiples of 15°).
// OFF=1: shift to s, turn about the pivot + e, shift back (s, e on a 0.05
//   grid up to 0.6): slides (e = s), shifts turning with the piece (e = 0)
//   and turns about an off-centre axis (s = 0). LIST=1 lists every clearing
//   path.
#include "lib/rotationgeometry.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <numbers>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace rotationGeometry;

struct scene_t {
  std::string name;
  unsigned axis = 0, sense = 0;
  int hx = 0, hy = 0, hz = 0;
  std::vector<cell_t> mv, st;
};

static scene_t readScene(const char * fn) {
  scene_t s;
  s.name = fn;
  if (auto p = s.name.rfind('/'); p != std::string::npos) s.name = s.name.substr(p + 1);
  FILE * in = fopen(fn, "r");
  if (!in) { perror(fn); exit(1); }
  char line[1 << 16];
  auto cells = [](const char * l) {
    std::vector<cell_t> v;
    int x, y, z, n;
    while ((l = strchr(l, '{')) && sscanf(l, "{%d,%d,%d}%n", &x, &y, &z, &n) == 3) { v.push_back({ x, y, z }); l += n; }
    return v;
  };
  while (fgets(line, sizeof line, in)) {
    if (!strncmp(line, "axis", 4)) sscanf(line, "axis %u sense %u pivot %d %d %d", &s.axis, &s.sense, &s.hx, &s.hy, &s.hz);
    else if (!strncmp(line, "moving:", 7)) s.mv = cells(line);
    else if (!strncmp(line, "static:", 7)) s.st = cells(line);
  }
  fclose(in);
  return s;
}

struct cand_t {
  std::string kind;   // "wiggle", "slide", "ramp h1-h2"
  int dir;
  double amount;
  path_t path;        // +90°
};

static const double D2R = std::numbers::pi / 180;

/* shift out linearly until h1 (instantly for 0), hold until h2, back linearly by 90 (instantly for 90) */
static path_t ramp(double u, double v, int h1, int h2) {
  path_t p { { 0, 0, 0 } };
  p.push_back({ h1 * D2R, u, v });
  if (h2 > h1) p.push_back({ h2 * D2R, u, v });
  p.push_back({ 90 * D2R, h2 == 90 ? u : 0, h2 == 90 ? v : 0 });
  if (h2 == 90) p.push_back({ 90 * D2R, 0, 0 });
  return p;
}

int main(int argc, char ** argv) {
  if (argc < 4) { fprintf(stderr, "famtest play bevel scene...\n"); return 2; }
  model_t model { atof(argv[1]), atof(argv[2]) };
  std::vector<scene_t> scenes;
  for (int i = 3; i < argc; i++) scenes.push_back(readScene(argv[i]));

  std::vector<cand_t> cands;
  for (int a100 = 5; a100 <= 100; a100 += 5) {
    double a = a100 / 100.0;
    for (int d = 0; d < DIRECTIONS; d++) {
      double u, v;
      directionVector(d, &u, &v);
      cands.push_back({ "wiggle", d, a, makePath({ PATH_WIGGLE, 0, d, a }) });
      for (int h1 = 0; h1 <= 75; h1 += 15)
        for (int h2 = h1; h2 <= 90; h2 += 15)
          cands.push_back({ "ramp " + std::to_string(h1) + "-" + std::to_string(h2), d, a, ramp(a * u, a * v, h1, h2) });
    }
  }
  /* TWO=1: two-leg paths, out to A by t1, on to B by t2, back by t3 (all
   * multiples of 15°, t1 <= t2 <= t3; 90 for t3 means back after the turn) */
  if (getenv("TWO")) {
    cands.clear();
    for (int a1 = 1; a1 <= 6; a1++) for (int d1 = 0; d1 < DIRECTIONS; d1++)
      for (int a2 = 1; a2 <= 6; a2++) for (int d2 = 0; d2 < DIRECTIONS; d2++) {
        if (a1 == a2 && d1 == d2) continue;
        double u1, v1, u2, v2;
        directionVector(d1, &u1, &v1); directionVector(d2, &u2, &v2);
        u1 *= a1 / 10.0; v1 *= a1 / 10.0; u2 *= a2 / 10.0; v2 *= a2 / 10.0;
        for (int t1 = 0; t1 <= 90; t1 += 15) for (int t2 = t1; t2 <= 90; t2 += 15) for (int t3 = t2; t3 <= 90; t3 += 15) {
          if (t3 == t2 && t2 == 90) continue;
          path_t p { { 0, 0, 0 }, { t1 * D2R, u1, v1 } };
          p.push_back({ t2 * D2R, u2, v2 });
          if (t3 > t2) p.push_back({ t3 * D2R, u2, v2 });
          p.push_back({ 90 * D2R, t3 == 90 ? u2 : 0, t3 == 90 ? v2 : 0 });
          if (t3 == 90) p.push_back({ 90 * D2R, 0, 0 });
          char k[96];
          snprintf(k, sizeof k, "2leg %d/%.1f>%d/%.1f %d-%d-%d", d1, a1 / 10.0, d2, a2 / 10.0, t1, t2, t3);
          cands.push_back({ k, d1, std::max(a1, a2) / 10.0, p });
        }
      }
  }
  /* OFF=1: shift to s, turn about the pivot + e (16 segments), shift back:
   * e = s is a slide, e = 0 a shift turning with the piece, s = 0 a turn
   * about an off-centre axis */
  if (getenv("OFF")) {
    cands.clear();
    const int N = 12; const double st = 0.05;
    for (int su = -N; su <= N; su++) for (int sv = -N; sv <= N; sv++)
      for (int eu = -N; eu <= N; eu++) for (int ev = -N; ev <= N; ev++) {
        double Su = su * st, Sv = sv * st, Eu = eu * st, Ev = ev * st;
        path_t p { { 0, 0, 0 } };
        if (su || sv) p.push_back({ 0, Su, Sv });
        for (int i = 1; i <= 16; i++) {
          double th = 90 * D2R * i / 16, c = std::cos(th), s = std::sin(th);
          p.push_back({ th, c * (Su - Eu) - s * (Sv - Ev) + Eu, s * (Su - Eu) + c * (Sv - Ev) + Ev });
        }
        p.push_back({ 90 * D2R, 0, 0 });
        char k[96];
        snprintf(k, sizeof k, "off s=(%.2f,%.2f) e=(%.2f,%.2f)", Su, Sv, Eu, Ev);
        double m = 0; for (auto & q : p) m = std::max(m, std::hypot(q.du, q.dv));
        cands.push_back({ k, 0, m, p });
      }
  }
  fprintf(stderr, "%zu candidates, %zu scenes\n", cands.size(), scenes.size());

  /* clear[c] = set of scenes candidate c clears; best[s] = best clearance per kind */
  std::vector<std::vector<double>> val(scenes.size(), std::vector<double>(cands.size(), -1));
  std::atomic<size_t> next { 0 };
  auto work = [&]() {
    for (;;) {
      size_t job = next++;
      if (job >= scenes.size() * cands.size()) return;
      size_t si = job / cands.size(), ci = job % cands.size();
      const scene_t & s = scenes[si];
      planarScene_c sc(s.mv, s.st, s.axis, s.hx, s.hy, s.hz, model);
      path_t p = cands[ci].path;
      if (s.sense) for (auto & q : p) { q.theta = -q.theta; q.dv = -q.dv; }
      val[si][ci] = sc.clearanceAtLeast(p, 0) ? 1 : -1;
    }
  };
  std::vector<std::thread> th;
  for (int i = 0; i < 4; i++) th.emplace_back(work);
  for (auto & t : th) t.join();

  /* per scene and kind: the smallest amount that clears */
  std::map<std::string, std::vector<double>> minAmount;
  for (size_t ci = 0; ci < cands.size(); ci++)
    for (size_t si = 0; si < scenes.size(); si++)
      if (val[si][ci] > 0) {
        auto & m = minAmount[cands[ci].kind];
        m.resize(scenes.size(), 99);
        m[si] = std::min(m[si], cands[ci].amount);
      }
  if (!getenv("TWO") && !getenv("OFF")) {
  printf("%-12s", "kind");
  for (auto & s : scenes) printf(" %9.9s", s.name.c_str());
  printf("\n");
  for (auto & [k, m] : minAmount) {
    printf("%-12s", k.c_str());
    for (double a : m) a > 50 ? printf(" %9s", "-") : printf(" %9.2f", a);
    printf("\n");
  }
  } else {
    /* how many paths clear each scene, and the first few */
    for (size_t si = 0; si < scenes.size(); si++) {
      size_t n = 0;
      size_t z = 0; for (size_t ci = 0; ci < cands.size(); ci++) if (val[si][ci] > 0 && cands[ci].kind.find("s=(0.00,0.00)") != std::string::npos && z++ < 3) printf("  %s: %s\n", scenes[si].name.c_str(), cands[ci].kind.c_str());
      for (size_t ci = 0; ci < cands.size(); ci++) if (val[si][ci] > 0 && n++ < (getenv("LIST") ? 100 : 0)) printf("  %s: %s\n", scenes[si].name.c_str(), cands[ci].kind.c_str());
      printf("%s: %zu paths clear, %zu without a start shift\n", scenes[si].name.c_str(), n, z);
    }
  }

  /* greedy cover, preferring small amounts on ties */
  std::set<size_t> open;
  for (size_t si = 0; si < scenes.size(); si++) open.insert(si);
  printf("greedy cover:\n");
  while (!open.empty()) {
    size_t bestC = 0, bestN = 0;
    for (size_t ci = 0; ci < cands.size(); ci++) {
      size_t n = 0;
      for (size_t si : open) n += val[si][ci] > 0;
      if (n > bestN || (n == bestN && n && cands[ci].amount < cands[bestC].amount)) { bestN = n; bestC = ci; }
    }
    if (!bestN) { printf("  not coverable:"); for (size_t si : open) printf(" %s", scenes[si].name.c_str()); printf("\n"); break; }
    printf("  %s dir %d amount %.2f clears", cands[bestC].kind.c_str(), cands[bestC].dir, cands[bestC].amount);
    for (size_t si : std::set<size_t>(open)) if (val[si][bestC] > 0) { printf(" %s", scenes[si].name.c_str()); open.erase(si); }
    printf("\n");
  }
}
