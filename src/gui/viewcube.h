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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <FL/Fl.H>
#pragma GCC diagnostic pop

class rotater_c;

class viewCube_c {

  public:

    enum Action {
      ACT_NONE,
      ACT_REDRAW,
      ACT_HOME,
      ACT_ANIMATING,
      ACT_HOME_ANIMATING  /* home clicked: reset pan/zoom, then animate rotation */
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

    Action handle(int event, rotater_c * rot, int winW, int winH, float pixelScale);

    /* Advance in-flight snap animation by one step; returns true while still
     * running.  The caller must call this on a ~60 Hz timer and redraw each
     * time until it returns false. */
    bool tick(rotater_c * rot);

    bool contains(int x, int y, int winW, int winH) const;
    bool isTracking(void) const { return tracking; }
    bool isAnimating(void) const { return animating; }

    /* Smallest host viewport the cube should appear in at all. Derived from the
     * cube's own minimum on-screen size rather than being a second independent
     * constant: below this the cube would take up so much of the view that
     * dragging the model becomes impractical, since overlayRect() cannot shrink
     * it indefinitely and handle() swallows any press landing inside it. Callers
     * that draw the cube must gate input on this too, or an unrendered cube goes
     * on eating clicks. */
    static int minimumHostSize(void);

  private:

    struct Overlay {
      int x, y, s;
    };

    Overlay overlayRect(int winW, int winH) const;
    void houseRect(const Overlay & o, int *x, int *y, int *s) const;

    Part hitTest(int mx, int my, rotater_c * rot, int winW, int winH) const;
    void snapToPart(Part p, rotater_c * rot);
    void snapNearest(rotater_c * rot);
    void lookMatrix(float nx, float ny, float nz, float m[9]) const;
    void partLook(Part p, float n[3]) const;

    void project(const float m[9], float x, float y, float z,
                 const Overlay & o, float *sx, float *sy, float *sz) const;

    bool isFaceAligned(rotater_c * rot) const;
    void applyNav(Part p, rotater_c * rot);
    bool isNavPart(Part p) const;

    void startAnim(const float target[9], rotater_c * rot);

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

    /* Snap animation state */
    float animStart[4];   /* start quaternion [x,y,z,w] */
    float animEnd[4];     /* target quaternion */
    Fl_Timestamp animStartTime; /* Fl::now() when animation began */
    bool animating;
    static constexpr double kAnimDuration = 0.30; /* seconds */
};

#endif
