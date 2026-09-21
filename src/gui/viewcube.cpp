/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include "viewcube.h"
#include "arcball.h"

#include <math.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/gl.h>
#pragma GCC diagnostic pop

static const int kSize = 156;
static const int kMargin = 6;
static const int kHouse = 29;
static const float kExtent = 1.55f;
static const float kChamfer = 0.28f;
static const float kDragPx = 5.0f;

static void vcopy(float *d, float x, float y, float z) {
  d[0] = x; d[1] = y; d[2] = z;
}

static void vnorm(float *v) {
  float l = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
  if (l > 1e-8f) {
    v[0] /= l; v[1] /= l; v[2] /= l;
  }
}

static void vcross(const float *a, const float *b, float *c) {
  c[0] = a[1]*b[2] - a[2]*b[1];
  c[1] = a[2]*b[0] - a[0]*b[2];
  c[2] = a[0]*b[1] - a[1]*b[0];
}

static float vdot(const float *a, const float *b) {
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static bool pointOnArc(float px, float py, float cx, float cy, float r,
                       float a0, float a1, bool ccw, float pad);

static void mulPoint(const float m[9], float x, float y, float z, float *o) {
  o[0] = m[0]*x + m[3]*y + m[6]*z;
  o[1] = m[1]*x + m[4]*y + m[7]*z;
  o[2] = m[2]*x + m[5]*y + m[8]*z;
}

static bool pointInTri(float px, float py,
                       float ax, float ay, float bx, float by, float cx, float cy) {
  float v0x = cx - ax, v0y = cy - ay;
  float v1x = bx - ax, v1y = by - ay;
  float v2x = px - ax, v2y = py - ay;
  float dot00 = v0x*v0x + v0y*v0y;
  float dot01 = v0x*v1x + v0y*v1y;
  float dot02 = v0x*v2x + v0y*v2y;
  float dot11 = v1x*v1x + v1y*v1y;
  float dot12 = v1x*v2x + v1y*v2y;
  float inv = dot00 * dot11 - dot01 * dot01;
  if (fabsf(inv) < 1e-12f)
    return false;
  inv = 1.0f / inv;
  float u = (dot11 * dot02 - dot01 * dot12) * inv;
  float v = (dot00 * dot12 - dot01 * dot02) * inv;
  return (u >= 0) && (v >= 0) && (u + v <= 1);
}

viewCube_c::viewCube_c(void)
  : hover(PART_NONE), pressPart(PART_NONE), pressX(0), pressY(0),
    dragging(false), tracking(false)
{
}

viewCube_c::Overlay viewCube_c::overlayRect(int winW, int winH) const {
  Overlay o;
  o.s = kSize;
  if (o.s > winW - 4) o.s = winW - 4;
  if (o.s > winH - 4) o.s = winH - 4;
  if (o.s < 40) o.s = 40;
  o.x = winW - kMargin - o.s;
  o.y = kMargin;
  if (o.x < 2) o.x = 2;
  (void)winH;
  return o;
}

void viewCube_c::houseRect(const Overlay & o, int *x, int *y, int *s) const {
  *s = kHouse;
  if (*s > o.s / 3) *s = o.s / 3;
  *x = o.x + 2;
  *y = o.y - 3;
}

void viewCube_c::project(const float m[9], float x, float y, float z,
                         const Overlay & o, float *sx, float *sy, float *sz) const {
  float p[3];
  mulPoint(m, x, y, z, p);
  *sx = o.x + (p[0] / kExtent + 1.0f) * 0.5f * o.s;
  *sy = o.y + (1.0f - (p[1] / kExtent + 1.0f) * 0.5f) * o.s;
  *sz = p[2];
}

void viewCube_c::lookMatrix(float nx, float ny, float nz, float m[9]) const {
  float n[3] = { nx, ny, nz };
  vnorm(n);

  float worldUp[3] = { 0, 1, 0 };
  if (fabsf(vdot(n, worldUp)) > 0.99f)
    vcopy(worldUp, 0, 0, -1);

  float right[3];
  vcross(worldUp, n, right);
  vnorm(right);
  float up[3];
  vcross(n, right, up);

  m[0] = right[0]; m[3] = right[1]; m[6] = right[2];
  m[1] = up[0];    m[4] = up[1];    m[7] = up[2];
  m[2] = n[0];     m[5] = n[1];     m[8] = n[2];
}

void viewCube_c::partLook(Part p, float n[3]) const {
  static const float dirs[][3] = {
    {  1,  0,  0 }, { -1,  0,  0 }, {  0,  1,  0 }, {  0, -1,  0 }, {  0,  0,  1 }, {  0,  0, -1 },
    {  0,  1,  1 }, {  0, -1,  1 }, {  0,  1, -1 }, {  0, -1, -1 },
    {  1,  0,  1 }, { -1,  0,  1 }, {  1,  0, -1 }, { -1,  0, -1 },
    {  1,  1,  0 }, { -1,  1,  0 }, {  1, -1,  0 }, { -1, -1,  0 },
    {  1,  1,  1 }, { -1,  1,  1 }, {  1, -1,  1 }, { -1, -1,  1 },
    {  1,  1, -1 }, { -1,  1, -1 }, {  1, -1, -1 }, { -1, -1, -1 },
  };
  if (p < 0 || p > CORNER_NNN) {
    vcopy(n, 0, 0, 1);
    return;
  }
  vcopy(n, dirs[p][0], dirs[p][1], dirs[p][2]);
  vnorm(n);
}

void viewCube_c::snapToPart(Part p, rotater_c * rot) const {
  if (!rot || p < FACE_PX || p > CORNER_NNN)
    return;
  float n[3], m[9];
  partLook(p, n);
  lookMatrix(n[0], n[1], n[2], m);
  rot->setRotation(m);
}

void viewCube_c::snapNearest(rotater_c * rot) const {
  if (!rot)
    return;
  float cur[9];
  rot->getRotation(cur);
  float cam[3] = { cur[2], cur[5], cur[8] };
  float cup[3] = { cur[1], cur[4], cur[7] };

  float best = -2.0f;
  Part bestP = FACE_PZ;
  for (int p = FACE_PX; p <= CORNER_NNN; p++) {
    float n[3], m[9];
    partLook((Part)p, n);
    lookMatrix(n[0], n[1], n[2], m);
    float tn[3] = { m[2], m[5], m[8] };
    float tu[3] = { m[1], m[4], m[7] };
    float score = vdot(cam, tn) + 0.25f * vdot(cup, tu);
    if (score > best) {
      best = score;
      bestP = (Part)p;
    }
  }
  if (best > 0.97f)
    snapToPart(bestP, rot);
}

static bool axisAligned(const float *v) {
  float ax = fabsf(v[0]), ay = fabsf(v[1]), az = fabsf(v[2]);
  float mx = ax > ay ? ax : ay;
  if (az > mx) mx = az;
  return mx > 0.97f;
}

bool viewCube_c::isFaceAligned(rotater_c * rot) const {
  if (!rot)
    return false;
  float m[9];
  rot->getRotation(m);
  float n[3] = { m[2], m[5], m[8] };
  float u[3] = { m[1], m[4], m[7] };
  return axisAligned(n) && axisAligned(u);
}

bool viewCube_c::isNavPart(Part p) const {
  return p == PART_HOME || (p >= ARROW_UP && p <= ROLL_CW);
}

void viewCube_c::applyNav(Part p, rotater_c * rot) const {
  if (!rot)
    return;
  float m[9];
  rot->getRotation(m);
  float r[3] = { m[0], m[3], m[6] };
  float u[3] = { m[1], m[4], m[7] };
  float n[3] = { m[2], m[5], m[8] };
  float nr[3], nu[3], nn[3];

  switch (p) {
  case ARROW_UP:
    vcopy(nr, r[0], r[1], r[2]);
    vcopy(nu, -n[0], -n[1], -n[2]);
    vcopy(nn, u[0], u[1], u[2]);
    break;
  case ARROW_DOWN:
    vcopy(nr, r[0], r[1], r[2]);
    vcopy(nu, n[0], n[1], n[2]);
    vcopy(nn, -u[0], -u[1], -u[2]);
    break;
  case ARROW_RIGHT:
    vcopy(nr, -n[0], -n[1], -n[2]);
    vcopy(nu, u[0], u[1], u[2]);
    vcopy(nn, r[0], r[1], r[2]);
    break;
  case ARROW_LEFT:
    vcopy(nr, n[0], n[1], n[2]);
    vcopy(nu, u[0], u[1], u[2]);
    vcopy(nn, -r[0], -r[1], -r[2]);
    break;
  case ROLL_CW:
    vcopy(nr, u[0], u[1], u[2]);
    vcopy(nu, -r[0], -r[1], -r[2]);
    vcopy(nn, n[0], n[1], n[2]);
    break;
  case ROLL_CCW:
    vcopy(nr, -u[0], -u[1], -u[2]);
    vcopy(nu, r[0], r[1], r[2]);
    vcopy(nn, n[0], n[1], n[2]);
    break;
  default:
    return;
  }

  m[0] = nr[0]; m[3] = nr[1]; m[6] = nr[2];
  m[1] = nu[0]; m[4] = nu[1]; m[7] = nu[2];
  m[2] = nn[0]; m[5] = nn[1]; m[8] = nn[2];
  rot->setRotation(m);
}

viewCube_c::NavLayout viewCube_c::navLayout(const Overlay & o) const {
  NavLayout n;
  n.visible = true;

  float cx = o.x + o.s * 0.5f;
  float cy = o.y + o.s * 0.5f;
  float hs = o.s / (2.0f * kExtent);
  float gap = 7.0f;
  float ts = 11.0f;

  /* UP */
  n.tri[0][0][0] = cx;           n.tri[0][0][1] = cy - hs - gap - ts;
  n.tri[0][1][0] = cx - ts*0.65f; n.tri[0][1][1] = cy - hs - gap;
  n.tri[0][2][0] = cx + ts*0.65f; n.tri[0][2][1] = cy - hs - gap;
  /* DOWN */
  n.tri[1][0][0] = cx;           n.tri[1][0][1] = cy + hs + gap + ts;
  n.tri[1][1][0] = cx + ts*0.65f; n.tri[1][1][1] = cy + hs + gap;
  n.tri[1][2][0] = cx - ts*0.65f; n.tri[1][2][1] = cy + hs + gap;
  /* LEFT */
  n.tri[2][0][0] = cx - hs - gap - ts; n.tri[2][0][1] = cy;
  n.tri[2][1][0] = cx - hs - gap;      n.tri[2][1][1] = cy + ts*0.65f;
  n.tri[2][2][0] = cx - hs - gap;      n.tri[2][2][1] = cy - ts*0.65f;
  /* RIGHT */
  n.tri[3][0][0] = cx + hs + gap + ts; n.tri[3][0][1] = cy;
  n.tri[3][1][0] = cx + hs + gap;      n.tri[3][1][1] = cy - ts*0.65f;
  n.tri[3][2][0] = cx + hs + gap;      n.tri[3][2][1] = cy + ts*0.65f;

  /* Short arcs just outside the cube faces, wrapping the top-right. */
  const float kDeg = 3.14159265f / 180.0f;
  float r = hs + 24.0f;
  n.rollHalfW = 4.6f;

  /* CCW: along the top, pointing left */
  n.rollCx[0] = cx;
  n.rollCy[0] = cy;
  n.rollR[0] = r;
  n.rollA0[0] = 308.0f * kDeg;
  n.rollA1[0] = 289.0f * kDeg;

  /* CW: along the right, pointing down */
  n.rollCx[1] = cx;
  n.rollCy[1] = cy;
  n.rollR[1] = r;
  n.rollA0[1] = 322.0f * kDeg;
  n.rollA1[1] = 341.0f * kDeg;

  return n;
}

struct PickPoly {
  viewCube_c::Part part;
  int n;
  float v[4][3];
};

static void addPoly(PickPoly * polys, int * count, viewCube_c::Part part, int n,
                    float ax, float ay, float az,
                    float bx, float by, float bz,
                    float cx, float cy, float cz,
                    float dx = 0, float dy = 0, float dz = 0) {
  PickPoly * p = &polys[*count];
  p->part = part;
  p->n = n;
  vcopy(p->v[0], ax, ay, az);
  vcopy(p->v[1], bx, by, bz);
  vcopy(p->v[2], cx, cy, cz);
  if (n == 4)
    vcopy(p->v[3], dx, dy, dz);
  (*count)++;
}

static int buildPolys(PickPoly * polys) {
  const float c = kChamfer;
  const float o = 1.0f;
  const float i = o - c;
  int n = 0;

  /* faces: +X -X +Y -Y +Z -Z */
  addPoly(polys, &n, viewCube_c::FACE_PX, 4,  o,-i,-i,  o, i,-i,  o, i, i,  o,-i, i);
  addPoly(polys, &n, viewCube_c::FACE_NX, 4, -o,-i, i, -o, i, i, -o, i,-i, -o,-i,-i);
  addPoly(polys, &n, viewCube_c::FACE_PY, 4, -i, o,-i,  i, o,-i,  i, o, i, -i, o, i);
  addPoly(polys, &n, viewCube_c::FACE_NY, 4, -i,-o, i,  i,-o, i,  i,-o,-i, -i,-o,-i);
  addPoly(polys, &n, viewCube_c::FACE_PZ, 4, -i,-i, o,  i,-i, o,  i, i, o, -i, i, o);
  addPoly(polys, &n, viewCube_c::FACE_NZ, 4,  i,-i,-o, -i,-i,-o, -i, i,-o,  i, i,-o);

  /* edges adjacent to +Z / -Z (horizontal in Y) */
  addPoly(polys, &n, viewCube_c::EDGE_PY_PZ, 4, -i, i, o,  i, i, o,  i, o, i, -i, o, i);
  addPoly(polys, &n, viewCube_c::EDGE_NY_PZ, 4,  i,-i, o, -i,-i, o, -i,-o, i,  i,-o, i);
  addPoly(polys, &n, viewCube_c::EDGE_PY_NZ, 4,  i, i,-o, -i, i,-o, -i, o,-i,  i, o,-i);
  addPoly(polys, &n, viewCube_c::EDGE_NY_NZ, 4, -i,-i,-o,  i,-i,-o,  i,-o,-i, -i,-o,-i);

  addPoly(polys, &n, viewCube_c::EDGE_PX_PZ, 4,  i,-i, o,  o,-i, i,  o, i, i,  i, i, o);
  addPoly(polys, &n, viewCube_c::EDGE_NX_PZ, 4, -o,-i, i, -i,-i, o, -i, i, o, -o, i, i);
  addPoly(polys, &n, viewCube_c::EDGE_PX_NZ, 4,  o,-i,-i,  i,-i,-o,  i, i,-o,  o, i,-i);
  addPoly(polys, &n, viewCube_c::EDGE_NX_NZ, 4, -i,-i,-o, -o,-i,-i, -o, i,-i, -i, i,-o);

  addPoly(polys, &n, viewCube_c::EDGE_PX_PY, 4,  i, o,-i,  o, i,-i,  o, i, i,  i, o, i);
  addPoly(polys, &n, viewCube_c::EDGE_NX_PY, 4, -o, i,-i, -i, o,-i, -i, o, i, -o, i, i);
  addPoly(polys, &n, viewCube_c::EDGE_PX_NY, 4,  o,-i,-i,  i,-o,-i,  i,-o, i,  o,-i, i);
  addPoly(polys, &n, viewCube_c::EDGE_NX_NY, 4, -i,-o,-i, -o,-i,-i, -o,-i, i, -i,-o, i);

  /* corners +++ +-+ etc. */
  addPoly(polys, &n, viewCube_c::CORNER_PPP, 3,  i, i, o,  o, i, i,  i, o, i);
  addPoly(polys, &n, viewCube_c::CORNER_NPP, 3, -i, i, o, -i, o, i, -o, i, i);
  addPoly(polys, &n, viewCube_c::CORNER_PNP, 3,  i,-i, o,  i,-o, i,  o,-i, i);
  addPoly(polys, &n, viewCube_c::CORNER_NNP, 3, -i,-i, o, -o,-i, i, -i,-o, i);
  addPoly(polys, &n, viewCube_c::CORNER_PPN, 3,  i, i,-o,  i, o,-i,  o, i,-i);
  addPoly(polys, &n, viewCube_c::CORNER_NPN, 3, -i, i,-o, -o, i,-i, -i, o,-i);
  addPoly(polys, &n, viewCube_c::CORNER_PNN, 3,  i,-i,-o,  o,-i,-i,  i,-o,-i);
  addPoly(polys, &n, viewCube_c::CORNER_NNN, 3, -i,-i,-o, -i,-o,-i, -o,-i,-i);

  return n;
}

viewCube_c::Part viewCube_c::hitTest(int mx, int my, rotater_c * rot, int winW, int winH) const {
  Overlay o = overlayRect(winW, winH);
  int hx, hy, hs;
  houseRect(o, &hx, &hy, &hs);
  if (mx >= hx && mx <= hx + hs && my >= hy && my <= hy + hs)
    return PART_HOME;

  if (isFaceAligned(rot)) {
    NavLayout nav = navLayout(o);
    static const Part triPart[4] = { ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT };
    for (int i = 0; i < 4; i++) {
      if (pointInTri(mx, my,
                     nav.tri[i][0][0], nav.tri[i][0][1],
                     nav.tri[i][1][0], nav.tri[i][1][1],
                     nav.tri[i][2][0], nav.tri[i][2][1]))
        return triPart[i];
    }
    for (int i = 0; i < 2; i++) {
      if (pointOnArc((float)mx, (float)my,
                     nav.rollCx[i], nav.rollCy[i], nav.rollR[i],
                     nav.rollA0[i], nav.rollA1[i], i == 0,
                     nav.rollHalfW + 6.0f))
        return i == 0 ? ROLL_CCW : ROLL_CW;
    }
  }

  if (mx < o.x || my < o.y || mx > o.x + o.s || my > o.y + o.s)
    return PART_NONE;

  if (!rot)
    return PART_NONE;

  float m[9];
  rot->getRotation(m);

  PickPoly polys[32];
  int npoly = buildPolys(polys);

  Part best = PART_NONE;
  float bestZ = -1e9f;

  for (int i = 0; i < npoly; i++) {
    float sx[4], sy[4], sz[4];
    for (int k = 0; k < polys[i].n; k++)
      project(m, polys[i].v[k][0], polys[i].v[k][1], polys[i].v[k][2], o, &sx[k], &sy[k], &sz[k]);

    float nrm[3];
    float ea[3] = {
      polys[i].v[1][0] - polys[i].v[0][0],
      polys[i].v[1][1] - polys[i].v[0][1],
      polys[i].v[1][2] - polys[i].v[0][2]
    };
    float eb[3] = {
      polys[i].v[2][0] - polys[i].v[0][0],
      polys[i].v[2][1] - polys[i].v[0][1],
      polys[i].v[2][2] - polys[i].v[0][2]
    };
    vcross(ea, eb, nrm);
    vnorm(nrm);
    float wn[3];
    mulPoint(m, nrm[0], nrm[1], nrm[2], wn);
    if (wn[2] < 0.02f)
      continue;

    bool inside = pointInTri(mx, my, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
    if (!inside && polys[i].n == 4)
      inside = pointInTri(mx, my, sx[0], sy[0], sx[2], sy[2], sx[3], sy[3]);
    if (!inside)
      continue;

    float z = 0;
    for (int k = 0; k < polys[i].n; k++)
      z += sz[k];
    z /= polys[i].n;
    if (z > bestZ) {
      bestZ = z;
      best = polys[i].part;
    }
  }

  return best;
}

bool viewCube_c::contains(int x, int y, int winW, int winH) const {
  Overlay o = overlayRect(winW, winH);
  return x >= o.x && y >= o.y && x <= o.x + o.s && y <= o.y + o.s;
}

viewCube_c::Action viewCube_c::handle(int event, rotater_c * rot, int winW, int winH) {

  int mx = Fl::event_x();
  int my = Fl::event_y();

  switch (event) {
  case FL_ENTER:
    return ACT_NONE;

  case FL_LEAVE:
    if (hover != PART_NONE) {
      hover = PART_NONE;
      return ACT_REDRAW;
    }
    return ACT_NONE;

  case FL_MOVE: {
    Part h = hitTest(mx, my, rot, winW, winH);
    if (h != hover) {
      hover = h;
      return ACT_REDRAW;
    }
    return ACT_NONE;
  }

  case FL_PUSH: {
    Part h = hitTest(mx, my, rot, winW, winH);
    hover = h;
    if (h == PART_NONE)
      return ACT_NONE;
    pressPart = h;
    pressX = mx;
    pressY = my;
    dragging = false;
    tracking = true;
    if (!isNavPart(h) && rot)
      rot->click((float)mx, (float)my);
    return ACT_REDRAW;
  }

  case FL_DRAG:
    if (!tracking)
      return ACT_NONE;
    {
      float dx = (float)(mx - pressX);
      float dy = (float)(my - pressY);
      if (dx*dx + dy*dy > kDragPx * kDragPx)
        dragging = true;
    }
    if (pressPart != PART_HOME && !isNavPart(pressPart) && rot && dragging) {
      rot->drag((float)mx, (float)my);
      hover = hitTest(mx, my, rot, winW, winH);
      return ACT_REDRAW;
    }
    {
      Part h = hitTest(mx, my, rot, winW, winH);
      if (h != hover) {
        hover = h;
        return ACT_REDRAW;
      }
    }
    return ACT_NONE;

  case FL_RELEASE:
    if (!tracking)
      return ACT_NONE;
    tracking = false;
    if (isNavPart(pressPart)) {
      Part h = hitTest(mx, my, rot, winW, winH);
      Part applied = pressPart;
      pressPart = PART_NONE;
      dragging = false;
      hover = h;
      if (h == applied) {
        if (applied == PART_HOME)
          return ACT_HOME;
        applyNav(applied, rot);
      }
      return ACT_REDRAW;
    }
    if (rot)
      rot->clack((float)mx, (float)my);
    if (!dragging && pressPart >= FACE_PX && pressPart <= CORNER_NNN)
      snapToPart(pressPart, rot);
    else if (dragging)
      snapNearest(rot);
    pressPart = PART_NONE;
    dragging = false;
    hover = hitTest(mx, my, rot, winW, winH);
    return ACT_REDRAW;
  }

  return ACT_NONE;
}

static void glColorGrey(float shade, bool lit) {
  if (lit)
    glColor3f(0.38f, 0.38f, 0.40f);
  else
    glColor3f(shade, shade, shade);
}

static void emitPoly(const PickPoly & p) {
  glBegin(p.n == 4 ? GL_QUADS : GL_TRIANGLES);
  for (int i = 0; i < p.n; i++)
    glVertex3fv(p.v[i]);
  glEnd();
}

static void emitOutline(const PickPoly & p) {
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < p.n; i++)
    glVertex3fv(p.v[i]);
  glEnd();
}

static float wrapPi(float a) {
  const float twopi = 2.0f * 3.14159265f;
  while (a < 0) a += twopi;
  while (a >= twopi) a -= twopi;
  return a;
}

static bool angleOnArc(float a, float a0, float a1, bool ccw) {
  a = wrapPi(a);
  a0 = wrapPi(a0);
  a1 = wrapPi(a1);
  if (ccw) {
    if (a0 >= a1)
      return a <= a0 && a >= a1;
    return a <= a0 || a >= a1;
  }
  if (a1 >= a0)
    return a >= a0 && a <= a1;
  return a >= a0 || a <= a1;
}

static bool pointOnArc(float px, float py, float cx, float cy, float r,
                       float a0, float a1, bool ccw, float pad) {
  float dx = px - cx, dy = py - cy;
  float dist = sqrtf(dx * dx + dy * dy);
  if (fabsf(dist - r) > pad)
    return false;
  return angleOnArc(atan2f(dy, dx), a0, a1, ccw);
}

static void arrowFill(bool lit) {
  if (lit)
    glColor3f(0.82f, 0.82f, 0.84f);
  else
    glColor3f(1.0f, 1.0f, 1.0f);
}

static void arrowOutline(void) {
  glColor3f(0.08f, 0.08f, 0.10f);
  glLineWidth(1.6f);
}

static void drawFilledTri(const float t[3][2], bool lit) {
  arrowFill(lit);
  glBegin(GL_TRIANGLES);
  glVertex2f(t[0][0], t[0][1]);
  glVertex2f(t[1][0], t[1][1]);
  glVertex2f(t[2][0], t[2][1]);
  glEnd();
  arrowOutline();
  glBegin(GL_LINE_LOOP);
  glVertex2f(t[0][0], t[0][1]);
  glVertex2f(t[1][0], t[1][1]);
  glVertex2f(t[2][0], t[2][1]);
  glEnd();
}

static void drawArcArrow(float cx, float cy, float r, float a0, float a1, bool ccw, bool lit) {
  float span = a1 - a0;
  if (ccw) {
    while (span > 0) span -= 2.0f * 3.14159265f;
    if (span > -0.15f) span -= 2.0f * 3.14159265f;
  } else {
    while (span < 0) span += 2.0f * 3.14159265f;
    if (span < 0.15f) span += 2.0f * 3.14159265f;
  }

  const float halfW = 4.6f;
  const float headLen = 13.0f;
  const float headW = 8.2f;
  const int segs = 18;
  const int capSegs = 8;
  const float pi = 3.14159265f;
  float r0 = r - halfW;
  float r1 = r + halfW;
  float c0 = cosf(a0), s0 = sinf(a0);
  float c1 = cosf(a1), s1 = sinf(a1);
  float tgx = -sinf(a1) * (ccw ? -1.0f : 1.0f);
  float tgy =  cosf(a1) * (ccw ? -1.0f : 1.0f);
  float tlen = sqrtf(tgx * tgx + tgy * tgy);
  if (tlen < 1e-4f)
    return;
  tgx /= tlen;
  tgy /= tlen;

  float tgx0 = -sinf(a0) * (ccw ? -1.0f : 1.0f);
  float tgy0 =  cosf(a0) * (ccw ? -1.0f : 1.0f);
  float t0len = sqrtf(tgx0 * tgx0 + tgy0 * tgy0);
  if (t0len > 1e-4f) {
    tgx0 /= t0len;
    tgy0 /= t0len;
  }

  float capX = cx + r * c0;
  float capY = cy + r * s0;

  /* Band ends on the triangle base so the head is attached to the box. */
  float bx = cx + r * c1;
  float by = cy + r * s1;
  float tipx = bx + tgx * headLen;
  float tipy = by + tgy * headLen;
  float baseInX = cx + (r - headW) * c1;
  float baseInY = cy + (r - headW) * s1;
  float baseOutX = cx + (r + headW) * c1;
  float baseOutY = cy + (r + headW) * s1;

  arrowFill(lit);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(capX, capY);
  for (int i = 0; i <= capSegs; i++) {
    float phi = pi * (1.0f - i / (float)capSegs);
    float px = capX + halfW * (cosf(phi) * c0 - sinf(phi) * tgx0);
    float py = capY + halfW * (cosf(phi) * s0 - sinf(phi) * tgy0);
    glVertex2f(px, py);
  }
  glEnd();
  glBegin(GL_TRIANGLE_STRIP);
  for (int i = 0; i <= segs; i++) {
    float a = a0 + span * (i / (float)segs);
    float c = cosf(a), s = sinf(a);
    glVertex2f(cx + r0 * c, cy + r0 * s);
    glVertex2f(cx + r1 * c, cy + r1 * s);
  }
  glEnd();
  glBegin(GL_TRIANGLES);
  glVertex2f(tipx, tipy);
  glVertex2f(baseInX, baseInY);
  glVertex2f(baseOutX, baseOutY);
  glEnd();

  arrowOutline();
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i <= capSegs; i++) {
    float phi = pi * (1.0f - i / (float)capSegs);
    glVertex2f(capX + halfW * (cosf(phi) * c0 - sinf(phi) * tgx0),
               capY + halfW * (cosf(phi) * s0 - sinf(phi) * tgy0));
  }
  for (int i = 1; i <= segs; i++) {
    float a = a0 + span * (i / (float)segs);
    glVertex2f(cx + r1 * cosf(a), cy + r1 * sinf(a));
  }
  glVertex2f(baseOutX, baseOutY);
  glVertex2f(tipx, tipy);
  glVertex2f(baseInX, baseInY);
  for (int i = segs - 1; i >= 0; i--) {
    float a = a0 + span * (i / (float)segs);
    glVertex2f(cx + r0 * cosf(a), cy + r0 * sinf(a));
  }
  glEnd();
}

static void drawHouseIcon(int x, int y, int s, bool lit) {
  float pad = s * 0.08f;
  float peakX = x + s * 0.5f;
  float peakY = y + pad * 0.4f;
  float eaveY = y + s * 0.56f;
  float eaveX0 = x + s * 0.04f;
  float eaveX1 = x + s - s * 0.04f;
  float x0 = x + s * 0.20f;
  float x1 = x + s - s * 0.20f;
  float y1 = y + s - pad;

  if (lit)
    glColor3f(0.82f, 0.82f, 0.84f);
  else
    glColor3f(1.0f, 1.0f, 1.0f);

  glBegin(GL_TRIANGLES);
  glVertex2f(eaveX0, eaveY);
  glVertex2f(peakX, peakY);
  glVertex2f(eaveX1, eaveY);
  glEnd();

  glBegin(GL_QUADS);
  glVertex2f(x0, eaveY);
  glVertex2f(x1, eaveY);
  glVertex2f(x1, y1);
  glVertex2f(x0, y1);
  glEnd();

  glColor3f(0.08f, 0.08f, 0.10f);
  float dw = s * 0.08f;
  glBegin(GL_QUADS);
  glVertex2f(peakX - dw, eaveY + s * 0.04f);
  glVertex2f(peakX + dw, eaveY + s * 0.04f);
  glVertex2f(peakX + dw, y1);
  glVertex2f(peakX - dw, y1);
  glEnd();

  glLineWidth(1.6f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(peakX, peakY);
  glVertex2f(eaveX1, eaveY);
  glVertex2f(x1, eaveY);
  glVertex2f(x1, y1);
  glVertex2f(x0, y1);
  glVertex2f(x0, eaveY);
  glVertex2f(eaveX0, eaveY);
  glEnd();
}

void viewCube_c::draw(rotater_c * rot, int winW, int winH, float pixelScale) const {

  if (!rot || winW < 16 || winH < 16)
    return;

  Overlay o = overlayRect(winW, winH);
  float m[9];
  rot->getRotation(m);

  PickPoly polys[32];
  int npoly = buildPolys(polys);

  int vpX = (int)(o.x * pixelScale);
  int vpY = (int)((winH - o.y - o.s) * pixelScale);
  int vpS = (int)(o.s * pixelScale);
  if (vpS < 8)
    return;

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);

  glViewport(vpX, vpY, vpS, vpS);
  glScissor(vpX, vpY, vpS, vpS);
  glEnable(GL_SCISSOR_TEST);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(-kExtent, kExtent, -kExtent, kExtent, -5, 5);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  /* axes in the lower-left of the overlay, rotating in place */
  glPushMatrix();
  glTranslatef(-1.05f, -1.05f, 0);
  rot->addTransform();
  glLineWidth(2.2f);
  glBegin(GL_LINES);
  glColor3f(0.90f, 0.22f, 0.18f); glVertex3f(0, 0, 0); glVertex3f(0.55f, 0, 0);
  glColor3f(0.20f, 0.72f, 0.22f); glVertex3f(0, 0, 0); glVertex3f(0, 0.55f, 0);
  glColor3f(0.22f, 0.42f, 0.95f); glVertex3f(0, 0, 0); glVertex3f(0, 0, 0.55f);
  glEnd();
  glPopMatrix();

  glLoadIdentity();
  rot->addTransform();

  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f, 1.0f);

  for (int i = 0; i < npoly; i++) {
    float nrm[3];
    float a[3] = {
      polys[i].v[1][0] - polys[i].v[0][0],
      polys[i].v[1][1] - polys[i].v[0][1],
      polys[i].v[1][2] - polys[i].v[0][2]
    };
    float b[3] = {
      polys[i].v[2][0] - polys[i].v[0][0],
      polys[i].v[2][1] - polys[i].v[0][1],
      polys[i].v[2][2] - polys[i].v[0][2]
    };
    vcross(a, b, nrm);
    vnorm(nrm);
    float wn[3];
    mulPoint(m, nrm[0], nrm[1], nrm[2], wn);
    if (wn[2] < -0.02f)
      continue;

    float shade = 0.78f + 0.16f * wn[2];
    bool lit = (hover == polys[i].part);
    if (polys[i].part >= EDGE_PY_PZ && polys[i].part <= EDGE_NX_NY)
      shade -= 0.06f;
    if (polys[i].part >= CORNER_PPP)
      shade -= 0.10f;
    glColorGrey(shade, lit);
    emitPoly(polys[i]);
  }

  glDisable(GL_POLYGON_OFFSET_FILL);

  glColor3f(0.45f, 0.45f, 0.48f);
  glLineWidth(1.0f);
  for (int i = 0; i < npoly; i++) {
    float nrm[3];
    float a[3] = {
      polys[i].v[1][0] - polys[i].v[0][0],
      polys[i].v[1][1] - polys[i].v[0][1],
      polys[i].v[1][2] - polys[i].v[0][2]
    };
    float b[3] = {
      polys[i].v[2][0] - polys[i].v[0][0],
      polys[i].v[2][1] - polys[i].v[0][1],
      polys[i].v[2][2] - polys[i].v[0][2]
    };
    vcross(a, b, nrm);
    vnorm(nrm);
    float wn[3];
    mulPoint(m, nrm[0], nrm[1], nrm[2], wn);
    if (wn[2] < -0.02f)
      continue;
    emitOutline(polys[i]);
  }

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  glDisable(GL_SCISSOR_TEST);
  glViewport(0, 0, (int)(winW * pixelScale), (int)(winH * pixelScale));
  glScissor(0, 0, (int)(winW * pixelScale), (int)(winH * pixelScale));
  glEnable(GL_SCISSOR_TEST);

  /* 2D labels, house, axis letters */
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, winW, winH, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  int hx, hy, hs;
  houseRect(o, &hx, &hy, &hs);
  drawHouseIcon(hx, hy, hs, hover == PART_HOME);

  if (isFaceAligned(rot)) {
    NavLayout nav = navLayout(o);
    static const Part triPart[4] = { ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT };
    for (int i = 0; i < 4; i++)
      drawFilledTri(nav.tri[i], hover == triPart[i]);
    drawArcArrow(nav.rollCx[0], nav.rollCy[0], nav.rollR[0],
                 nav.rollA0[0], nav.rollA1[0], true, hover == ROLL_CCW);
    drawArcArrow(nav.rollCx[1], nav.rollCy[1], nav.rollR[1],
                 nav.rollA0[1], nav.rollA1[1], false, hover == ROLL_CW);
  }

  static const char * faceName[] = {
    "RIGHT", "LEFT", "TOP", "BOTTOM", "FRONT", "BACK"
  };

  glDisable(GL_TEXTURE_2D);
  gl_font(FL_HELVETICA_BOLD, 10);

  for (int f = FACE_PX; f <= FACE_NZ; f++) {
    float n[3];
    partLook((Part)f, n);
    float wn[3];
    mulPoint(m, n[0], n[1], n[2], wn);
    if (wn[2] < 0.28f)
      continue;
    float sx, sy, sz;
    project(m, n[0] * 0.72f, n[1] * 0.72f, n[2] * 0.72f, o, &sx, &sy, &sz);
    const char * lab = faceName[f];
    int tw = gl_width(lab);
    int th = gl_height();
    if (hover == f)
      glColor3f(1, 1, 1);
    else
      glColor3f(0.12f, 0.12f, 0.14f);
    gl_draw(lab, (int)(sx - tw * 0.5f), (int)(sy + th * 0.35f));
  }

  /* axis letters at the triad tips */
  {
    float sx, sy, sz;
    gl_font(FL_HELVETICA_BOLD, 11);
    const float ox = -1.05f, oy = -1.05f;
    float p[3];
    mulPoint(m, 0.62f, 0, 0, p);
    sx = o.x + ((ox + p[0]) / kExtent + 1.0f) * 0.5f * o.s;
    sy = o.y + (1.0f - ((oy + p[1]) / kExtent + 1.0f) * 0.5f) * o.s;
    glColor3f(0.90f, 0.22f, 0.18f);
    gl_draw("X", (int)sx - 3, (int)sy + 4);

    mulPoint(m, 0, 0.62f, 0, p);
    sx = o.x + ((ox + p[0]) / kExtent + 1.0f) * 0.5f * o.s;
    sy = o.y + (1.0f - ((oy + p[1]) / kExtent + 1.0f) * 0.5f) * o.s;
    glColor3f(0.20f, 0.72f, 0.22f);
    gl_draw("Y", (int)sx - 3, (int)sy + 4);

    mulPoint(m, 0, 0, 0.62f, p);
    sx = o.x + ((ox + p[0]) / kExtent + 1.0f) * 0.5f * o.s;
    sy = o.y + (1.0f - ((oy + p[1]) / kExtent + 1.0f) * 0.5f) * o.s;
    glColor3f(0.22f, 0.42f, 0.95f);
    gl_draw("Z", (int)sx - 3, (int)sy + 4);
    (void)sz;
  }

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
}
