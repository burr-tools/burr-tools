// Which extended families rescue the rotations our family misses?
const G = require('./geom.js');
const fs = require('fs');
const { FAM, multiScene, certify, senseOf } = require('./audit.js');
const rots = fs.readFileSync(process.argv[2], 'utf8').split('\n').filter(l => l.startsWith('ROTJSON ')).map(l => JSON.parse(l.slice(8)));
const step = +(process.argv[4] || 0.05);
// shift vectors on a grid, |d| <= 0.45
const vecs = [];
for (let a = -0.45; a <= 0.45 + 1e-9; a += step) for (let b = -0.45; b <= 0.45 + 1e-9; b += step) { const n = Math.hypot(a, b); if (n > 1e-9 && n <= 0.45 + 1e-9) vecs.push([a, b]); }
const end = s => s === 0 ? Math.PI / 2 : -Math.PI / 2;
const bulge = th => (Math.cos(th) + Math.sin(th) - 1) / (Math.SQRT2 - 1);
const kinds = {
  slide: (s, [a, b]) => [{ theta: 0, du: 0, dv: 0 }, { theta: 0, du: a, dv: b }, { theta: end(s), du: a, dv: b }, { theta: end(s), du: 0, dv: 0 }],
  wiggle: (s, [a, b]) => { const p = []; for (let k = 0; k <= 16; k++) { const t = k / 16, w = bulge(Math.PI / 2 * t); p.push({ theta: end(s) * t, du: w * a, dv: w * b }); } p[16] = { theta: end(s), du: 0, dv: 0 }; return p; },
  // shift while turning the first quarter of the angle, hold, shift back over the last quarter
  plateau: (s, [a, b]) => [{ theta: 0, du: 0, dv: 0 }, { theta: end(s) / 4, du: a, dv: b }, { theta: 3 * end(s) / 4, du: a, dv: b }, { theta: end(s), du: 0, dv: 0 }],
};
const res = { missed: 0 }; for (const k in kinds) res[k] = 0; res.any = 0;
for (const r of rots) {
  const s = senseOf(r); const ms = multiScene(r);
  if (FAM[s].some(p => certify(ms, p) === 'clear')) continue;
  res.missed++;
  let any = false;
  for (const k in kinds) if (vecs.some(v => certify(ms, kinds[k](s, v)) === 'clear')) { res[k]++; any = true; }
  if (any) res.any++;
}
console.log('grid step', step, 'vectors', vecs.length, JSON.stringify(res));
