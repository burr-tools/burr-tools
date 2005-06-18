#include "print.h"

void print(const voxel_c * v, char base) {
  for (unsigned int z = 0; z < v->getZ(); z++) {
    printf(" +");
    for (unsigned int x = 0; x < v->getX(); x++)
      printf("-");
    printf("+");
  }
  printf("\n");

  for (unsigned int y = 0; y < v->getY(); y++) {
    for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" !");
      for (unsigned int x = 0; x < v->getX(); x++)
        if (v->get(x, y, z) != 0)
          printf("%c", base + v->get(x, y, z)-1);
        else
          printf(" ");
      printf("!");
    }
    printf("\n");
  }

  { for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" +");
      for (unsigned int x = 0; x < v->getX(); x++)
        printf("-");
      printf("+");
    }
  }
  printf("\n");
}

void print(const assemblyVoxel_c * v) {
  for (unsigned int z = 0; z < v->getZ(); z++) {
    printf(" +");
    for (unsigned int x = 0; x < v->getX(); x++)
      printf("-");
    printf("+");
  }
  printf("\n");

  for (unsigned int y = 0; y < v->getY(); y++) {
    for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" !");
      for (unsigned int x = 0; x < v->getX(); x++)
        if (v->get(x, y, z) != assemblyVoxel_c::VX_EMPTY)
          printf("%c", 'a' + v->get(x, y, z));
        else
          printf(" ");
      printf("!");
    }
    printf("\n");
  }

  { for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" +");
      for (unsigned int x = 0; x < v->getX(); x++)
        printf("-");
      printf("+");
    }
  }
  printf("\n");
}

void print(const pieceVoxel_c * v) {
  for (unsigned int z = 0; z < v->getZ(); z++) {
    printf(" +");
    for (unsigned int x = 0; x < v->getX(); x++)
      printf("-");
    printf("+");
  }
  printf("\n");

  for (unsigned int y = 0; y < v->getY(); y++) {
    for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" !");
      for (unsigned int x = 0; x < v->getX(); x++)
        if (v->getState(x, y, z) != pieceVoxel_c::VX_EMPTY)
          if (v->getState(x, y, z) == pieceVoxel_c::VX_FILLED)
            printf("#");
          else
            printf("+");
        else
          printf(" ");
      printf("!");
    }
    printf("\n");
  }

  { for (unsigned int z = 0; z < v->getZ(); z++) {
      printf(" +");
      for (unsigned int x = 0; x < v->getX(); x++)
        printf("-");
      printf("+");
    }
  }
  printf("\n");
}


void print(const puzzle_c * p) {

  for (unsigned int s = 0; s < p->shapeNumber(); s++) {
    printf("shape %i:\n", s);
    print(p->getShape(s));
  }

  printf("=======================================================\n");

  for (unsigned int pr = 0; pr < p->problemNumber(); pr++) {
    printf("problem %i (%s):\n", pr, p->probGetName(pr).c_str());
    printf(" result shape: %i\n", p->probGetResult(pr));

    for (unsigned int sh = 0; sh < p->probShapeNumber(pr); sh++)
      if (p->probGetShapeCount(pr, sh) > 1)
        printf(" piece shape: %i times shape number %i\n", p->probGetShapeCount(pr, sh), p->probGetShape(pr, sh));
      else
        printf(" piece shape: %i\n", p->probGetShape(pr, sh));

    printf("-------------------------------------------------------\n");
  }
}

void print(const state_c * s, const assemblyVoxel_c *start, const separation_c * sep, unsigned int piecenumber) {

  for (unsigned int i = 0; i < piecenumber; i++)
    printf("%c(%i; %i; %i), ", 'a'+sep->getPieceName(i), s->getX(i), s->getY(i), s->getZ(i));

  printf("\n");

  for (unsigned int y = 0;  y < 2 * start->getY(); y++) {
    for (unsigned int z = 0; z < 2 * start->getZ(); z++) {
      for (unsigned int x = 0; x < 2 * start->getX(); x++) {

        bool foundpiece = false;

        for (unsigned int p = 0; p < piecenumber; p++) {
          unsigned int x1 = x - s->getX(p) - start->getX()/2;
          unsigned int y1 = y - s->getY(p) - start->getY()/2;
          unsigned int z1 = z - s->getZ(p) - start->getZ()/2;

          if ((x1 < start->getX()) &&
              (y1 < start->getY()) &&
              (z1 < start->getZ()))
            if (start->get(x1, y1, z1) == sep->getPieceName(p)) {
              printf("%c", 'a' + sep->getPieceName(p));
              foundpiece = true;
            }
        }

        if (!foundpiece) printf(" ");
      }

      printf(" | ");
    }

    printf("\n");
  }

  printf("\n\n");
}

void print(const separation_c * s, const assemblyVoxel_c * start) {
  for (unsigned int i = 0; i <= s->getMoves(); i++)
    print(s->getState(i), start, s, s->getPieceNumber());

  printf(" puzzle separates into  2 parts\n");

  if (s->getRemoved()) print(s->getRemoved(), start);
  if (s->getLeft()) print(s->getLeft(), start);
}

