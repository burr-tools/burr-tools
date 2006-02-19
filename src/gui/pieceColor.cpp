/* Burr Solver
 * Copyright (C) 2003-2006  Andreas R�ver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include "pieceColor.h"

#include <math.h>

#define COLS 18

// the table for the first COLS fixed defined colors
static float r[COLS] = {
  0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.7f,
  0.0f, 0.7f, 0.7f, 0.0f, 0.7f, 0.0f, 0.7f, 1.0f, 1.0f
};
static float g[COLS] = {
  0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.7f, 0.0f,
  0.7f, 0.7f, 0.0f, 1.0f, 1.0f, 0.7f, 0.0f, 0.7f, 0.0f
};
static float b[COLS] = {
  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.7f, 0.0f, 0.0f,
  0.7f, 0.0f, 0.7f, 0.7f, 0.0f, 1.0f, 1.0f, 0.0f, 0.7f
};

#define JITTERS 53

// the table with the modofication values for the multi-pieces
static float jr[JITTERS] = {
   0.0f,
  -0.3f,  0.3f, -0.3f,  0.3f, -0.3f,  0.3f, -0.3f,  0.3f,  0.3f,
  -0.3f, -0.3f,  0.3f,  0.3f, -0.3f, -0.3f,  0.3f,  0.0f,  0.0f,
   0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.3f, -0.3f,
  -0.4f,  0.4f, -0.4f,  0.4f, -0.4f,  0.4f, -0.4f,  0.4f,  0.4f,
  -0.4f, -0.4f,  0.4f,  0.4f, -0.4f, -0.4f,  0.4f,  0.0f,  0.0f,
   0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.4f, -0.4f
};
static float jg[JITTERS] = {
   0.0f,
  -0.3f,  0.3f,  0.3f, -0.3f, -0.3f,  0.3f,  0.3f, -0.3f,  0.3f,
  -0.3f,  0.3f, -0.3f,  0.0f,  0.0f,  0.0f,  0.0f,  0.3f, -0.3f,
  -0.3f,  0.3f,  0.0f,  0.0f,  0.3f, -0.3f,  0.0f,  0.0f,
  -0.4f,  0.4f,  0.4f, -0.4f, -0.4f,  0.4f,  0.4f, -0.4f,  0.4f,
  -0.4f,  0.4f, -0.4f,  0.0f,  0.0f,  0.0f,  0.0f,  0.4f, -0.4f,
  -0.4f,  0.4f,  0.0f,  0.0f,  0.4f, -0.4f,  0.0f,  0.0f
};
static float jb[JITTERS] = {
   0.0f,
  -0.3f,  0.3f,  0.3f, -0.3f,  0.3f, -0.3f, -0.3f,  0.3f,  0.0f,
   0.0f,  0.0f,  0.0f,  0.3f, -0.3f,  0.3f, -0.3f,  0.3f, -0.3f,
   0.3f, -0.3f,  0.3f, -0.3f,  0.0f,  0.0f,  0.0f,  0.0f,
  -0.4f,  0.4f,  0.4f, -0.4f,  0.4f, -0.4f, -0.4f,  0.4f,  0.0f,
   0.0f,  0.0f,  0.0f,  0.4f, -0.4f,  0.4f, -0.4f,  0.4f, -0.4f,
   0.4f, -0.4f,  0.4f, -0.4f,  0.0f,  0.0f,  0.0f,  0.0f
};



float pieceColorR(int x) {
  if (x < COLS)
    return r[x];
  else
    return float((1+sin(0.7*x))/2);
}

float pieceColorG(int x) {
  if (x < COLS)
    return g[x];
  else
    return float((1+sin(1.3*x+1.5))/2);
}

float pieceColorB(int x) {
  if (x < COLS)
    return b[x];
  else
    return float((1+sin(3.5*x+2.3))/2);
}

/* the problem is that simply adding the jitter
 * would cause an overflow every now and then, so we
 * search in the list of possible jitter values
 * for the x-th with no overflow
 * when the table is exausted, we simple do no
 * jittering any longer
 */
static int getJitter(int val, int sub) {
  int j = 0;
  float x;

  while (j < JITTERS) {
    x = pieceColorR(val) + jr[j];
    if ((x < 0) || (x > 1)) {
      j++;
      continue;
    }
    x = pieceColorG(val) + jg[j];
    if ((x < 0) || (x > 1)) {
      j++;
      continue;
    }
    x = pieceColorB(val) + jb[j];
    if ((x < 0) || (x > 1)) {
      j++;
      continue;
    }

    if (sub == 0)
      break;

    sub--;
    j++;
  }

  if (j == JITTERS) j = 0;

  return j;
}



float pieceColorR(int x, int sub) {

  float jitter = jr[getJitter(x, sub)];
  float val = pieceColorR(x);

  return val + jitter;
}

float pieceColorG(int x, int sub) {

  float jitter = jg[getJitter(x, sub)];
  float val = pieceColorG(x);

  return val + jitter;
}

float pieceColorB(int x, int sub) {

  float jitter = jb[getJitter(x, sub)];
  float val = pieceColorB(x);

  return val + jitter;
}

float darkPieceColor(float f) { return float(f * 0.9); }
float lightPieceColor(float f) { return float(1 - (0.9 * (1-f))); }

