/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <vector>

// contains the information for a plane
typedef struct plane
{
  double nx, ny, nz, d;
} plane;

// create a plane out of 3 pints
plane calcPlane(
    double x1, double y1, double z1,
    double x2, double y2, double z2,
    double x3, double y3, double z3) {

  plane p;

  double a1 = x2-x1;
  double a2 = y2-y1;
  double a3 = z2-z1;

  double b1 = x3-x1;
  double b2 = y3-y1;
  double b3 = z3-z1;

  // cross product of the 2 vectors

  p.nx = a2*b3-a3*b2;
  p.ny = a3*b1-a1*b3;
  p.nz = a1*b2-a2*b1;

  // for the rest to properly work, we
  // need a normalized normal
  double len = sqrt(p.nx*p.nx + p.ny*p.ny + p.nz*p.nz);
  p.nx /= len;
  p.ny /= len;
  p.nz /= len;

  p.d = x1*p.nx+y1*p.ny+z1*p.nz;

  return p;
}

void planeIntersect(plane p1, plane p2, plane p3, double * x, double * y, double * z)
{
  // solve equation system
  //  p1n x (x, y, z) = p1d
  //  p2n x (x, y, z) = p2d
  //  p3n x (x, y, z) = p3d

  double det = p1.nx*p2.ny*p3.nz+p1.ny*p2.nz*p3.nx+p1.nz*p2.nx*p3.ny-
               p1.nz*p2.ny*p3.nx-p1.ny*p2.nx*p3.nz-p1.nx*p2.nz*p3.ny;

  if (fabs(det) < 10e-5)
  {
    printf ("oops unsolvable plane intersection\n");
    return;
  }

  *x = p1.d*p2.ny*p3.nz+p1.ny*p2.nz*p3.d+p1.nz*p2.d*p3.ny-
       p1.nz*p2.ny*p3.d-p1.ny*p2.d*p3.nz-p1.d*p2.nz*p3.ny;

  *y = p1.nx*p2.d*p3.nz+p1.d*p2.nz*p3.nx+p1.nz*p2.nx*p3.d-
       p1.nz*p2.d*p3.nx-p1.d*p2.nx*p3.nz-p1.nx*p2.nz*p3.d;

  *z = p1.nx*p2.ny*p3.d+p1.ny*p2.d*p3.nx+p1.d*p2.nx*p3.ny-
       p1.d*p2.ny*p3.nx-p1.ny*p2.nx*p3.d-p1.nx*p2.d*p3.ny;

  *x /= det;
  *y /= det;
  *z /= det;
}

#define NUM_POLYHEDRA 10
#define NUM_VERTICES (4*10)
#define HEIGHT sqrt(3)/2
double vertices[NUM_VERTICES][3] =
{
  {0, 0, 0}, // 0
  {1, 1, 0}, // 1
  {1, 0, 1}, // 2
  {0, 1, 1}, // 3

  {1, 0, 0}, // 4
  {0, 0, 1}, // 5
  {0, 1, 0}, // 6
  {1, 1, 1}, // 7

  {0, 0, 0}, // 8
  {1, 0, 1}, // 9
  {1, 1, 0}, // 10
  {1, 0, 0}, // 11

  {1, 0, 0}, // 12
  {0, 1, 0}, // 13
  {0, 0, 1}, // 14
  {0, 0, 0}, // 15

  {0, 0, 0}, // 16
  {1, 1, 0}, // 17
  {0, 1, 1}, // 18
  {0, 1, 0}, // 19

  {0, 1, 0}, // 20
  {1, 0, 0}, // 21
  {1, 1, 1}, // 22
  {1, 1, 0}, // 23

  {0, 0, 0}, // 24
  {1, 0, 1}, // 25
  {0, 0, 1}, // 26
  {0, 1, 1}, // 27

  {1, 0, 0}, // 28
  {1, 0, 1}, // 29
  {0, 0, 1}, // 30
  {1, 1, 1}, // 31

  {1, 1, 0}, // 32
  {1, 1, 1}, // 33
  {1, 0, 1}, // 34
  {0, 1, 1}, // 35

  {0, 1, 0}, // 36
  {0, 0, 1}, // 37
  {0, 1, 1}, // 38
  {1, 1, 1}, // 39
};

#define NUM_FACES 4
int faces[NUM_POLYHEDRA][NUM_FACES][4] =
{
    { { 3,  0, 1, 2 },    { 3,  0, 3, 1 },    { 3,  0, 2, 3 },    { 3,  2, 1, 3 } },
    { { 3,  4, 5, 6 },    { 3,  4, 6, 7 },    { 3,  4, 7, 5 },    { 3,  6, 5, 7 } },
    { { 3,  8, 9, 10 },   { 3,  8, 10, 11 },  { 3,  11, 10, 9 },  { 3,  8, 11, 9 } },
    { { 3,  12, 13, 14 }, { 3,  15, 13, 12 }, { 3,  15, 12, 14 }, { 3,  15, 14, 13 } },
    { { 3,  16, 17, 18 }, { 3,  16, 19, 17 }, { 3,  19, 18, 17 }, { 3,  16, 18, 19 } },
    { { 3,  21, 22, 20 }, { 3,  21, 20, 23 }, { 3,  21, 23, 22 }, { 3,  20, 22, 23 } },  // 5
    { { 3,  24, 25, 26 }, { 3,  24, 26, 27 }, { 3,  24, 27, 25 }, { 3,  26, 25, 27 } },
    { { 3,  28, 29, 30 }, { 3,  28, 31, 29 }, { 3,  28, 30, 31 }, { 3,  30, 29, 31 } },  // 7
    { { 3,  32, 33, 34 }, { 3,  32, 35, 33 }, { 3,  34, 35, 32 }, { 3,  34, 33, 35 } },
    { { 3,  36, 37, 38 }, { 3,  36, 38, 39 }, { 3,  36, 39, 37 }, { 3,  37, 39, 38 } }
};

int faces2[NUM_POLYHEDRA][NUM_FACES][5];

void printNumber(FILE * f, double num)
{
  static const struct {
    double val;
    const char * text;
    bool brackets;
  } values[] = {
    { 0, "0" },
    { 1, "1" },
    { 2, "2" },
    { sqrt(0.75), "sqrt(0.75)" },
    { 0.5, "0.5" },
    { sqrt(3), "sqrt(3)" },
    { sqrt(1.0/3), "sqrt(1.0/3)" },
    { sqrt(4.0/3), "sqrt(4.0/3)" },
    { 1+sqrt(8), "1+sqrt(8)", true },
    { 1+sqrt(2), "1+sqrt(2)", true },
    { sqrt(0.5), "sqrt(0.5)" },
    { sqrt(2), "sqrt(2)" },
    { sqrt(4.5), "sqrt(4.5)" },
    { sqrt(8), "sqrt(8)" },
    { 1+sqrt(3), "1+sqrt(3)", true },
    { 2+sqrt(3), "2+sqrt(3)", true },
    { -1 },
  };

  int i = 0;

  while (values[i].val >= 0)
  {
    if (fabs(num-values[i].val) < 0.0001)
    {
      fprintf(f, values[i].text);
      return;
    }
    if (fabs(num+values[i].val) < 0.0001)
    {
      if (values[i].brackets)
        fprintf(f, "-(%s)", values[i].text);
      else
        fprintf(f, "-%s", values[i].text);
      return;
    }

    i++;
  }

  fprintf(f, "%f", num);
}

// calculate the information for bevel and offset stuff
// idea is to use proper mathematics and intersect the planes to find
// the real points for some values and extrapolate the movement vectors out of that
// assuming they are linearly used
void beveloffset(void)
{
  FILE * out = fopen("meshverts.inc", "w");
  fprintf(out, "static const double vertices[][3][3] = {\n");

  // for each vertex we must calculate for each plane that has a corner at that vertex a

  int newVertexNumber = 0;


  for (int pl = 0; pl < NUM_POLYHEDRA; pl++)
  {
    for (int v = 0; v < NUM_VERTICES; v++)
    {
      // find all faces that have a corner at the current vertex
      std::vector<int> vfaces;
      std::vector<int> vertexNumber;
      for (int f = 0; f < NUM_FACES; f++)
        for (int i = 0; i < faces[pl][f][0]; i++)
          if (faces[pl][f][i+1] == v)
          {
            vfaces.push_back(f);
            vertexNumber.push_back(i);
            break;
          }

      if (vfaces.size() == 0)
        continue;

      // right now we can only handle exactly 3 faces for each vertex
      if (vfaces.size() != 3)
      {
        printf("oops not valid number of faces at a vertex, aborting %i\n", vfaces.size());
        return;
      }

      // now we calculate the 3 new corners for this vertex for each plane

      plane p[3];

      for (int i = 0; i < vfaces.size(); i++)
      {
        p[i] = calcPlane(
            vertices[faces[pl][vfaces[i]][1]][0], vertices[faces[pl][vfaces[i]][1]][1], vertices[faces[pl][vfaces[i]][1]][2],
            vertices[faces[pl][vfaces[i]][2]][0], vertices[faces[pl][vfaces[i]][2]][1], vertices[faces[pl][vfaces[i]][2]][2],
            vertices[faces[pl][vfaces[i]][3]][0], vertices[faces[pl][vfaces[i]][3]][1], vertices[faces[pl][vfaces[i]][3]][2]);
      }

      // starting point
      double xs, ys, zs;

      planeIntersect(p[0], p[1], p[2], &xs, &ys, &zs);

      // the result should be reasonable close to the original vertex
      if (fabs(vertices[v][0] - xs) > 10e-5 ||
          fabs(vertices[v][1] - ys) > 10e-5 ||
          fabs(vertices[v][2] - zs) > 10e-5)
      {
        printf("oops safety check failes\n");
        return;
      }

      // starting point
      double xo, yo, zo;


      // for the bevel, we simply move all the planes for a small amount
      // against the normal (0.1) and see where we arrive
      // out of those values we can calculate the offset vector
      p[0].d -= 0.1;
      p[1].d -= 0.1;
      p[2].d -= 0.1;
      planeIntersect(p[0], p[1], p[2], &xo, &yo, &zo);
      p[0].d += 0.1;
      p[1].d += 0.1;
      p[2].d += 0.1;

      // this is the offset vector now
      xo -= xs;
      yo -= ys;
      zo -= zs;

      xo *= 10;
      yo *= 10;
      zo *= 10;

      // for the bevel, we do something similar, just that we don't move
      // the plane, where the bevel will be, that results in 3 different
      // bevel vectors, one for each plane involved in the operation

      for (int i = 0; i < vfaces.size(); i++)
      {
        for (int j = 0; j < vfaces.size(); j++)
          if (i != j)
            p[j].d -= 0.1;

        // starting point
        double xb, yb, zb;

        planeIntersect(p[0], p[1], p[2], &xb, &yb, &zb);

        for (int j = 0; j < vfaces.size(); j++)
          if (i != j)
            p[j].d += 0.1;

        // this is the offset vector now
        xb -= xs;
        yb -= ys;
        zb -= zs;

        xb *= 10;
        yb *= 10;
        zb *= 10;


        fprintf(out, "  { { ");
        printNumber(out, vertices[v][0]); fprintf(out, ", ");
        printNumber(out, vertices[v][1]); fprintf(out, ", ");
        printNumber(out, vertices[v][2]); fprintf(out, "}, {");

        printNumber(out, xo); fprintf(out, ", ");
        printNumber(out, yo); fprintf(out, ", ");
        printNumber(out, zo); fprintf(out, "}, {");

        printNumber(out, xb); fprintf(out, ", ");
        printNumber(out, yb); fprintf(out, ", ");
        printNumber(out, zb); fprintf(out, "} },\n");

        printf(" %f %f %f  -> + o*(%f %f %f) + b*(%f %f %f)\n", vertices[v][0], vertices[v][1], vertices[v][2], xo, yo, zo, xb, yb, zb);

        faces2[pl][vfaces[i]][vertexNumber[i]+1] = newVertexNumber;
        newVertexNumber++;
      }
    }
  }

  // we output 2 things: the list of verteces: each vertex consists of one position, and 2 vectors
  // that need to be scaled according to the bevel and offset and added

  fprintf(out, "};\n\n");

  // the 2nd thing we write out is the list of faces, one list for the "normal" faces and one for
  // the bevel faces.
  fprintf(out, "static const int normalFaces[10][4][3] = {\n");

  for (int p = 0; p < NUM_POLYHEDRA; p++)
  {
    fprintf(out, "  {\n");
    for (int i = 0; i < NUM_FACES; i++)
    {
      if (faces[p][i][0] == 3)
        fprintf(out, "    { %i, %i, %i },\n", faces2[p][i][1], faces2[p][i][2], faces2[p][i][3]);
      else
        printf("ooops\n");
    }
    fprintf(out, "  },\n");
  }

  fprintf(out, "};\n\n");

  fprintf(out, "static const int bevelFaces[10][10][5] = {\n");

  for (int p = 0; p < NUM_POLYHEDRA; p++)
  {

    fprintf(out, "  {\n");

    // the the final output of the corners we need to know in what directions
    // the triangles must be oriented, we save that within this array for
    // each vertex
    int vertexdir[NUM_VERTICES];
    for (int i = 0; i < NUM_VERTICES; i++)
    {
      vertexdir[i] = -1;
    }


    // first let us output the edge bevel faces
    for (int i = 0; i < NUM_VERTICES; i++)
    {
      for (int j = i+1; j < NUM_VERTICES; j++)
      {
        int f = 0;

        // ontains the found faces that use edge i-j
        int facecorners[20][3];
        int foundfaces = 0;

        for (int f = 0; f < NUM_FACES; f++)
        {

          for (int e = 0; e < faces[p][f][0]; e++)
          {
            if (faces[p][f][1+(e % faces[p][f][0])] == i &&
                faces[p][f][1+((e+1) % faces[p][f][0])] == j)
            {
              facecorners[foundfaces][0] = f;
              facecorners[foundfaces][1] = e;
              facecorners[foundfaces][2] = (e+1) % faces[p][f][0];
              foundfaces++;
            }

            if (faces[p][f][1+(e % faces[p][f][0])] == j &&
                faces[p][f][1+((e+1) % faces[p][f][0])] == i)
            {
              facecorners[foundfaces][0] = f;
              facecorners[foundfaces][1] = e;
              facecorners[foundfaces][2] = (e+1) % faces[p][f][0];
              foundfaces++;
            }
          }
        }

        if (foundfaces == 0)
          continue;

        if (foundfaces != 2)
        {
          printf("oops too many faces on one edge, only 2 allowed\n");
        }

        if (
            faces[p][facecorners[0][0]][1+facecorners[0][1]] !=
            faces[p][facecorners[1][0]][1+facecorners[1][2]]
            ||
            faces[p][facecorners[0][0]][1+facecorners[0][2]] !=
            faces[p][facecorners[1][0]][1+facecorners[1][1]])
        {
          printf("oops not the right corners %i %i    %i %i\n",
              faces[p][facecorners[0][0]][1+facecorners[0][1]],
              faces[p][facecorners[1][0]][1+facecorners[0][2]],
              faces[p][facecorners[0][0]][1+facecorners[1][1]],
              faces[p][facecorners[1][0]][1+facecorners[1][2]]);
        }

        fprintf(out, "    { 4,  %i, %i, %i, %i },\n",
            faces2[p][facecorners[0][0]][1+facecorners[0][2]],
            faces2[p][facecorners[0][0]][1+facecorners[0][1]],
            faces2[p][facecorners[1][0]][1+facecorners[1][2]],
            faces2[p][facecorners[1][0]][1+facecorners[1][1]]);

        if (
            ((faces2[p][facecorners[0][0]][1+facecorners[0][1]] + 1) ==
             faces2[p][facecorners[1][0]][1+facecorners[1][2]])
            ||
            ((faces2[p][facecorners[0][0]][1+facecorners[0][1]] - 2) ==
             faces2[p][facecorners[1][0]][1+facecorners[1][2]])
           )

          vertexdir[faces[p][facecorners[0][0]][1+facecorners[0][1]]] = 2;
        else
          vertexdir[faces[p][facecorners[0][0]][1+facecorners[0][1]]] = 1;

        if (
            ((faces2[p][facecorners[0][0]][1+facecorners[0][2]]) ==
             faces2[p][facecorners[1][0]][1+facecorners[1][1]] + 1)
            ||
            ((faces2[p][facecorners[0][0]][1+facecorners[0][2]]) ==
             faces2[p][facecorners[1][0]][1+facecorners[1][1]] - 2)
           )
          vertexdir[faces[p][facecorners[1][0]][1+facecorners[1][1]]] = 2;
        else
          vertexdir[faces[p][facecorners[1][0]][1+facecorners[1][1]]] = 1;
      }
    }

    // first let's output the corner faces
    int j = 0;
    for (int i = 0; i < NUM_VERTICES; i++)
    {
      if (vertexdir[i] != -1)
      {
        if (vertexdir[i] == 1)
          fprintf(out, "    { 3,  %i, %i, %i },\n", 3*i, 3*i+1, 3*i+2);
        else
          fprintf(out, "    { 3,  %i, %i, %i },\n", 3*i, 3*i+2, 3*i+1);

        j++;
      }
    }

    fprintf(out, "  },\n");
  }

  fprintf(out, "};\n");

  fclose(out);
}

int main(int /*argv*/, char** /*args[]*/) {

  beveloffset();
}

