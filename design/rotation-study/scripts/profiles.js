const G = require('./geom.js');
const seed = +process.argv[2], N = +process.argv[3], which = process.argv[4];
const MODELS = { sharp: { play: 0.081, rounding: 0, chamfer: 0 }, pcad: { play: 0.0126, rounding: 0, chamfer: 0.0317 } };
const M = MODELS[which];
const h = 0.5 * (1 - M.play), k = M.chamfer;
// leading-face advance of the turning cube, as a function of theta (0 at both ends)
const adv = th => Math.max(h * Math.cos(th) + (h - k) * Math.sin(th), (h - k) * Math.cos(th) + h * Math.sin(th)) - h;
const advPeak = adv(Math.PI / 4);
const b = th => adv(th) / advPeak;          // model-aware bulge, peak 1
const AM = [0.1, 0.2, 0.3, 0.4];
const SEG = 24;
const prof = {
  sine: (th, a) => a * Math.sin(2 * th),
  bulge: (th, a) => a * b(th),
  contact: (th, a) => Math.max(0, (a + M.play) * b(th) - M.play),
  'clip.2': (th, a) => a * Math.max(0, (b(th) - 0.2) / 0.8),
  'clip.4': (th, a) => a * Math.max(0, (b(th) - 0.4) / 0.6),
  'clip.6': (th, a) => a * Math.max(0, (b(th) - 0.6) / 0.4),
};
function path(P, dir, a) {
  const len = Math.hypot(...dir), p = [];
  for (let i = 0; i <= SEG; i++) { const th = Math.PI / 2 * i / SEG, s = P(th, a); p.push({ theta: th, du: s * dir[0] / len, dv: s * dir[1] / len }); }
  p[SEG] = { theta: Math.PI / 2, du: 0, dv: 0 };
  return p;
}
const fams = {}; for (const n in prof) fams[n] = AM.flatMap(a => G.DIRS.map(d => path(prof[n], d, a)));
const pure = G.makePath({ kind: 'pure', sense: 0 });
let rng = seed >>> 0;
const R = () => { rng = (rng + 0x6D2B79F5) >>> 0; let t = rng; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
const key = (u, v) => u + ',' + v;
function randomGroup(n) { const cells = [[0, 0]], set = new Set([key(0, 0)]); while (cells.length < n) { const [u, v] = cells[Math.floor(R() * cells.length)]; const d = [[1,0],[-1,0],[0,1],[0,-1]][Math.floor(R() * 4)]; const c = [u + d[0], v + d[1]]; if (!set.has(key(...c))) { set.add(key(...c)); cells.push(c); } } return cells; }
const res = { scenes: N, pureOK: 0, any: 0 }; for (const n in prof) res[n] = 0;
const pair = {};
for (let it = 0; it < N; it++) {
  const mv = randomGroup(1 + Math.floor(R() * 5));
  const us = mv.map(c => c[0]), vs = mv.map(c => c[1]), corner = R() < 0.5;
  const pick = (lo, hi) => corner ? Math.floor(lo - 0.5 + R() * (hi - lo + 2)) + 0.5 : lo + Math.floor(R() * (hi - lo + 1));
  const pv = [pick(Math.min(...us), Math.max(...us)), pick(Math.min(...vs), Math.max(...vs))];
  const occ = new Set(mv.map(c => key(...c)));
  mv.forEach(([u, v]) => occ.add(key(pv[0] - (v - pv[1]), pv[1] + (u - pv[0]))));
  const st = []; const dens = 0.2 + 0.4 * R();
  for (let u = Math.min(...us) - 3; u <= Math.max(...us) + 3; u++) for (let v = Math.min(...vs) - 3; v <= Math.max(...vs) + 3; v++) if (!occ.has(key(u, v)) && R() < dens) st.push([u, v]);
  const sc = G.scene(mv, st, pv, M);
  if (G.certify(sc, pure) === 'clear') { res.pureOK++; continue; }
  const ok = {}; for (const n in fams) { ok[n] = fams[n].some(p => G.certify(sc, p) === 'clear'); if (ok[n]) res[n]++; }
  if (Object.values(ok).some(x => x)) res.any++;
  for (const a in ok) for (const c in ok) if (ok[a] && !ok[c]) pair[a + '>' + c] = (pair[a + '>' + c] || 0) + 1;
}
console.log(JSON.stringify({ which, res, pair }));
