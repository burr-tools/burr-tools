#include <movementcache.h>

static unsigned int hashValue(unsigned int s1, unsigned int s2, int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int tableSize) {
  unsigned int val = dx * 0x10101010;
  val +=             dy * 0x14814814;
  val +=             dz * 0x95145951;
  val +=             t1 * 0x1A54941A;
  val +=             t2 * 0x5AA59401;
  val +=             s1 * 0x01059a04;
  val +=             s2 * 0x9af42682;
  return val % tableSize;
}

int min(int a, int b) { if (a < b) return a; else return b; }
int max(int a, int b) { if (a > b) return a; else return b; }

void movementCache::rehash(void) {
  unsigned int oldSize = tableSize;

  tableSize = 2* tableSize + 1;

  entry ** newHash = new entry * [tableSize];
  memset(newHash, 0, tableSize * sizeof(entry*));

  for (unsigned int i = 0; i < oldSize; i++)
    while (hash[i]) {
      entry * e = hash[i];
      hash[i] = e->next;

      unsigned int h = hashValue(e->s1, e->s2, e->dx, e->dy, e->dz, e->t1, e->t2, tableSize);
      e->next = newHash[h];
      newHash[h] = e;
    }

  delete [] hash;
  hash = newHash;
}

movementCache::entry * movementCache::calcValues(unsigned char s1, unsigned char t1,
                                                 unsigned int s2, unsigned int t2,
                                                 int dx, int dy, int dz) {
  const pieceVoxel_c * sh1 = shapes[s1][t1];

  if (!sh1) {
    sh1 = new pieceVoxel_c(shapes[s1][0], t1);
    shapes[s1][t1] = sh1;
  }

  const pieceVoxel_c * sh2 = shapes[s2][t2];

  if (!sh2) {
    sh2 = new pieceVoxel_c(shapes[s2][0], t2);
    shapes[s2][t2] = sh2;
  }

  entry * e = new entry;

  e->dx = dx;
  e->dy = dy;
  e->dz = dz;

  e->t1 = t1;
  e->t2 = t2;

  e->s1 = s1;
  e->s2 = s2;

  int x1i, x2i, y1i, y2i, z1i, z2i;

  x1i = max(sh1->boundX1(), sh2->boundX1() + dx);
  x2i = min(sh1->boundX2(), sh2->boundX2() + dx);
  y1i = max(sh1->boundY1(), sh2->boundY1() + dy);
  y2i = min(sh1->boundY2(), sh2->boundY2() + dy);
  z1i = max(sh1->boundZ1(), sh2->boundZ1() + dz);
  z2i = min(sh1->boundZ2(), sh2->boundZ2() + dz);

  int x1u, x2u, y1u, y2u, z1u, z2u;

  x1u = min(sh1->boundX1(), sh2->boundX1() + dx);
  x2u = max(sh1->boundX2(), sh2->boundX2() + dx);
  y1u = min(sh1->boundY1(), sh2->boundY1() + dy);
  y2u = max(sh1->boundY2(), sh2->boundY2() + dy);
  z1u = min(sh1->boundZ1(), sh2->boundZ1() + dz);
  z2u = max(sh1->boundZ2(), sh2->boundZ2() + dz);

  int mx, my, mz;

  mx = my = mz = 32000;

  for (int y = y1i; y <= y2i; y++)
    for (int z = z1i; z <= z2i; z++) {

      int last = -32000;

      for (int x = x1u; x <= x2u; x++) {

        assert(sh1->isEmpty2(x, y, z) || sh2->isEmpty2(x-dx, y-dy, z-dz));

        if (sh1->isFilled2(x, y, z))
          last = x;
        else if (sh2->isFilled2(x-dx, y-dy, z-dz) && (x-last-1 < mx))
          mx = x-last-1;
      }
    }

  for (int x = x1i; x <= x2i; x++)
    for (int z = z1i; z <= z2i; z++) {

      int last = -32000;

      for (int y = y1u; y <= y2u; y++)
        if (sh1->isFilled2(x, y, z))
          last = y;
        else if (sh2->isFilled2(x-dx, y-dy, z-dz) && (y-last-1 < my))
          my = y-last-1;
    }

  for (int x = x1i; x <= x2i; x++)
    for (int y = y1i; y <= y2i; y++) {

      int last = -32000;

      for (int z = z1u; z <= z2u; z++)
        if (sh1->isFilled2(x, y, z))
          last = z;
        else if (sh2->isFilled2(x-dx, y-dy, z-dz) && (z-last-1 < mz))
          mz = z-last-1;
    }

  assert((mx >= 0) && (my >= 0) && (mz >= 0));

  e->mx = mx;
  e->my = my;
  e->mz = mz;

  return e;
}

movementCache::movementCache(const puzzle_c * puzzle, unsigned int problem) {

  tableSize = 101;
  hash = new entry * [tableSize];

  memset(hash, 0, tableSize * sizeof(entry*));

  entries = 0;

  num_shapes = puzzle->probShapeNumber(problem);

  shapes = new const pieceVoxel_c ** [num_shapes];
  for (unsigned int s = 0; s < num_shapes; s++) {
    shapes[s] = new const pieceVoxel_c * [NUM_TRANSFORMATIONS];
    memset(shapes[s], 0, NUM_TRANSFORMATIONS * sizeof(pieceVoxel_c*));
    shapes[s][0] = puzzle->probGetShapeShape(problem, s);
  }

  pieces = new unsigned int [puzzle->probPieceNumber(problem)];

  int pos = 0;

  for (unsigned int s = 0; s < puzzle->probShapeNumber(problem); s++)
    for (unsigned int i = 0; i < puzzle->probGetShapeCount(problem, s); i++)
      pieces[pos++] = s;
}

movementCache::~movementCache() {

  for (unsigned int i = 0; i < tableSize; i++)
    while (hash[i]) {
      entry * e = hash[i];
      hash[i] = e->next;

      delete e;
    }

  delete [] hash;
  delete [] pieces;

  for (unsigned int s = 0; s < num_shapes; s++) {
    for (unsigned int t = 1; t < NUM_TRANSFORMATIONS; t++)
      if (shapes[s][t])
        delete shapes[s][t];
    delete [] shapes[s];
  }
  delete [] shapes;
}

void movementCache::getValue(int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int p1, unsigned int p2,
         int * mx, int * my, int * mz) {

  unsigned int s1 = pieces[p1];
  unsigned int s2 = pieces[p2];

  unsigned int h = hashValue(s1, s2, dx, dy, dz, t1, t2, tableSize);

  entry * e = hash[h];

  while (e && ((e->dx != dx) || (e->dy != dy) || (e->dz != dz) ||
               (e->t1 != t1) || (e->t2 != t2) || (e->s1 != s1) || (e->s2 != s2)))
    e = e->next;


  if (!e) {

    entries++;
    if (entries > tableSize) {
      rehash();
      h = hashValue(s1, s2, dx, dy, dz, t1, t2, tableSize);
    }

    /* calculate values */
    e = calcValues(s1, t1, s2, t2, dx, dy, dz);
    e->next = hash[h];
    hash[h] = e;
  }

  *mx = e->mx;
  *my = e->my;
  *mz = e->mz;
}

