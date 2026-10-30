const G = require('./geom.js');
const seed = +process.argv[2], N = +process.argv[3], which = process.argv[4];
const MODELS = { sharp: { play: 0.081, rounding: 0, chamfer: 0 }, pcad: { play: 0.0126, rounding: 0, chamfer: 0.0317 } };
const M = MODELS[which];
const h = 0.5 * (1 - M.play), k = M.chamfer;
const adv = th => Math.max(h * Math.cos(th) + (h - k) * Math.sin(th), (h - k) * Math.cos(th) + h * Math.sin(th)) - h;
const A1 = adv(Math.PI / 4), A2 = A1 + 0.5 * (Math.SQRT2 - 1);
const b = th => adv(th) / A1;
const SEG = 24;
function path(dir, a) {
  const len = Math.hypot(...dir), p = [];
  for (let i = 0; i <= SEG; i++) { const th = Math.PI / 2 * i / SEG, s = a * b(th); p.push({ theta: th, du: s * dir[0] / len, dv: s * dir[1] / len }); }
  p[SEG] = { theta: Math.PI / 2, du: 0, dv: 0 };
  return p;
}
const SETS = { grid: [0.1, 0.2, 0.3, 0.4], natural: [A1, A2], natural4: [A1 - 0.05, A1, A2 - 0.05, A2], grid6: [0.1, 0.2, 0.3, 0.4, 0.5, 0.6] };
const fams = {}; for (const n in SETS) fams[n] = SETS[n].flatMap(a => G.DIRS.map(d => path(d, a)));
const pure = G.makePath({ kind: 'pure', sense: 0 });
let rng = seed >>> 0;
const R = () => { rng = (rng + 0x6D2B79F5) >>> 0; let t = rng; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
const key = (u, v) => u + ',' + v;
function randomGroup(n) { const cells = [[0, 0]], set = new Set([key(0, 0)]); while (cells.length < n) { const [u, v] = cells[Math.floor(R() * cells.length)]; const d = [[1,0],[-1,0],[0,1],[0,-1]][Math.floor(R() * 4)]; const c = [u + d[0], v + d[1]]; if (!set.has(key(...c))) { set.add(key(...c)); cells.push(c); } } return cells; }
const res = { pureOK: 0 }; for (const n in SETS) res[n] = 0;
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
  for (const n in fams) if (fams[n].some(p => G.certify(sc, p) === 'clear')) res[n]++;
}
console.log(JSON.stringify({ which, A1: +A1.toFixed(4), A2: +A2.toFixed(4), res }));
