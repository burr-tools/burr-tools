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
#ifndef __VIEW_CUBE_H__
#define __VIEW_CUBE_H__

class rotater_c;

class viewCube_c {

  public:

    enum Action {
      ACT_NONE,
      ACT_REDRAW,
      ACT_HOME
    };

    enum Part {
      PART_NONE = -1,
      FACE_PX, FACE_NX, FACE_PY, FACE_NY, FACE_PZ, FACE_NZ,
      EDGE_PY_PZ, EDGE_NY_PZ, EDGE_PY_NZ, EDGE_NY_NZ,
      EDGE_PX_PZ, EDGE_NX_PZ, EDGE_PX_NZ, EDGE_NX_NZ,
      EDGE_PX_PY, EDGE_NX_PY, EDGE_PX_NY, EDGE_NX_NY,
      CORNER_PPP, CORNER_NPP, CORNER_PNP, CORNER_NNP,
      CORNER_PPN, CORNER_NPN, CORNER_PNN, CORNER_NNN,
      PART_HOME,
      ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT,
      ROLL_CCW, ROLL_CW,
      PART_COUNT
    };

    viewCube_c(void);

    void draw(rotater_c * rot, int winW, int winH, float pixelScale) const;

    Action handle(int event, rotater_c * rot, int winW, int winH);

    bool contains(int x, int y, int winW, int winH) const;
    bool isTracking(void) const { return tracking; }

  private:

    struct Overlay {
      int x, y, s;
    };

    Overlay overlayRect(int winW, int winH) const;
    void houseRect(const Overlay & o, int *x, int *y, int *s) const;

    Part hitTest(int mx, int my, rotater_c * rot, int winW, int winH) const;
    void snapToPart(Part p, rotater_c * rot) const;
    void snapNearest(rotater_c * rot) const;
    void lookMatrix(float nx, float ny, float nz, float m[9]) const;
    void partLook(Part p, float n[3]) const;

    void project(const float m[9], float x, float y, float z,
                 const Overlay & o, float *sx, float *sy, float *sz) const;

    bool isFaceAligned(rotater_c * rot) const;
    void applyNav(Part p, rotater_c * rot) const;
    bool isNavPart(Part p) const;

    struct NavLayout {
      bool visible;
      float tri[4][3][2];
      float rollCx[2], rollCy[2], rollR[2];
      float rollA0[2], rollA1[2];
      float rollHalfW;
    };
    NavLayout navLayout(const Overlay & o) const;

    mutable Part hover;
    Part pressPart;
    int pressX, pressY;
    bool dragging;
    bool tracking;
};

#endif
