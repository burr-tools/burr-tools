// Re-check rotations PR #50 accepted (ROTJSON lines) with the exact checker:
// is some path of our family clear, does any planar motion exist (grid search,
// certified), or none at all?
const G = require('./geom.js');
const fs = require('fs');
const M = { play: +(process.argv[3] || 0.081), rounding: 0, chamfer: 0 };
const fam = s => G.family(s, [0.1, 0.2, 0.3, 0.4]).map(G.makePath);
const FAM = [fam(0), fam(1)];
const big = s => G.family(s, [0.5, 0.6, 0.7, 0.8]).slice(1).map(G.makePath);
const BIG = [big(0), big(1)];

const inPlane = (c, a) => a === 0 ? [c[1], c[2]] : a === 1 ? [c[2], c[0]] : [c[0], c[1]];
const axial = (c, a) => c[a];
const key = c => c.join(',');

// multi-layer scene: one planar scene per axial layer that has moving cubes
function multiScene(r) {
  const piv = inPlane(r.p.map(x => x / 2), r.a);
  const layers = {};
  for (const c of r.m) (layers[axial(c, r.a)] ||= { m: [], s: [] }).m.push(inPlane(c, r.a));
  for (const c of r.o) { const L = layers[axial(c, r.a)]; if (L) L.s.push(inPlane(c, r.a)); }
  const scs = Object.values(layers).filter(L => L.s.length).map(L => G.scene(L.m, L.s, piv, M));
  const rho = Math.max(0, ...Object.values(layers).map(L => G.scene(L.m, [], piv, M).rho));
  return { scs, rho, piv, layers };
}
const clearance = (ms, p) => Math.min(Infinity, ...ms.scs.map(sc => G.clearance(sc, p).cl));
function certify(ms, path) {
  // same algorithm as G.certify, minimum over layers
  let verdict = 'clear';
  for (let k = 0; k + 1 < path.length; k++) {
    const a = path[k], b = path[k + 1];
    const L = Math.hypot(b.du - a.du, b.dv - a.dv) + ms.rho * Math.abs(b.theta - a.theta);
    const todo = [[0, 1]];
    while (todo.length) {
      const [s0, s1] = todo.pop(), sm = 0.5 * (s0 + s1);
      const cl = clearance(ms, { theta: a.theta + sm * (b.theta - a.theta), du: a.du + sm * (b.du - a.du), dv: a.dv + sm * (b.dv - a.dv) });
      if (cl < 0) return 'blocked';
      if (cl >= 0.5 * (s1 - s0) * L) continue;
      if ((s1 - s0) * L < 1e-7) { verdict = 'undecided'; continue; }
      todo.push([s0, sm], [sm, s1]);
    }
  }
  return verdict;
}
// which sense maps m onto e (our sense 0 = +90 right-handed in the in-plane frame)
function senseOf(r) {
  const piv = inPlane(r.p.map(x => x / 2), r.a);
  const E = new Set(r.e.map(key));
  for (const s of [0, 1]) {
    const ok = r.m.every(c => {
      const [u, v] = inPlane(c, r.a), du = u - piv[0], dv = v - piv[1];
      const [nu, nv] = s === 0 ? [piv[0] - dv, piv[1] + du] : [piv[0] + dv, piv[1] - du];
      const n = c.slice();
      if (r.a === 0) { n[1] = nu; n[2] = nv; } else if (r.a === 1) { n[2] = nu; n[0] = nv; } else { n[0] = nu; n[1] = nv; }
      return E.has(key(n.map(Math.round)));
    });
    if (ok) return s;
  }
  return -1;
}
// grid search over (theta, du, dv), certified shortcut path
function plan(ms, sense) {
  const NT = 60, DD = 0.025, ND = 28, W = 2 * ND + 1, sg = sense === 0 ? 1 : -1;
  const th = t => sg * (Math.PI / 2) * t / NT;
  const idx = (t, a, b) => (t * W + a + ND) * W + b + ND;
  const st = new Int8Array((NT + 1) * W * W), prev = new Int32Array((NT + 1) * W * W).fill(-2);
  const free = (t, a, b) => { const i = idx(t, a, b); if (!st[i]) st[i] = clearance(ms, { theta: th(t), du: a * DD, dv: b * DD }) >= 0.002 ? 1 : 2; return st[i] === 1; };
  const s = idx(0, 0, 0), g = idx(NT, 0, 0);
  prev[s] = -1; let q = [s];
  while (q.length) {
    const nq = [];
    for (const i of q) {
      if (i === g) {
        const pts = []; let j = i;
        while (j >= 0) { pts.push({ theta: th(Math.floor(j / W / W)), du: (Math.floor(j / W) % W - ND) * DD, dv: (j % W - ND) * DD }); j = prev[j]; }
        pts.reverse();
        const out = [pts[0]]; let k = 0;
        while (k < pts.length - 1) {
          let best = -1;
          for (let m = pts.length - 1; m > k; m--) if (certify(ms, [pts[k], pts[m]]) === 'clear') { best = m; break; }
          if (best < 0) return { found: true, certified: false };
          out.push(pts[best]); k = best;
        }
        let maxd = 0; for (const p of out) maxd = Math.max(maxd, Math.hypot(p.du, p.dv));
        return { found: true, certified: true, maxd: +maxd.toFixed(3), path: out.map(p => [+(p.theta * 180 / Math.PI).toFixed(1), +p.du.toFixed(3), +p.dv.toFixed(3)]) };
      }
      const t = Math.floor(i / W / W), a = Math.floor(i / W) % W - ND, b = i % W - ND;
      for (const [dt, da, db] of [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]]) {
        const T = t + dt, A = a + da, B = b + db;
        if (T < 0 || T > NT || Math.abs(A) > ND || Math.abs(B) > ND) continue;
        const j = idx(T, A, B); if (prev[j] !== -2) continue;
        if (!free(T, A, B)) { prev[j] = -3; continue; }
        prev[j] = i; nq.push(j);
      }
    }
    q = nq;
  }
  return { found: false };
}

module.exports = { FAM, multiScene, certify, senseOf, plan };
if (require.main === module) {
const rots = fs.readFileSync(process.argv[2], 'utf8').split('\n').filter(l => l.startsWith('ROTJSON ')).map(l => JSON.parse(l.slice(8)));
const stat = { total: rots.length, noSense: 0, family: 0, bigAmount: 0, otherMotion: 0, gridUncertified: 0, noMotion: 0 };
const examples = { otherMotion: [], noMotion: [], bigAmount: [] };
for (const r of rots) {
  const s = senseOf(r);
  if (s < 0) { stat.noSense++; continue; }
  const ms = multiScene(r);
  if (FAM[s].some(p => certify(ms, p) === 'clear')) { stat.family++; continue; }
  if (BIG[s].some(p => certify(ms, p) === 'clear')) { stat.bigAmount++; examples.bigAmount.push({ p: r.p, a: r.a, s }); continue; }
  const pl = plan(ms, s);
  if (pl.found && pl.certified) { stat.otherMotion++; examples.otherMotion.push({ p: r.p, a: r.a, s, maxd: pl.maxd, path: pl.path, nm: r.m.length }); }
  else if (pl.found) stat.gridUncertified++;
  else { stat.noMotion++; examples.noMotion.push({ p: r.p, a: r.a, s, nm: r.m.length, minClearanceAtStart: +clearance(ms, { theta: 0, du: 0, dv: 0 }).toFixed(3) }); }
}
console.log(JSON.stringify(stat));
for (const k in examples) console.log(k, examples[k].length, JSON.stringify(examples[k].slice(0, 4)));
}
