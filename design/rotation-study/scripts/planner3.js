const G = require('./geom.js');
const M = { play: 0.081, rounding: 0, chamfer: 0 };
const AM = [0.1, 0.2, 0.3, 0.4];
const fam = G.family(0, AM);
const famPaths = fam.map(G.makePath);
let rng = (+(process.argv[2] || 5)) >>> 0;
const R = () => { rng = (rng + 0x6D2B79F5) >>> 0; let t = rng; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
// extended families: larger wiggle/slide amounts, and arcs about an edge midpoint (turn about Q, then shift back onto P's end pose)
const bigPaths = G.family(0, [0.5, 0.6, 0.7, 0.8]).slice(1).map(G.makePath);
const arcPaths = [];
for (const q of [[0.5,0],[-0.5,0],[0,0.5],[0,-0.5],[0.5,0.5],[0.5,-0.5],[-0.5,0.5],[-0.5,-0.5]].filter(q => Math.abs(q[0]) + Math.abs(q[1]) === 0.5)) {
  // pose about P that equals turning by theta about P+q: d = (I - R) q
  const pts = [];
  for (let k = 0; k <= 16; k++) { const th = Math.PI / 2 * k / 16, c = Math.cos(th), s = Math.sin(th); pts.push({ theta: th, du: q[0] - (c * q[0] - s * q[1]), dv: q[1] - (s * q[0] + c * q[1]) }); }
  pts.push({ theta: Math.PI / 2, du: 0, dv: 0 });
  arcPaths.push(pts);
  // and the reverse order: shift first, then turn about the edge midpoint into place
  const rev = [{ theta: 0, du: 0, dv: 0 }];
  const e = pts[16];
  for (let k = 0; k <= 16; k++) { const th = Math.PI / 2 * k / 16, c = Math.cos(th), s = Math.sin(th); rev.push({ theta: th, du: -e.du + q[0] - (c * q[0] - s * q[1]), dv: -e.dv + q[1] - (s * q[0] + c * q[1]) }); }
  rev[rev.length - 1] = { theta: Math.PI / 2, du: 0, dv: 0 };
  // rev ends at -e + e = 0 only if arc ends at e: fix start shift
  rev.splice(1, 0, { theta: 0, du: -e.du, dv: -e.dv }); rev.splice(2, 1);
  arcPaths.push(rev);
}

const key = (u, v) => u + ',' + v;

function randomGroup(n) {
  const cells = [[0, 0]], set = new Set([key(0, 0)]);
  while (cells.length < n) {
    const [u, v] = cells[Math.floor(R() * cells.length)];
    const d = [[1,0],[-1,0],[0,1],[0,-1]][Math.floor(R() * 4)];
    const c = [u + d[0], v + d[1]];
    if (!set.has(key(...c))) { set.add(key(...c)); cells.push(c); }
  }
  return cells;
}

// grid BFS over (theta, du, dv); returns certified polyline or null
const DT = 90 / 60, DD = 0.025, DMAX = 0.7;
const ND = Math.round(DMAX / DD);
function plan(sc) {
  const NT = 60, W = 2 * ND + 1;
  const idx = (t, a, b) => (t * W + (a + ND)) * W + (b + ND);
  const free = new Int8Array((NT + 1) * W * W); // 0 unknown 1 free 2 blocked
  const isFree = (t, a, b) => {
    const i = idx(t, a, b);
    if (!free[i]) free[i] = G.clearance(sc, { theta: (t * DT) * Math.PI / 180, du: a * DD, dv: b * DD }).cl >= 0.002 ? 1 : 2;
    return free[i] === 1;
  };
  const prev = new Int32Array((NT + 1) * W * W).fill(-2);
  const s = idx(0, 0, 0), goal = idx(NT, 0, 0);
  if (!isFree(0, 0, 0)) return null;
  prev[s] = -1; let q = [s];
  while (q.length) {
    const nq = [];
    for (const i of q) {
      if (i === goal) {
        const pts = []; let j = i;
        while (j >= 0) { const b = j % W - ND, a = Math.floor(j / W) % W - ND, t = Math.floor(j / W / W); pts.push({ theta: t * DT * Math.PI / 180, du: a * DD, dv: b * DD }); j = prev[j]; }
        pts.reverse();
        // shortcut greedily with the certified checker
        const out = [pts[0]]; let k = 0;
        while (k < pts.length - 1) {
          let best = k + 1;
          for (let m = pts.length - 1; m > k + 1; m -= Math.max(1, Math.floor((m - k) / 8))) if (G.certify(sc, [pts[k], pts[m]]) === 'clear') { best = m; break; }
          if (best === k + 1 && G.certify(sc, [pts[k], pts[k + 1]]) !== 'clear') return { pts, certified: false };
          out.push(pts[best]); k = best;
        }
        return { pts: out, certified: true };
      }
      const t = Math.floor(i / W / W), a = Math.floor(i / W) % W - ND, b = i % W - ND;
      for (const [dt, da, db] of [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]]) {
        const T = t + dt, A = a + da, B = b + db;
        if (T < 0 || T > NT || Math.abs(A) > ND || Math.abs(B) > ND) continue;
        const j = idx(T, A, B);
        if (prev[j] !== -2) continue;
        if (!isFree(T, A, B)) { prev[j] = -3; continue; }
        prev[j] = i; nq.push(j);
      }
    }
    q = nq;
  }
  return null;
}

const N = +(process.argv[3] || 400);
// can a unit translation plus a family turn (either order) reach the same end pose?
const famAll = famPaths.concat([]);
function famClear(mv, st, pv) { const sc = G.scene(mv, st, pv, M); return famAll.some(p => G.certify(sc, p) === 'clear'); }
function composite(mv, st, pv) {
  const stat = new Set(st.map(c => key(...c)));
  for (const t of [[1,0],[-1,0],[0,1],[0,-1]]) {
    // translate first: group at mv + t, pivot P' = P - (I-R)^-1 R t, (I-R)^-1 = 1/2 [[1,-1],[1,1]], R t = (-t1, t0)
    const rt = [-t[1], t[0]];
    const a = [0.5 * (rt[0] - rt[1]), 0.5 * (rt[0] + rt[1])];
    const moved = mv.map(([u, v]) => [u + t[0], v + t[1]]);
    if (moved.every(c => !stat.has(key(...c))) && famClear(moved, st, [pv[0] - a[0], pv[1] - a[1]])) return 'slide-then-turn';
    // turn first about P'' = P - (I-R)^-1 t, then translate by t
    const b = [0.5 * (t[0] - t[1]), 0.5 * (t[0] + t[1])];
    if (famClear(mv, st, [pv[0] - b[0], pv[1] - b[1]])) return 'turn-then-slide';
  }
  return null;
}
const stats = { bigOnly: 0, arcOnly: 0, bigOrArc: 0, cand: 0, pure: 0, wiggleOnly: 0, slideOnly: 0, both: 0, famFail: 0, planFound: 0, uncert: 0 };
const missed = [];
for (let it = 0; it < N; it++) {
  const mv = randomGroup(2 + Math.floor(R() * 4));
  const corner = R() < 0.5;
  const us = mv.map(c => c[0]), vs = mv.map(c => c[1]);
  const pv = [Math.min(...us) - 0.5 + Math.floor(R() * (Math.max(...us) - Math.min(...us) + 2)) + (corner ? 0 : 0.5) - (corner ? 0 : 0) , 0];
  pv[0] = corner ? Math.floor(Math.min(...us) - 0.5 + R() * (Math.max(...us) - Math.min(...us) + 2)) + 0.5 : Math.min(...us) + Math.floor(R() * (Math.max(...us) - Math.min(...us) + 1));
  pv[1] = corner ? Math.floor(Math.min(...vs) - 0.5 + R() * (Math.max(...vs) - Math.min(...vs) + 2)) + 0.5 : Math.min(...vs) + Math.floor(R() * (Math.max(...vs) - Math.min(...vs) + 1));
  const occ = new Set(mv.map(c => key(...c)));
  const endc = mv.map(([u, v]) => [pv[0] - (v - pv[1]), pv[1] + (u - pv[0])]);
  endc.forEach(e => occ.add(key(...e)));
  const st = [];
  const dens = 0.25 + 0.35 * R();
  for (let u = Math.min(...us) - 3; u <= Math.max(...us) + 3; u++) for (let v = Math.min(...vs) - 3; v <= Math.max(...vs) + 3; v++)
    if (!occ.has(key(u, v)) && R() < dens) st.push([u, v]);
  const sc = G.scene(mv, st, pv, M);
  stats.cand++;
  const ver = famPaths.map(p => G.certify(sc, p) === 'clear');
  if (ver[0]) { stats.pure++; continue; }
  const w = ver.some((x, i) => x && fam[i].kind === 'wiggle'), s = ver.some((x, i) => x && fam[i].kind === 'slide');
  if (w && s) { stats.both++; continue; }
  if (w) { stats.wiggleOnly++; continue; }
  if (s) { stats.slideOnly++; continue; }
  stats.famFail++;
  const big = bigPaths.some(p => G.certify(sc, p) === 'clear');
  const arc = arcPaths.some(p => G.certify(sc, p) === 'clear');
  if (big || arc) { stats.bigOrArc++; if (big && !arc) stats.bigOnly++; if (arc && !big) stats.arcOnly++; }
  const r = plan(sc);
  if (r && r.certified && (big || arc)) { stats.planFound++; stats.planAlsoExt = (stats.planAlsoExt || 0) + 1; continue; }
  if (r && r.certified) {
    stats.planFound++;
    // characterise: max shift, shift at theta ~ 0 / 90, direction spread
    const P = r.pts;
    let maxd = 0, dirs = [];
    for (let i = 0; i <= 200; i++) { const p = G.poseAt(P, i / 200); const d = Math.hypot(p.du, p.dv); maxd = Math.max(maxd, d); if (d > 0.03) dirs.push(Math.atan2(p.dv, p.du)); }
    let spread = 0; for (const a of dirs) for (const b of dirs) { let x = Math.abs(a - b); x = Math.min(x, 2 * Math.PI - x); spread = Math.max(spread, x); }
    // shift while theta is still < 10deg or > 80deg (slide-like), max shift reached at which theta
    let atMax = 0, edge = 0;
    for (let i = 0; i <= 200; i++) { const p = G.poseAt(P, i / 200); const d = Math.hypot(p.du, p.dv); if (d >= maxd - 1e-9) atMax = p.theta * 180 / Math.PI; if ((p.theta < 0.17 || p.theta > 1.4) && d > 0.1) edge = 1; }
    const comp = composite(mv, st, pv);
    if (comp) { stats.composite = (stats.composite || 0) + 1; stats[comp] = (stats[comp] || 0) + 1; continue; }
    missed.push({ maxd: +maxd.toFixed(3), spreadDeg: Math.round(spread * 180 / Math.PI), thetaAtMax: Math.round(atMax), edgeShift: edge, n: mv.length, mv, stc: st, pv, path: P.map(p => [+(p.theta*180/Math.PI).toFixed(1), +p.du.toFixed(3), +p.dv.toFixed(3)]) });
  } else if (r) stats.uncert++;
}
console.log(JSON.stringify(stats));
console.log(JSON.stringify(missed));
