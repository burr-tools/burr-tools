const G = require('./geom.js');
const M = { play: 0.081, rounding: 0, chamfer: 0 };
// profile comparison: sine bump vs the cube's own bulge, normalised to peak 1
let maxdiff = 0, at = 0;
for (let i = 0; i <= 900; i++) { const th = Math.PI / 2 * i / 900; const a = Math.sin(2 * th), b = (Math.cos(th) + Math.sin(th) - 1) / (Math.SQRT2 - 1); if (Math.abs(a - b) > maxdiff) { maxdiff = Math.abs(a - b); at = th * 180 / Math.PI; } }
console.log('max |sin2θ - bulge| =', maxdiff.toFixed(3), 'at', at.toFixed(1), '°');
const AM = [0.1, 0.2, 0.3, 0.4];
const DIRS = G.DIRS;
const profiles = {
  sine: t => Math.sin(Math.PI * t),
  bulge: t => { const th = Math.PI / 2 * t; return (Math.cos(th) + Math.sin(th) - 1) / (Math.SQRT2 - 1); },
  tent: t => 1 - Math.abs(2 * t - 1),
  plateau: t => Math.min(1, 3 * Math.min(t, 1 - t)),   // out by t=1/3, hold, back from 2/3
};
function wig(prof, dir, a) {
  const len = Math.hypot(...dir), su = a * dir[0] / len, sv = a * dir[1] / len, p = [];
  for (let k = 0; k <= 16; k++) { const t = k / 16, w = prof(t); p.push({ theta: Math.PI / 2 * t, du: w * su, dv: w * sv }); }
  p[16] = { theta: Math.PI / 2, du: 0, dv: 0 };
  return p;
}
const fams = {};
for (const [n, f] of Object.entries(profiles)) fams[n] = AM.flatMap(a => DIRS.map(d => wig(f, d, a)));
const pure = G.makePath({ kind: 'pure', sense: 0 });
let rng = 777 >>> 0;
const R = () => { rng = (rng + 0x6D2B79F5) >>> 0; let t = rng; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
const key = (u, v) => u + ',' + v;
function randomGroup(n) { const cells = [[0, 0]], set = new Set([key(0, 0)]); while (cells.length < n) { const [u, v] = cells[Math.floor(R() * cells.length)]; const d = [[1,0],[-1,0],[0,1],[0,-1]][Math.floor(R() * 4)]; const c = [u + d[0], v + d[1]]; if (!set.has(key(...c))) { set.add(key(...c)); cells.push(c); } } return cells; }
const res = { pureOK: 0 }; for (const n in fams) res[n] = 0;
const only = {}; let union = 0;
const N = 6000;
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
  const ok = {};
  for (const n in fams) { ok[n] = fams[n].some(p => G.certify(sc, p) === 'clear'); if (ok[n]) res[n]++; }
  if (Object.values(ok).some(x => x)) union++;
  for (const n in fams) if (ok[n] && Object.entries(ok).every(([m, v]) => m === n || !v)) only[n] = (only[n] || 0) + 1;
  if (ok.sine !== ok.bulge) only['sine xor bulge'] = (only['sine xor bulge'] || 0) + 1;
}
console.log('candidates', N, JSON.stringify(res), 'rescued by any profile', union, 'only by one:', JSON.stringify(only));
