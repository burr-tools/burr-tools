/* Ear clipping of a planar polygon with holes, for the cube grid's lookup
 * mesher (cubemesh.cpp). Extracted from the chamfer tracer's gapmesh.h,
 * where it was written for the same purpose. */
#ifndef TRIANGULATE_H
#define TRIANGULATE_H

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cubeMesh {

  /* triangulate a polygon with holes: pts are 2D points with .x/.y,
   * loops[0] is the outer loop (CCW), the others are holes (CW), each
   * an index list into pts. Holes are bridged into the outer loop
   * (a keyhole per hole: from the hole's rightmost vertex to a visible
   * polygon vertex, Eberly's rule), then ear clipped. The keyhole
   * polygon is weakly simple: vertices and whole edges repeat along
   * the bridges, and loops may touch at vertices, so every test is by
   * polygon position, and an ear is valid only when no edge of the
   * remaining polygon passes through the open ear triangle and the
   * ear lies inside the polygon. Tolerances are distances (TOL, in
   * the points' units): mesh vertices carry ~1e-7 of noise. Output:
   * index triples into pts. Returns false (with a dump on stderr) when
   * a hole is not inside the outline or clipping gets stuck. */
  template <class P2>
  bool triangulateLoops(const std::vector<P2> & pts,
                        const std::vector<std::vector<int> > & loops,
                        std::vector<int> & tris) {
    tris.clear();
    if (loops.empty() || loops[0].size() < 3) return true;
    const double TOL = 1e-6;
    std::vector<int> poly = loops[0];
    bool ok = true; const char * why = "";
    struct geo_s {
      const std::vector<P2> & pts; double TOL;
      double X(int v) const { return pts[v].x; }
      double Y(int v) const { return pts[v].y; }
      bool same(double ax, double ay, double bx, double by) const {
        return fabs(ax - bx) < TOL && fabs(ay - by) < TOL;
      }
      /* signed distance of (px,py) from the line a->b (positive left) */
      double sdist(double ax, double ay, double bx, double by, double px, double py) const {
        double sx = bx - ax, sy = by - ay, l = sqrt(sx * sx + sy * sy);
        if (l < 1e-300) return sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay));
        return (sx * (py - ay) - sy * (px - ax)) / l;
      }
      /* does the interior wedge of polygon position q (prev p, next n)
       * contain direction d? Inclusive of the wedge's edges */
      bool wedge(double qx, double qy, double px, double py, double nx, double ny, double dx, double dy) const {
        double nxx = nx - qx, nxy = ny - qy, prx = px - qx, pry = py - qy;
        double ln = sqrt(nxx * nxx + nxy * nxy), lp = sqrt(prx * prx + pry * pry), ld = sqrt(dx * dx + dy * dy);
        if (ln < 1e-300 || lp < 1e-300 || ld < 1e-300) return false;
        nxx /= ln; nxy /= ln; prx /= lp; pry /= lp; dx /= ld; dy /= ld;
        double cnd = nxx * dy - nxy * dx;            /* cross(next dir, d) */
        double cdp = dx * pry - dy * prx;            /* cross(d, prev dir) */
        double cnp = nxx * pry - nxy * prx;          /* cross(next dir, prev dir): >0 convex */
        double dnp = nxx * prx + nxy * pry;
        const double A = 1e-7;
        if (cnp > A || (fabs(cnp) <= A && dnp < 0))  /* convex or straight */
          return cnd > -A && cdp > -A;
        return !(cnd < A && cdp < A);                /* reflex: not in the exterior wedge */
      }
    } G = {pts, TOL};
    /* holes sorted by rightmost vertex, rightmost first */
    std::vector<std::pair<double, int> > order;
    for (unsigned h = 1; h < loops.size(); h++) {
      if (loops[h].size() < 3) continue;
      double mx = -1e300;
      for (unsigned i = 0; i < loops[h].size(); i++) mx = std::max(mx, (double)pts[loops[h][i]].x);
      order.push_back(std::make_pair(-mx, (int)h));
    }
    std::sort(order.begin(), order.end());
    for (unsigned oq = 0; oq < order.size() && ok; oq++) {
      const std::vector<int> & H = loops[order[oq].second];
      int mi = 0;
      for (unsigned i = 1; i < H.size(); i++)
        if (pts[H[i]].x > pts[H[mi]].x) mi = (int)i;
      double Mx = pts[H[mi]].x, My = pts[H[mi]].y;
      int n = (int)poly.size();
      int P = -1;
      /* a hole vertex in the interior of an outline edge (the hole
       * touches the outline there without a vertex: a hull facet
       * pinched by a wedge, rhombic mask 3851 at 5g): split that edge
       * there, so the touch is at a polygon vertex. Every hole vertex,
       * not just M: a tiny hole touching the outline at a vertex other
       * than its rightmost one left a T-junction the clipper got stuck
       * on (two removed stars 1.9e-3 apart, tetoct cells 3,2,3;4,1,4's
       * complement at 2g) */
      for (unsigned hv = 0; hv < H.size(); hv++) {
        double Hx = pts[H[hv]].x, Hy = pts[H[hv]].y;
        for (int e = 0; e < n; e++) {
          int a = poly[e], b = poly[(e+1) % n];
          double ax = G.X(a), ay = G.Y(a), bx = G.X(b), by = G.Y(b);
          double ex = bx - ax, ey = by - ay, el = sqrt(ex * ex + ey * ey);
          if (el < TOL) continue;
          double t = ((Hx - ax) * ex + (Hy - ay) * ey) / (el * el);
          if (t * el <= TOL || (1 - t) * el <= TOL) continue;
          if (fabs(G.sdist(ax, ay, bx, by, Hx, Hy)) >= TOL) continue;
          poly.insert(poly.begin() + (e + 1), H[hv]);
          n++;
          break;
        }
      }
      /* the hole touches the polygon at M itself: a zero-length
       * bridge, spliced at the copy whose wedge holds the hole's
       * outgoing edge (the hole lies inside that copy's interior) */
      {
        double hx = pts[H[(mi + 1) % H.size()]].x, hy = pts[H[(mi + 1) % H.size()]].y;
        for (int q = 0; q < n && P < 0; q++) {
          if (!G.same(G.X(poly[q]), G.Y(poly[q]), Mx, My)) continue;
          int pr = poly[(q + n - 1) % n], nx = poly[(q + 1) % n];
          if (G.wedge(Mx, My, G.X(pr), G.Y(pr), G.X(nx), G.Y(nx), hx - Mx, hy - My)) P = q;
        }
      }
      int hitE = -1; double hitX = 1e300, hitT = 0; bool found = false;
      double Rx = Mx, Ry = My;
      if (P < 0) {
        /* closest edge hit by the ray +x from M. Only edges crossed
         * upward count: the polygon is CCW, so their interior side
         * faces M (the downward twin of a bridge faces away) */
        for (int e = 0; e < n; e++) {
          int a = poly[e], b = poly[(e+1) % n];
          double ax = G.X(a), ay = G.Y(a), bx = G.X(b), by = G.Y(b);
          if (!(ay <= My && My < by)) continue;
          double t = (My - ay) / (by - ay);
          double x = ax + t * (bx - ax);
          if (x < Mx - TOL) continue;
          if (x < hitX) { hitX = x; hitE = e; hitT = t; }
        }
        if (hitE < 0) { ok = false; why = "hole outside the outline"; break; }
        int e0 = hitE, e1 = (hitE + 1) % n;
        double el = sqrt((G.X(poly[e1]) - G.X(poly[e0])) * (G.X(poly[e1]) - G.X(poly[e0])) +
                         (G.Y(poly[e1]) - G.Y(poly[e0])) * (G.Y(poly[e1]) - G.Y(poly[e0])));
        if (hitT * el < TOL) P = e0;               /* ray through a vertex: bridge to it */
        else if ((1 - hitT) * el < TOL) P = e1;
        else P = (G.X(poly[e0]) > G.X(poly[e1])) ? e0 : e1;
        Rx = G.X(poly[P]); Ry = G.Y(poly[P]);
        /* Eberly: a reflex vertex inside the triangle (M, I, P) may
         * block the bridge to P; take the one with the smallest angle
         * to the ray (closest on a tie) instead. Copies of P at its own
         * coordinates are P itself for this purpose. */
        double bestAng = 1e300, bestD = 1e300;
        double x1 = Mx, y1 = My, x2 = hitX, y2 = My, x3 = Rx, y3 = Ry;
        for (int q = 0; q < n; q++) {
          double qx = G.X(poly[q]), qy = G.Y(poly[q]);
          if (qx < Mx - TOL) continue;
          if (G.same(qx, qy, Rx, Ry) || G.same(qx, qy, Mx, My)) continue;
          int pr = poly[(q + n - 1) % n], nx = poly[(q+1) % n];
          if (G.sdist(G.X(pr), G.Y(pr), qx, qy, G.X(nx), G.Y(nx)) >= -TOL) continue;   /* not reflex */
          /* inside the (often sliver) triangle M, I, P: within TOL of
           * every side's inner half plane, and inside its box */
          bool inside = true;
          double ex[3] = {x1, x2, x3}, ey[3] = {y1, y2, y3};
          double area = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
          for (int k = 0; k < 3 && inside; k++) {
            double d = G.sdist(ex[k], ey[k], ex[(k+1)%3], ey[(k+1)%3], qx, qy);
            if (area < 0) d = -d;
            if (d < -TOL) inside = false;
          }
          if (qx > std::max(x2, x3) + TOL || qy < std::min(y1, y3) - TOL || qy > std::max(y1, y3) + TOL) inside = false;
          if (!inside) continue;
          double ang = atan2(fabs(qy - My), qx - Mx);
          double dd = (qx - Mx) * (qx - Mx) + (qy - My) * (qy - My);
          if (ang < bestAng - 1e-9 || (fabs(ang - bestAng) <= 1e-9 && dd < bestD)) {
            bestAng = ang; bestD = dd; found = true; Rx = qx; Ry = qy;
          }
        }
        /* the target coordinates may occur at several polygon positions
         * (bridge twins, touching loops): take the copy whose interior
         * wedge contains the direction towards M */
        int Pp = -1;
        for (int q = 0; q < n; q++) {
          if (!G.same(G.X(poly[q]), G.Y(poly[q]), Rx, Ry)) continue;
          if (!found && q == P) { Pp = q; break; }
          int pr = poly[(q + n - 1) % n], nx = poly[(q + 1) % n];
          if (G.wedge(Rx, Ry, G.X(pr), G.Y(pr), G.X(nx), G.Y(nx), Mx - Rx, My - Ry)) { Pp = q; break; }
        }
        if (Pp < 0) {
          ok = false; why = "no bridge target copy faces the hole";
          fprintf(stderr, "triangulateLoops: hole %d M (%.17g,%.17g) hit edge %d t %.3g P %d found %d R (%.17g,%.17g)\n",
                  order[oq].second, Mx, My, hitE, hitT, P, (int)found, Rx, Ry);
          break;
        }
        P = Pp;
      }
      /* splice: ... P, M, hole around back to M, P, ... */
      std::vector<int> np;
      for (int q = 0; q <= P; q++) np.push_back(poly[q]);
      for (unsigned i = 0; i <= H.size(); i++) np.push_back(H[(mi + i) % H.size()]);
      for (int q = P; q < n; q++) np.push_back(poly[q]);
      poly.swap(np);
    }
    /* ear clipping */
    std::vector<int> I = poly;
    while (ok && I.size() > 3) {
      int m = (int)I.size();
      int pick = -1, drop = -1;
      for (int i = 0; i < m && pick < 0; i++) {
        int a = I[(i + m - 1) % m], b = I[i], c = I[(i + 1) % m];
        double ax = G.X(a), ay = G.Y(a), bx = G.X(b), by = G.Y(b), cx = G.X(c), cy = G.Y(c);
        /* coincident points: a zero-length edge, or the two twins of a
         * bridge meeting again (a == c) - b is dropped, no triangle.
         * A collinear vertex is never dropped: it is a real vertex
         * that neighbouring constructs attach to, so the edges on
         * both sides of it must stay (ears elsewhere consume it) */
        if (drop < 0 && (G.same(ax, ay, bx, by) || G.same(bx, by, cx, cy) || G.same(ax, ay, cx, cy))) { drop = i; continue; }
        /* convex: b more than TOL left of a->c */
        if (G.sdist(ax, ay, cx, cy, bx, by) >= -TOL) continue;   /* reflex or straight */
        bool clear = true;
        /* no vertex on the open diagonal a-c: the diagonal becomes a
         * triangle edge and that vertex would be a T-junction on it
         * (a hole edge collinear with an outline vertex) */
        double acx = cx - ax, acy = cy - ay, acl = sqrt(acx * acx + acy * acy);
        for (int q = 0; q < m && clear; q++) {
          if (q == (i + m - 1) % m || q == i || q == (i + 1) % m) continue;
          double wx = G.X(I[q]), wy = G.Y(I[q]);
          double t = ((wx - ax) * acx + (wy - ay) * acy) / (acl * acl);
          if (t * acl <= TOL || (1 - t) * acl <= TOL) continue;
          if (fabs(G.sdist(ax, ay, cx, cy, wx, wy)) < TOL) clear = false;
        }
        /* no edge of the polygon (other than the two at b) may pass
         * through the open triangle a, b, c: an endpoint strictly
         * inside, or a crossing of the interior (loops running through
         * a sliver ear without a vertex inside it). The triangle a,b,c
         * is CCW (b right of a->c), so inside is left of each side */
        double ex[3] = {ax, bx, cx}, ey[3] = {ay, by, cy};
        for (int q = 0; q < m && clear; q++) {
          if (q == (i + m - 1) % m || q == i) continue;   /* edges a-b and b-c */
          int u = I[q], v = I[(q + 1) % m];
          double ux = G.X(u), uy = G.Y(u), vx = G.X(v), vy = G.Y(v);
          /* parameter interval of u+t(v-u) deeper than TOL inside all three sides */
          double lo = 0, hi = 1;
          for (int k = 0; k < 3 && lo < hi; k++) {
            double du = G.sdist(ex[k], ey[k], ex[(k+1)%3], ey[(k+1)%3], ux, uy);
            double dv = G.sdist(ex[k], ey[k], ex[(k+1)%3], ey[(k+1)%3], vx, vy);
            if (du <= TOL && dv <= TOL) { lo = 1; hi = 0; break; }   /* never inside */
            if (du > TOL && dv > TOL) continue;
            double t = (du - TOL) / (du - dv);       /* crossing of the TOL-inset side */
            if (du > dv) hi = std::min(hi, t); else lo = std::max(lo, t);
          }
          if (lo < hi) clear = false;
        }
        /* and the ear must be inside the polygon: once a hole touching
         * the outline has been clipped around, the leftover can be a
         * zero-area slit whose edges all run twice in opposite
         * directions - an ear there passes the edge tests but covers
         * exterior (rhombic 94375). Winding number of the centroid. */
        if (clear) {
          double gx = (ax + bx + cx) / 3, gy = (ay + by + cy) / 3;
          int wn = 0;
          for (int q = 0; q < m; q++) {
            int u = I[q], v = I[(q + 1) % m];
            double ux = G.X(u), uy = G.Y(u), vx = G.X(v), vy = G.Y(v);
            if (uy <= gy) { if (vy > gy && (vx - ux) * (gy - uy) - (vy - uy) * (gx - ux) > 0) wn++; }
            else if (vy <= gy && (vx - ux) * (gy - uy) - (vy - uy) * (gx - ux) < 0) wn--;
          }
          if (wn != 1) clear = false;
        }
        if (clear) pick = i;
      }
      if (pick < 0) {
        if (drop < 0) {
          /* a remainder with no area is finished: a slit whose edges
           * all run along one line (a hole's bridge doubling back on
           * an outline edge) has nothing left to triangulate (rhombic
           * 395177's complement at 2g: four collinear points remained) */
          double ex0 = G.X(I[0]), ey0 = G.Y(I[0]), far = 0; int fi = 0;
          for (int q = 1; q < m; q++) {
            double dd = (G.X(I[q]) - ex0) * (G.X(I[q]) - ex0) + (G.Y(I[q]) - ey0) * (G.Y(I[q]) - ey0);
            if (dd > far) { far = dd; fi = q; }
          }
          bool flatRem = true;
          for (int q = 0; q < m && flatRem; q++)
            if (fabs(G.sdist(ex0, ey0, G.X(I[fi]), G.Y(I[fi]), G.X(I[q]), G.Y(I[q]))) > TOL) flatRem = false;
          if (flatRem) break;
          ok = false; why = "no valid ear";
          fprintf(stderr, "triangulateLoops: remaining polygon:");
          for (int q = 0; q < m; q++) fprintf(stderr, " (%.17g,%.17g)", G.X(I[q]), G.Y(I[q]));
          fprintf(stderr, "\n");
          break;
        }
        I.erase(I.begin() + drop);
        continue;
      }
      int a = I[(pick + m - 1) % m], b = I[pick], c = I[(pick + 1) % m];
      tris.push_back(a); tris.push_back(b); tris.push_back(c);
      I.erase(I.begin() + pick);
    }
    if (ok && I.size() == 3) {
      if (G.sdist(G.X(I[0]), G.Y(I[0]), G.X(I[2]), G.Y(I[2]), G.X(I[1]), G.Y(I[1])) < -TOL) {
        tris.push_back(I[0]); tris.push_back(I[1]); tris.push_back(I[2]);
      }
    }
    /* invariant: the triangles cover exactly the loops' area */
    if (ok) {
      double la = 0, ta = 0;
      for (unsigned l = 0; l < loops.size(); l++)
        for (unsigned i = 0; i < loops[l].size(); i++) {
          const P2 & a = pts[loops[l][i]], & b = pts[loops[l][(i+1) % loops[l].size()]];
          la += ((double)a.x * b.y - (double)b.x * a.y) / 2;
        }
      for (unsigned t = 0; t < tris.size(); t += 3) {
        const P2 & a = pts[tris[t]], & b = pts[tris[t+1]], & c = pts[tris[t+2]];
        ta += ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) / 2;
      }
      if (fabs(la - ta) > 1e-6 * std::max(1.0, fabs(la))) { ok = false; why = "area mismatch"; }
    }
    if (!ok) {
      fprintf(stderr, "triangulateLoops: %s (%zu loops, %zu tris made)\n", why, loops.size(), tris.size() / 3);
      for (unsigned l = 0; l < loops.size(); l++) {
        fprintf(stderr, "  loop %u:", l);
        for (unsigned i = 0; i < loops[l].size(); i++)
          fprintf(stderr, " (%.17g,%.17g)", (double)pts[loops[l][i]].x, (double)pts[loops[l][i]].y);
        fprintf(stderr, "\n");
      }
    }
    return ok;
  }

} // namespace cubeMesh

#endif
