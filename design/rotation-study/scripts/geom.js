// Port of src/lib/rotationgeometry.cpp (planar part): signed distance,
// clearance, certified path checker, path family.
const G = (() => {
  const SQ2 = Math.SQRT2;
  const MIN_DISPLACEMENT = 1e-7;

  function corners(h, k) {
    const sg = [[-1, -1], [1, -1], [1, 1], [-1, 1]];
    const out = [];
    for (const [a, b] of sg) {
      const x = h * a, y = h * b;
      if (k > 0) {
        const hin = a !== b;
        out.push(hin ? [x - a * k, y] : [x, y - b * k]);
        out.push(hin ? [x, y - b * k] : [x - a * k, y]);
      } else out.push([x, y]);
    }
    return out;
  }

  function segPt2(px, py, ax, ay, bx, by) {
    const dx = bx - ax, dy = by - ay;
    const l2 = dx * dx + dy * dy;
    let t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
    t = Math.max(0, Math.min(1, t));
    const qx = ax + t * dx - px, qy = ay + t * dy - py;
    return qx * qx + qy * qy;
  }

  // signed distance between static core at (su,sv) and moving core at (mu,mv) turned by (c,s)
  function sqSD(su, sv, mu, mv, c, s, h, k) {
    const loc = corners(h, k);
    const sp = loc.map(([x, y]) => [su + x, sv + y]);
    const mp = loc.map(([x, y]) => [mu + x * c - y * s, mv + x * s + y * c]);
    const r = SQ2 / 2;
    const normals = [[1, 0], [0, 1], [c, s], [-s, c], [r, r], [r, -r], [r * (c - s), r * (s + c)], [r * (c + s), r * (s - c)]];
    const nn = k > 0 ? 8 : 4;
    let depth = Infinity, apart = false;
    for (let j = 0; j < nn && !apart; j++) {
      let smin = Infinity, smax = -Infinity, mmin = Infinity, mmax = -Infinity;
      for (let i = 0; i < sp.length; i++) {
        const ds = sp[i][0] * normals[j][0] + sp[i][1] * normals[j][1];
        const dm = mp[i][0] * normals[j][0] + mp[i][1] * normals[j][1];
        smin = Math.min(smin, ds); smax = Math.max(smax, ds);
        mmin = Math.min(mmin, dm); mmax = Math.max(mmax, dm);
      }
      const ov = Math.min(smax, mmax) - Math.max(smin, mmin);
      if (ov <= 0) apart = true; else depth = Math.min(depth, ov);
    }
    if (!apart) return -depth;
    let d2 = Infinity;
    const n = sp.length;
    for (let i = 0; i < n; i++)
      for (let j = 0; j < n; j++) {
        const j2 = (j + 1) % n;
        d2 = Math.min(d2, segPt2(sp[i][0], sp[i][1], mp[j][0], mp[j][1], mp[j2][0], mp[j2][1]));
        d2 = Math.min(d2, segPt2(mp[i][0], mp[i][1], sp[j][0], sp[j][1], sp[j2][0], sp[j2][1]));
      }
    return Math.sqrt(d2);
  }

  // scene: single layer; moving/stat are [u,v] cell centres, pivot [pu,pv] in cell units
  function scene(moving, stat, pivot, model) {
    const half = 0.5 * (1 - model.play) - model.rounding;
    const m = moving.map(([u, v]) => [u - pivot[0], v - pivot[1]]);
    const st = stat.map(([u, v]) => [u - pivot[0], v - pivot[1]]);
    let rho = 0;
    for (const [u, v] of m) rho = Math.max(rho, Math.hypot(u, v) + half * SQ2);
    return { m, st, half, rounding: model.rounding, chamfer: model.chamfer || 0, rho };
  }

  // returns {cl, mi, si}: clearance and the closest pair
  function clearance(sc, p) {
    const c = Math.cos(p.theta), s = Math.sin(p.theta);
    let best = Infinity, mi = -1, si = -1;
    sc.m.forEach(([u, v], a) => {
      const mu = u * c - v * s + p.du, mv = u * s + v * c + p.dv;
      sc.st.forEach(([su, sv], b) => {
        if (Math.hypot(mu - su, mv - sv) > 2 * SQ2 * sc.half + 2 * sc.rounding + best + 1e-9) return;
        const d = sqSD(su, sv, mu, mv, c, s, sc.half, sc.chamfer) - 2 * sc.rounding;
        if (d < best) { best = d; mi = a; si = b; }
      });
    });
    return { cl: best, mi, si };
  }

  // certified checker; log collects {t0,t1,state} over global path parameter
  function certify(sc, path, log) {
    let verdict = 'clear';
    const nseg = path.length - 1;
    for (let k = 0; k < nseg; k++) {
      const a = path[k], b = path[k + 1];
      const L = Math.hypot(b.du - a.du, b.dv - a.dv) + sc.rho * Math.abs(b.theta - a.theta);
      const todo = [[0, 1]];
      while (todo.length) {
        const [s0, s1] = todo.pop();
        const sm = 0.5 * (s0 + s1);
        const p = { theta: a.theta + sm * (b.theta - a.theta), du: a.du + sm * (b.du - a.du), dv: a.dv + sm * (b.dv - a.dv) };
        const cl = clearance(sc, p).cl;
        const g0 = (k + s0) / nseg, g1 = (k + s1) / nseg, gm = (k + sm) / nseg;
        if (cl < 0) { log && log.push({ t0: g0, t1: g1, tm: gm, state: 'hit' }); return 'blocked'; }
        if (cl >= 0.5 * (s1 - s0) * L) { log && log.push({ t0: g0, t1: g1, tm: gm, state: 'ok' }); continue; }
        if ((s1 - s0) * L < MIN_DISPLACEMENT) { verdict = 'undecided'; continue; }
        log && log.push({ t0: g0, t1: g1, tm: gm, state: 'split' });
        todo.push([s0, sm]); todo.push([sm, s1]);
      }
    }
    return verdict;
  }

  const WIGGLE_SEGMENTS = 16;
  function makePath(spec) {
    const end = spec.sense === 0 ? Math.PI / 2 : -Math.PI / 2;
    if (spec.kind === 'pure') return [{ theta: 0, du: 0, dv: 0 }, { theta: end, du: 0, dv: 0 }];
    const len = Math.hypot(spec.dir[0], spec.dir[1]);
    const su = spec.amount * spec.dir[0] / len, sv = spec.amount * spec.dir[1] / len;
    if (spec.kind === 'slide')
      return [{ theta: 0, du: 0, dv: 0 }, { theta: 0, du: su, dv: sv }, { theta: end, du: su, dv: sv }, { theta: end, du: 0, dv: 0 }];
    const path = [];
    for (let k = 0; k <= WIGGLE_SEGMENTS; k++) {
      const t = k / WIGGLE_SEGMENTS, th = Math.PI / 2 * t, w = (Math.cos(th) + Math.sin(th) - 1) / (Math.SQRT2 - 1);
      path.push({ theta: end * t, du: w * su, dv: w * sv });
    }
    path[path.length - 1] = { theta: end, du: 0, dv: 0 };
    return path;
  }

  function poseAt(path, t) {
    if (t <= 0) return path[0];
    if (t >= 1) return path[path.length - 1];
    const n = path.length - 1;
    const seg = Math.min(Math.floor(t * n), n - 1);
    const l = t * n - seg, a = path[seg], b = path[seg + 1];
    return { theta: a.theta + l * (b.theta - a.theta), du: a.du + l * (b.du - a.du), dv: a.dv + l * (b.dv - a.dv) };
  }

  // compass directions counter-clockwise from (1,0)
  const DIRS = [[1, 0], [1, 1], [0, 1], [-1, 1], [-1, 0], [-1, -1], [0, -1], [1, -1]];
  function family(sense, amounts) {
    const f = [{ kind: 'pure', sense }];
    for (const a of amounts)
      for (const kind of ['wiggle', 'slide'])
        for (const d of DIRS) {
          // the -90° tables are mirrored at the u axis
          const dir = sense === 0 ? d : [d[0], -d[1]];
          f.push({ kind, sense, dir, amount: a });
        }
    return f;
  }

  return { sqSD, scene, clearance, certify, makePath, poseAt, family, DIRS };
})();
if (typeof module !== 'undefined') module.exports = G;
