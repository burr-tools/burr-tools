/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#include "assembler_0.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

void assembler_0_c::GenerateFirstRow(int res_filled) {
  for (int i = 0; i < varivoxelStart; i++) {
    right[i] = i+1;
    left[i+1] = i;
    up[i] = i;
    down[i] = i;
    colCount[i] = 0;
  }

  /* make the linked list cyclic */
  left[0] = varivoxelStart - 1;
  right[varivoxelStart - 1] = 0;

  /* create column nodes for vari columns, these
   * are not inside the column header list and so
   * are not forced to be filled but otherwise behave
   * like normal columns
   */
  for (int j = varivoxelStart; j < varivoxelEnd; j++) {
    left[j+1] = j;
    right[j] = j+1;
    up[j] = j;
    down[j] = j;
    colCount[j] = 0;
  }
  left[varivoxelStart] = varivoxelEnd;
  right[varivoxelEnd] = varivoxelStart;

  /* first free node, we'll start to fill up starting from there */
  firstfree = 2 + piecenumber + res_filled;
}

void assembler_0_c::AddFillerNode(void ) {
  firstfree++;
}

int assembler_0_c::AddPieceNode(int piece) {
  int piecenode = firstfree++;

  left[piecenode] = piecenode;
  right[piecenode] = piecenode;
  down[piecenode] = piece+1;
  up[piecenode] = up[piece+1];
  down[up[piece+1]] = piecenode;
  up[piece+1] = piecenode;

  colCount[piecenode] = piece+1;
  colCount[piece+1]++;

  return piecenode;
}

void assembler_0_c::AddVoxelNode(int col, int piecenode) {
  int newnode = firstfree++;

  right[newnode] = piecenode;
  left[newnode] = left[piecenode];
  right[left[piecenode]] = newnode;
  left[piecenode] = newnode;

  down[newnode] = col;
  up[newnode] = up[col];
  down[up[col]] = newnode;
  up[col] = newnode;

  colCount[newnode] = col;
  colCount[col]++;
}

void assembler_0_c::nextPiece(int piece, int count, int number) {
  multiPieceCount[piece] = count;
  multiPieceIndex[piece] = number;
  pieceStart[piece] = firstfree;
}

assembler_0_c::assembler_0_c() :
  rows(0), columns(0), left(0), right(0), up(0), down(0), colCount(0), searchState(0), nodeF(0),
  numF(0), pieceF(0), nodeB(0), numB(0), piece(0), pieceStart(0), multiPieceCount(0), multiPieceIndex(0)
{
}

assembler_0_c::~assembler_0_c() {
  if (rows) delete [] rows;
  if (columns) delete [] columns;
  if (left) delete [] left;
  if (right) delete [] right;
  if (up) delete [] up;
  if (down) delete [] down;
  if (colCount) delete [] colCount;
  if (multiPieceCount) delete [] multiPieceCount;
  if (pieceStart) delete [] pieceStart;
  if (multiPieceIndex) delete [] multiPieceIndex;
  if (nodeF) delete [] nodeF;
  if (numF) delete [] numF;
  if (pieceF) delete [] pieceF;
  if (nodeB) delete [] nodeB;
  if (numB) delete [] numB;
  if (piece) delete [] piece;
  if (searchState) delete [] searchState;
}

void assembler_0_c::createMatrix(const puzzle_c * p) {

  puzzle_c puz(p);

  /* get and save piecenumber of puzzle */
  piecenumber = puz.getPieces();

  // minimize all pieces
  for (int i = 0; i < puz.getShapeNumber(); i++)
    puz.getShape(i)->minimizePiece();

  /* count the filled and variable units */
  int res_vari = puz.getResult()->countState(pieceVoxel_c::VX_VARIABLE);
  int res_filled = puz.getResult()->countState(pieceVoxel_c::VX_FILLED) + res_vari;

  varivoxelStart = 1 + piecenumber + res_filled - res_vari;
  varivoxelEnd = 1 + piecenumber + res_filled;

  // check if number of voxels in pieces is not bigger than
  // number of voxel in result

  // check if number of filled voxels in result
  // is not bigger than number of voxels in pieces
  holes = res_filled;

  for (int j = 0; j < puz.getShapeNumber(); j++)
    holes -= puz.getShape(j)->countState(pieceVoxel_c::VX_FILLED) * puz.getShapeCount(j);

  if (holes < 0) {
    errorsState = 1;
    errorsParam = -holes;
    return;
  }

  if (holes > res_vari) {
    errorsState = 2;
    errorsParam = holes-res_vari;
    return;
  }

  /* count the number of required nodes*/
  unsigned long nodes = countNodes(&puz);

  // check, if there is one piece unplacable
  if (nodes <= 0) {
    errorsState = 3;
    errorsParam = nodes + 1;
    return;
  }

  /* countNode only counts inner node, so add the title row */
  nodes += 2 + piecenumber + res_filled;

  /* allocate all the required memory */
  rows = new int[piecenumber];
  columns = new int [piecenumber];
  nodeF = new int [piecenumber];
  numF = new int [piecenumber];
  pieceF = new int [piecenumber];
  nodeB = new int [piecenumber];
  numB = new int [piecenumber];
  piece = new int [piecenumber];
  searchState = new int [piecenumber + 1];

  searchState[0] = 0;

  pieceStart = new int[piecenumber];
  multiPieceCount = new int[piecenumber];
  multiPieceIndex = new int[piecenumber];
  left = new int [nodes];
  right = new int [nodes];
  up = new int [nodes];
  down = new int [nodes];
  colCount = new int [nodes];

  /* fill the nodes arrays */
  prepare(&puz, res_filled, res_vari);

  memset(rows, 0xff, piecenumber * sizeof(int));
  memset(columns, 0, piecenumber * sizeof(int));
  pos = 0;
  iterations = 0;
  errorsState = 0;
}


/* remove column from array, and also all the rows, where the column is one */
void assembler_0_c::cover(register int col)
{
  {
    register int l, r;

    l = left[col];
    r = right[col];

    left[r] = l;
    right[l] = r;
  }

  register int i = down[col];
  while (i != col) {

    register int j = right[i];
    while (j != i) {

      {
        register int u, d;

        u = up[j];
        d = down[j];

        up[d] = u;
        down[u] = d;
      }

      colCount[colCount[j]]--;

      j = right[j];
    }

    i = down[i];
  }
}

/* remove column from array, and also all the rows, where the column is one */
bool assembler_0_c::try_cover(register int col, int * columns)
{
  {
    register int l, r;

    l = left[col];
    r = right[col];

    left[r] = l;
    right[l] = r;
  }

  bool result = true;

  register int i = down[col];
  while (i != col) {

    register int j = right[i];
    while (j != i) {

      {
        register int u, d;

        u = up[j];
        d = down[j];

        up[d] = u;
        down[u] = d;
      }

      colCount[colCount[j]]--;

      if ((colCount[colCount[j]] == 0) && (columns[colCount[j]] == 0))
        result = false;

      j = right[j];
    }

    i = down[i];
  }

  return result;
}

void assembler_0_c::uncover(register int col) {

  register int i = up[col];
  while (i != col) {

    register int j = left[i];
    while (j != i) {

      colCount[colCount[j]]++;

      up[down[j]] = j;
      down[up[j]] = j;

      j = left[j];
    }

    i = up[i];
  }

  left[right[col]] = col;
  right[left[col]] = col;
}

/* remove all the columns from the matrix in which the given
 * row contains ones
 */
void assembler_0_c::cover_row(register int r) {
  register int j = right[r];
  while (j != r) {
    cover(colCount[j]);
    j = right[j];
  }
}

bool assembler_0_c::try_cover_row(register int r) {

  int *columns = new int[varivoxelEnd];
  memset(columns, 0, varivoxelEnd * sizeof(int));

  register int j = right[r];
  while (j != r) {
    columns[colCount[j]] = 1;
    j = right[j];
  }

  j = right[varivoxelEnd];
  while (j != varivoxelEnd) {
    columns[j] = 1;
    j = right[j];
  }


  j = right[r];
  while (j != r) {

    if (!try_cover(colCount[j], columns)) {
      do {
        uncover(colCount[j]);
        j = left[j];
      } while (j != r);

      delete [] columns;
      return false;
    }

    j = right[j];
  }

  delete [] columns;
  return true;
}

void assembler_0_c::uncover_row(register int r) {
  register int j = left[r];
  while (j != r) {
    uncover(colCount[j]);
    j = left[j];
  }
}

bool assembler_0_c::checkmatrix(int rec, int branch) {

  /* check the number of holes, if they are larger than allowed return */
  int j = right[varivoxelEnd];
  int count = holes;
  while (j != varivoxelEnd) {
    if (colCount[j] == 0) {
      count--;
      if (count < 0)
        return true;
    }
    j = right[j];
  }

  int col = right[0];

  while (col) {
    /* if there is a column that can not be covered
     * any longer, the whole problem can't be solved
     */
    if (colCount[col] == 0)
      return true;

    /* if one column must be covered in less than branch
     * level, check all these possibilities
     */
    if (rec && (colCount[col] <= branch)) {

      cover(col);

      int r = down[col];

      while (r != col) {

        if (!try_cover_row(r)) {
  
          uncover(col);
          return true;
  
        }
  
        bool ret = checkmatrix(rec - 1, branch);
        uncover_row(r);

        if (ret) {
          uncover(col);
          return true;
        }

        r = down[r];
      }

      uncover(col);
    }

    col = right[col];
  }

  return false;
}

void assembler_0_c::reduce(void) {
  int removed = 0;
  bool rem_sth;
  int iteration = 1;

  int rec = 0;
  int branch = 0;

  do {

    rem_sth = false;

    /* check all the pieces */
    for (int p = 0; p < piecenumber;) {

      // place the piece and check, if this leads to some
      // infillable holes or unplacable pieces or whaever
      // conditions that make a solution impossible

      cover(p+1);

      // we first collect all the rows that we finally want to
      // remove and only remove them after the complete check
      int * rowsToRemove = new int[colCount[p+1]];
      int countToRemove = 0;

      // go over all the placements of the piece and check, if
      // each for possibility
      int r = down[p+1];
      int row = 0;
      while (r != p+1) {

        // try to do this placement, if the placing goes
        // wrong already, we don't need to do the deep check
        if (!try_cover_row(r)) {
          rowsToRemove[countToRemove++] = row;
        } else {

          /* and check if that results in a dead end */
          if (checkmatrix(rec, branch))
            rowsToRemove[countToRemove++] = row;

          uncover_row(r);
        }

        r = down[r];
        row++;
      }

      uncover(p+1);

      int count = multiPieceCount[p];

      for (int pc = 0; pc < 1/*count*/; pc++) {

        int r = down[p+1];
        int row = 0;

        for (int rem = 0; rem < countToRemove; rem++) {

          while (row < rowsToRemove[rem]) {
            r = down[r];
            row++;
          }

          int s = r;
          int rr = down[r];

          do {
            up[down[s]] = up[s];
            down[up[s]] = down[s];
            colCount[colCount[s]]--;
            s = right[s];
          } while (s != r);

          r = rr;
          row++;
  
          removed++;
        }

        p++;
      }

      if (countToRemove)
        rem_sth = true;

      delete [] rowsToRemove;
    }

    iteration++;
    rec++;
    branch = 1;

  } while (rem_sth);
}

/* to understand this function you need to first completely understand the
 * dancing link algorithm.
 * The next thing to know is how this alg. avoids finding the solutions
 * with identical pieces with all permutations. This is done by enforcing
 * an order of the pieces. The placements and the pieces are enumerated and
 * pieces with a smaller number must have a placement with a smaller number.
 * Then you need to know the layout of the matrix used here. It's exactly as
 * describes by Knuth. First columns for pieces. Followed by the columns for
 * the normal result voxels followed by the columnns for the variable
 * pieces. All placements for one piece are one below the other. One piece
 * follows after another. The nodes are enumerated line by line.
 * This fixed order allows for simple checks of the placement numbers against
 * each other (only relative comparisons are possible and necessary) by
 * knowing the piece this row belongs to and knowing the first node that
 * belongs to this piece and comparing the distances to this first node
 * for the two placements of the two pieces
 */
void assembler_0_c::iterativeMultiSearch(void) {

  abbort = false;
  running = true;

  // this variable is used to shor if we continue with our loop over
  // the rows or have finished
  bool cont;

  while (!abbort) {

    // we have finished if pos is minus one
    if (pos == -1)
      break;

    // check, if all pieces are placed and all voxels are filled
    // careful here abbort is also modified by another thread
    // i once used the expression
    //   abbort |= !solution()
    // and search halve a day why it didn't work. The value of abbort was read
    // then the function called then the new value calculated then the new value
    // written. Meanwhile abbort was pressed and abbort was changed. this new value got lost.
    if (!right[0])
      if (!solution())
        abbort = true;

    // if all pieces are placed we can not go on even if there are
    // more columns that need attention, all of them should be
    // empty any ways, so we backtrack once and continue there
    if (pos == piecenumber)
      pos--;

    cont = false;
    iterations++;

    if (rows[pos] == -1) {

      // start with a new column
      if (searchState[pos] == 0) {
        /* search the best column for the next recursion step
         *
         * we actually do a bit more:
         * we find the column with the smallest column count. This column is selected
         * from the piece columns and the normal result columns
         *
         * we also look for piece and result columns that have a count of 0 that value
         * will lead to impossible arrangements
         *
         * FIXME: possible improvement
         * finally we count the number of holes by checking the variable result voxels
         * that can not be filled any longer. that number must be smaller than the number of holes
         */
        int c = right[0];
        int s = colCount[c];

        if (s) {
          register int j = right[c];

          while (j) {
  
            if (colCount[j] < s) {
              c = j;
              s = colCount[c];
  
              if (!s)
                break;
            }
  
            j = right[j];
          }
        }

        // now check for the holes, only the variable columns can be unfillable
        // but there must not be more unfillable voxels than there are holes
        // if that is the case we can backtrack
        // sometimes this doesnt help much, but it also seems like
        // it doesn't cost a lot of time, so let's keep it in for the moment
        if (s) {
          int currentHoles = holes;
          register int j = right[varivoxelEnd];

          while (j != varivoxelEnd) {
            if (colCount[j] == 0) {
              currentHoles--;
              if (currentHoles < 0) {
                s = 0;
                break;
              }
            }
            j = right[j];
          }
        }

        if (s) {
          // we have found a valid column, start a search
          columns[pos] = c;
          piece[pos] = 0;
          rows[pos] = down[columns[pos]];
          cont = true;
        }
      } else {
        // this is the branch for the case we are forced to place certain pieces
        // this happens, if we placed a piece with a shape used by more than one piece
        // we do not need to search for a good column here as we already know wich
        // column we are going to place, but we must check, if there is one column
        // with a count of 0 so we can back track
        int j = right[0];

        do {
          if (!colCount[j])
            break;
          j = right[j];
        } while (j);

        if (!j) {
          // select the column. We can either go backwards, if the last piece
          // placed was not the first one, or forward until we have placed
          // all multi pieces
          columns[pos] = piece[pos] + 1;
          rows[pos] = down[columns[pos]];
          cont = true;

          // in case of the forward place mode we have to find a row with a placement larger than
          // that of the last piece
          if (searchState[pos] == 1) {
            while ((rows[pos] - pieceStart[piece[pos]]) <= nodeF[pos]) {
              rows[pos] = down[rows[pos]];
              // we have searched all the rows and none fits so we can backtrack
              if (rows[pos] == columns[pos]) {
                cont = false;
                break;
              }
            }
          } else if ((rows[pos] - pieceStart[piece[pos]]) >= nodeB[pos])
            // searchState can only be 2 by now meaning backward search
            // if the row is already bigger than the last we don't even need to start
            cont = false;
        }
      }

      // found no fitting row, or column with zero count
      if (!cont) {
        rows[pos] = -1;
        pos--;
        continue;
      }

      cover(columns[pos]);

    } else {

      // continue on a column we have already started, this is inside the loop in the
      // recourseve function, after we return from the recoursive call
      // we uncover our row, find the next one and continue, if there is a new row
      uncover_row(rows[pos]);
      rows[pos] = down[rows[pos]];
      cont = (rows[pos] != columns[pos]);

      // check, if the this row if grater than the last alowed row, this is the abbort
      // criterium in backwards search as ther will be no morw valid rows
      if (cont && (searchState[pos] == 2) && ((rows[pos] - pieceStart[piece[pos]]) >= nodeB[pos]))
        cont = false;
    }

    // check, if we continue, if we have found a new row to cover
    if (cont) {

      // cover the row
      cover_row(rows[pos]);

      // the next lines with the crazy if-nesting sets up the parameters for the next
      // recoursion step, this is different depending on the state we are in if there
      // are more pieces in front or behind us...

      // let's assume that the next recursion step is the simple one
      // the value will be changed if required
      searchState[pos+1] = 0;

      if (searchState[pos] == 0) {

        while ((piece[pos] < piecenumber-1) && (rows[pos] >= pieceStart[piece[pos]+1])) piece[pos]++;
 
        // now depending on the properties of the current piece set up the variables for the
        // next recoursion step.
        if (multiPieceCount[piece[pos]] != 1) {

          // is is a multi piece, so we have to at least go forward until we have placed all
          // the multi pieces
          nodeF[pos+1] = rows[pos] - pieceStart[piece[pos]];
          numF[pos+1] = multiPieceCount[piece[pos]] - 1 - multiPieceIndex[piece[pos]];

          if (multiPieceIndex[piece[pos]]) {
            // if the piece just placed is not the first one in the set we also have to go
            // backwards and place all the pieces in front of the current on
            nodeB[pos+1] = rows[pos] - pieceStart[piece[pos]];
            numB[pos+1] = multiPieceIndex[piece[pos]];
            piece[pos+1] = piece[pos] - 1;
            pieceF[pos+1] = piece[pos] + 1;

            searchState[pos+1] = 2;

          } else {

            piece[pos+1] = piece[pos] + 1;
            searchState[pos+1] = 1;
          }
        }
      } else if (searchState[pos] == 1) {

        // we are moving forward, so let's check, if there are more peces to place
        if (numF[pos] > 1) {

          // there are more pieces, do let's do another forward step
          nodeF[pos+1] = rows[pos]-pieceStart[piece[pos]];
          numF[pos+1] = numF[pos] - 1;
          piece[pos+1] = piece[pos] + 1;

          searchState[pos+1] = 1;
        }
      } else if ((numB[pos] > 1) || (numF[pos] > 0)) {

        // if there are still pieces in front of us or behind us we need to continue somewhere
        nodeF[pos+1] = nodeF[pos];
        numF[pos+1] = numF[pos];

        if (numB[pos] > 1) {

          // there is also space behind us
          nodeB[pos+1] = rows[pos]-pieceStart[piece[pos]];
          numB[pos+1] = numB[pos] - 1;
          piece[pos+1] = piece[pos] - 1;

          pieceF[pos+1] = pieceF[pos];

          searchState[pos+1] = 2;

        } else {

          searchState[pos+1] = 1;
          piece[pos+1] = pieceF[pos];
        }
      }
      pos++;

    } else {

      // ok finished this column, uncover it and backtrack
      uncover(columns[pos]);
      rows[pos] = -1;
      pos--;
    }
  }

  running = false;
}

void assembler_0_c::assemble(assembler_cb * callback) {

  if (errorsState == 0) {
    asm_bc = callback;
    iterativeMultiSearch();
  }
}

float assembler_0_c::getFinished(void) {

  /* we don't need locking, as I hope that I have written the
   * code in a way that updated the data so, that it will never
   * be in an inconsistant state. The thing that will happen is that
   * the value may jump
   */
  float erg = 0;

  if (pos == -1)
    return 1;

  for (int i = pos - 1; i >= 0; i--) {

    int r = rows[i];
    int l = colCount[columns[i]];

    while ((l) && (r != -1) && (r != down[columns[i]])) {
      erg += 1;
      r = up[r];
      l--;
    }

    erg /= colCount[columns[i]];
  }

  return erg;
}

bool assembler_0_c::getPosition(char * string, int len) {

  int i;

  i = snprintf(string, len, "%i ", pos);

  if (i >= len)
    return false;
  else {
    len -= i;
    string += i;
  }

  for (int j = 0; j < pos; j++) {

    i = snprintf(string, len, "%i %i ", rows[j], columns[j]);
  
    if (i >= len)
      return false;
    else {
      len -= i;
      string += i;
    }
  }

  return true;
}

static int getInt(char * s, int * i) {

  int p = 0;
  i = 0;

  while (s[p] && (s[p] != ' ')) {

    *i = *i * 10 + s[p] - '0';
    p++;
  }

  while (s[p] && (s[p] == ' ')) {
    p++;
  }

  return p;
}

void assembler_0_c::setPosition(char * string) {

  /* uncover everything and so bring the matrix into starting position
   * this will normally do nothing as this function should only be called
   * on a fresh instance of the class but just in case someone
   * calls setPosition on a already started search we need it
   */
  while (pos > 0) {

    if (rows[pos] != -1) {
      uncover_row(rows[pos]);
      uncover(columns[pos]);
      rows[pos] = -1;
    }

    pos--;
  }

  /* get the values from the string. For the moment we assume that
   * the string is correct and contains enough values, if not sad things
   * will happen
   * FIXME
   */
  string += getInt(string, &pos);

  for (int i = 0; i < pos; i++) {
    string += getInt(string, &rows[i]);
    string += getInt(string, &columns[i]);
  }

  /* here we need to get the matrix into this exact position as it has been, when we
   * saved the position that means we need to cover all rows and columns in the same
   * order as it happened in the original process
   */
  int p = 0;
  while (p < pos) {

    if (rows[pos] != -1) {
      cover(columns[p]);
      cover_row(rows[p]);
    }

    p++;
  }
}


const char * assembler_0_c::errors(void) {

  static char tmp[500];

  switch (errorsState) {
  case 0: return 0;
  case 1:
    snprintf(tmp, 500, "There are %i more cubes than there is space in result!", errorsParam);
    return tmp;
  case 2:
    snprintf(tmp, 500, "There are %i less cubes than we need in result space!", errorsParam);
    return tmp;
  case 3:
    snprintf(tmp, 500, "There is no room for piece %i in the solution shape!", errorsParam);
    return tmp;
  }

  return 0;
}

