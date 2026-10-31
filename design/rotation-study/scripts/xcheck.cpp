/* Cross-check a Crowell "_SolutionFile" (a list of snapshot assemblies) against
 * our rotation-enabled disassembly of its first snapshot.
 *
 * Every configuration (snapshot or separation state) is reduced to a canonical
 * form: the voxel sets of the present pieces, moved by the rigid motion that
 * puts the lowest present piece back into its assembly pose. Two
 * configurations that differ only by a global rigid motion compare equal.
 */
#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembly.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/rotationtables.h"
#include "lib/solution.h"
#include "lib/voxel.h"
#include "lib/gridtype.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

typedef std::array<int, 3> pt;
typedef std::vector<pt> cells;                       // sorted
typedef std::map<unsigned int, cells> config;        // piece -> cells
typedef std::vector<std::pair<unsigned int, cells>> canon_t;

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

static cells placeShape(const gridType_c * gt, const voxel_c * shape, unsigned int trans, int x, int y, int z) {
  std::unique_ptr<voxel_c> v(gt->getVoxel(shape));
  if (!v->transform(trans)) { fprintf(stderr, "transform failed\n"); exit(1); }
  cells c;
  for (unsigned int a = 0; a < v->getX(); a++)
    for (unsigned int b = 0; b < v->getY(); b++)
      for (unsigned int d = 0; d < v->getZ(); d++)
        if (v->getState(a, b, d) != voxel_c::VX_EMPTY)
          c.push_back({ (int)a + x - v->getHx(), (int)b + y - v->getHy(), (int)d + z - v->getHz() });
  std::sort(c.begin(), c.end());
  return c;
}

static config assemblyConfig(const problem_c & pr, const assembly_c * a) {
  config c;
  const gridType_c * gt = pr.getPuzzle().getGridType();
  for (unsigned int i = 0; i < a->placementCount(); i++)
    if (a->isPlaced(i))
      c[i] = placeShape(gt, pr.getPartShape(pr.getPartIdToPieceId(i)), a->getTransformation(i), a->getX(i), a->getY(i), a->getZ(i));
  return c;
}

static config home;   // assembly pose of every piece

static canon_t canon(const config & c) {
  unsigned int ref = c.begin()->first;
  const cells & want = home.at(ref);
  canon_t best;
  bool have = false;
  for (auto & m : rots) {
    cells r;
    for (auto & p : c.at(ref)) r.push_back(rotApply(m, p));
    std::sort(r.begin(), r.end());
    pt t = { want[0][0]-r[0][0], want[0][1]-r[0][1], want[0][2]-r[0][2] };
    bool ok = true;
    for (size_t i = 0; i < r.size() && ok; i++)
      ok = (r[i][0]+t[0] == want[i][0] && r[i][1]+t[1] == want[i][1] && r[i][2]+t[2] == want[i][2]);
    if (!ok) continue;
    canon_t res;
    for (auto & [pc, cl] : c) {
      cells o;
      for (auto & p : cl) { pt q = rotApply(m, p); o.push_back({ q[0]+t[0], q[1]+t[1], q[2]+t[2] }); }
      std::sort(o.begin(), o.end());
      res.push_back({ pc, o });
    }
    if (!have || res < best) { best = res; have = true; }
  }
  if (!have) { fprintf(stderr, "reference piece does not match its assembly shape\n"); exit(1); }
  return best;
}

static std::string pieceSet(const config & c) {
  std::string s;
  for (auto & kv : c) s += char('a' + kv.first);
  return s;
}

/* our disassembly, flattened: per separation state the canonical config of the
 * pieces that are still in */
struct ourState { std::string where; std::string pieces; bool rotation; };
static std::map<canon_t, ourState> ours;
static std::vector<std::string> ourLines;
/* configs on the way out: the removed group of a separation's last move
 * shifted by k cells along its removal direction, k = 1..15 */
static std::map<canon_t, std::string> partial;

static void walk(const separation_c * s, const problem_c & pr, const assembly_c * a, const std::string & name) {
  const gridType_c * gt = pr.getPuzzle().getGridType();
  std::string line = name + " {";
  for (unsigned int p : s->getPieces()) line += char('a' + p);
  line += "}: " + std::to_string(s->getMoves()) + " moves";
  for (unsigned int i = 0; i <= s->getMoves(); i++) {
    const state_c * st = s->getState(i);
    config c;
    for (unsigned int k = 0; k < s->getPieceNumber(); k++) {
      if (st->pieceRemoved(k)) continue;
      unsigned int pc = s->getPieceName(k);
      unsigned int trans = st->hasTrans() ? st->getTrans(k) : a->getTransformation(pc);
      c[pc] = placeShape(gt, pr.getPartShape(pr.getPartIdToPieceId(pc)), trans, st->getX(k), st->getY(k), st->getZ(k));
    }
    if (st->getRotation()) line += " R@" + std::to_string(i);
    canon_t k = canon(c);
    if (!ours.count(k))
      ours[k] = { name + "#" + std::to_string(i), pieceSet(c), st->getRotation() != nullptr };
  }
  if (s->getMoves() >= 1) {
    const state_c * last = s->getState(s->getMoves()), * prev = s->getState(s->getMoves() - 1);
    config c;
    std::vector<unsigned int> group;
    pt dir{};
    for (unsigned int k = 0; k < s->getPieceNumber(); k++) {
      unsigned int pc = s->getPieceName(k);
      unsigned int trans = prev->hasTrans() ? prev->getTrans(k) : a->getTransformation(pc);
      c[pc] = placeShape(gt, pr.getPartShape(pr.getPartIdToPieceId(pc)), trans, prev->getX(k), prev->getY(k), prev->getZ(k));
      if (last->pieceRemoved(k)) {
        group.push_back(pc);
        auto q = [](int d) { return (d + (d > 0 ? 10000 : -10000)) / 20000; };
        dir = { q(last->getX(k) - prev->getX(k)), q(last->getY(k) - prev->getY(k)), q(last->getZ(k) - prev->getZ(k)) };
      }
    }
    line += " removes {";
    for (unsigned int pc : group) line += char('a' + pc);
    line += "} along (" + std::to_string(dir[0]) + "," + std::to_string(dir[1]) + "," + std::to_string(dir[2]) + ")";
    for (int step = 1; step <= 15; step++) {
      config d = c;
      for (unsigned int pc : group)
        for (auto & p : d[pc]) { p[0] += step*dir[0]; p[1] += step*dir[1]; p[2] += step*dir[2]; }
      canon_t k = canon(d);
      if (!partial.count(k))
        partial[k] = name + "#" + std::to_string(s->getMoves()) + " partway (" + std::to_string(step) + " cells)";
    }
  }
  ourLines.push_back(line);
  if (s->getRemoved()) walk(s->getRemoved(), pr, a, name + "r");
  if (s->getLeft()) walk(s->getLeft(), pr, a, name + "l");
}

/* for an unmatched snapshot: our state with the same pieces that differs in
 * the fewest pieces, and how those pieces differ (offset, or turned) */
static std::string nearest(const canon_t & k) {
  const std::pair<const canon_t, ourState> * best = nullptr;
  int bestDiff = 1000;
  for (auto & e : ours) {
    if (e.first.size() != k.size()) continue;
    bool same = true;
    for (size_t i = 0; i < k.size(); i++) same = same && e.first[i].first == k[i].first;
    if (!same) continue;
    int diff = 0;
    for (size_t i = 0; i < k.size(); i++) {
      const cells & a = k[i].second, & b = e.first[i].second;
      if (a == b) continue;
      bool shift = a.size() == b.size();
      for (size_t j = 0; j < a.size() && shift; j++)
        for (int x = 0; x < 3; x++) shift = shift && a[j][x]-b[j][x] == a[0][x]-b[0][x];
      diff += shift ? 2 : 3;
    }
    if (diff < bestDiff) { bestDiff = diff; best = &e; }
  }
  if (!best) return "- (no state with these pieces)";
  std::string r = "- nearest " + best->second.where + ":";
  for (size_t i = 0; i < k.size(); i++) {
    const cells & a = k[i].second, & b = best->first[i].second;
    if (a == b) continue;
    r += std::string(" ") + char('a' + k[i].first);
    pt t = { a[0][0]-b[0][0], a[0][1]-b[0][1], a[0][2]-b[0][2] };
    bool shift = true;
    for (size_t j = 0; j < a.size() && shift; j++)
      shift = a[j][0]-b[j][0] == t[0] && a[j][1]-b[j][1] == t[1] && a[j][2]-b[j][2] == t[2];
    if (shift) r += "(" + std::to_string(t[0]) + "," + std::to_string(t[1]) + "," + std::to_string(t[2]) + ")";
    else r += "(turned)";
  }
  return r;
}

/* how piece cells a became cells b: shift, or turn (angle about axis) */
static std::string motion(const cells & a, const cells & b) {
  if (a == b) return "";
  std::string best;
  for (auto & m : rots) {
    cells r;
    for (auto & p : a) r.push_back(rotApply(m, p));
    std::sort(r.begin(), r.end());
    pt t = { b[0][0]-r[0][0], b[0][1]-r[0][1], b[0][2]-r[0][2] };
    bool ok = true;
    for (size_t i = 0; i < r.size() && ok; i++)
      ok = r[i][0]+t[0] == b[i][0] && r[i][1]+t[1] == b[i][1] && r[i][2]+t[2] == b[i][2];
    if (!ok) continue;
    int tr = m[0] + m[4] + m[8];
    std::string d;
    if (tr == 3) d = "shift(" + std::to_string(t[0]) + "," + std::to_string(t[1]) + "," + std::to_string(t[2]) + ")";
    else {
      const char * ang = tr == 1 ? "90" : tr == -1 ? "180" : "120";
      int ax[3] = { m[7]-m[5], m[2]-m[6], m[3]-m[1] };
      std::string axis;
      if (tr == -1) { axis = m[0] == 1 ? "X" : m[4] == 1 ? "Y" : m[8] == 1 ? "Z" : "diag"; }
      else for (int i = 0; i < 3; i++) if (ax[i]) axis += std::string(ax[i] > 0 ? "+" : "-") + "XYZ"[i];
      d = std::string("turn") + ang + axis;
    }
    if (best.empty() || d.rfind("shift", 0) == 0 || (d.find("90") != std::string::npos && best.find("90") == std::string::npos)) best = d;
  }
  return best;
}

static std::unique_ptr<puzzle_c> load(const char * fn) {
  auto str = openGzFile(fn);
  if (!str) { fprintf(stderr, "cannot open %s\n", fn); exit(2); }
  xmlParser_c pars(*str);
  return std::make_unique<puzzle_c>(pars);
}

int main(int argc, char ** argv) {
  if (argc < 2) { fprintf(stderr, "xcheck solutionfile\n"); return 2; }
  makeRots();
  auto puz = load(argv[1]);
  problem_c * pr = puz->getProblem(0);
  const gridType_c * gt = puz->getGridType();
  unsigned int n = pr->getNumberOfSavedSolutions();
  if (!n) { printf("no snapshots\n"); return 1; }

  const assembly_c * first = pr->getSavedSolution(0)->getAssembly();
  home = assemblyConfig(*pr, first);

  auto ctx = rotationContextFor(*pr, defaultRotationSettings());
  disassembler_0_c d(*pr, ctx);
  auto da = d.disassemble(first);
  if (!da) printf("OURS: no disassembly found\n");
  else {
    printf("OURS: level %s, %u rotations\n", da->movesText().c_str(), da->sumRotations());
    walk(da.get(), *pr, first, "S");
    for (auto & l : ourLines) printf("  %s\n", l.c_str());
  }

  printf("CROWELL: %u snapshots\n", n);
  config prev;
  unsigned int matched = 0;
  for (unsigned int i = 0; i < n; i++) {
    const assembly_c * a = pr->getSavedSolution(i)->getAssembly();
    config c = assemblyConfig(*pr, a);
    /* describe the step: per piece translation, or turn */
    std::string step;
    if (i > 0) {
      for (unsigned int k = 0; k < a->placementCount(); k++) {
        const assembly_c * b = pr->getSavedSolution(i-1)->getAssembly();
        if (!b->isPlaced(k)) continue;
        if (!a->isPlaced(k)) { step += std::string(" ") + char('a'+k) + ":out"; continue; }
        if (a->getTransformation(k) != b->getTransformation(k)) step += std::string(" ") + char('a'+k) + ":turn";
      }
    }
    canon_t k = canon(c);
    auto it = ours.find(k);
    std::string m = it == ours.end() ? nearest(k) : it->second.where + (it->second.rotation ? " (rot)" : "");
    if (it != ours.end()) matched++;
    else if (partial.count(k)) { m = partial[k]; matched++; }
    std::string mv;
    if (i > 0) {
      config common;
      for (auto & kv : prev) if (c.count(kv.first)) common[kv.first] = kv.second;
      canon_t pk = canon(common);
      for (auto & [pc, cl] : k)
        for (auto & [pc2, cl2] : pk)
          if (pc == pc2) { std::string x = motion(cl2, cl); if (!x.empty()) mv += std::string(" ") + char('a'+pc) + ":" + x; }
    }
    std::string from;
    if (getenv("FROM")) {
      bool hasAll = true;
      for (unsigned int q = 0; q < a->placementCount(); q++) hasAll = hasAll && a->isPlaced(q);
      if (hasAll || getenv("PARTIAL")) { auto d2 = d.disassemble(a); from = d2 ? " [from here: " + d2->movesText() + "]" : " [from here: none]"; }
    }
    printf("  %2u {%s}%-18s ours: %-40s%s%s\n", i, pieceSet(c).c_str(), step.c_str(), m.c_str(), mv.c_str(), from.c_str());
    prev = c;
  }
  printf("MATCHED %u/%u\n", matched, n);
  (void)gt;
  return 0;
}
