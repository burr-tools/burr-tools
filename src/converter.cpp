/* program to convert puzzles from old format into the latest one */


#include "lib/puzzle.h"

#include <iostream>
#include <fstream>

using namespace std;

class my_voxel_0 : public voxel_c {

public:

  my_voxel_0(istream * str) : voxel_c(0, 0, 0) {

    int sx, sy, sz;

    *str >> sx >> sy >> sz;

  
    if ((sx < 0) || (sy < 0) || (sz < 0)) {
      printf("wrong size for voxel space");
      exit(1);
    }

    resize(sx, sy, sz, VX_EMPTY);
  
    char c;
  
    for (int j = 0; j < getXYZ(); j++) {
      *str >> c;
  
      switch (c) {
      case '_': setState(j, VX_EMPTY); break;
      case '#': setState(j, VX_FILLED); break;
      case '+': setState(j, VX_VARIABLE); break;
      case '\n':
        while (j < getXYZ()) {
          set(j, VX_EMPTY);
          j++;
        }
        return;
      default:
        printf("not allowed character in voxel space definition");
        exit(1);
      }
    }
  }

};




class my_puzzle_0 : public puzzle_c {


  /* load the puzzle from the file
   *
   * the format is quite simple:
   * first line is x y z size of the assembled block separated by space
   * all other lines stand one line for one piece
   * the first 3 number are the dimensions followed by a string in ""
   * in this string everything else esxept for space is asumed to be filled
   * the string is a linear list of all blocks inside the piece block
   * with x the lowest and z the highest dimension. So the formula to get
   * the position of one block is x+xs*(y+ys*z)
   */

public:
  my_puzzle_0(istream * str) {

    unsigned int prob = addProblem();
    probSetName(prob, "Problem");

    voxel_c * v = new my_voxel_0(str);
    assert(v);
    probSetResult(prob, addShape(v));
  
    int pieces;
  
    *str >> pieces;
  
    if ((pieces < 0) || (pieces > 500)) {
      printf("too many pieces in file? probably voxel space not defined correctly");
      exit(1);
    }
  
    while (pieces > 0) {
  
      int nr;
  
      *str >> nr;
  
      if ((nr < 0) || (nr > 500)) {
        printf("too many instances of one piece? probably voxel space not defined correctly");
        exit(1);
      }
  
      v = new my_voxel_0(str);
      assert(v);

      probAddShape(prob, addShape(v), nr);

      pieces--;
    }
  }
};

/* return values:
 * 0: ok
 * 1: the number of defined problems must be one
 */

int PS3Dsave(const puzzle_c & p, std::ostream * str) {

#if 0
  if (p.problemNumber() != 1)
    return 1;

  for (unsigned int i = 0; i < shapes.size(); i++) {
    *str << "PIECE " << shapes[i].piece->getX() << "," << shapes[i].piece->getY() << "," << shapes[i].piece->getZ() << std::endl;
    for (int y = 0; y < shapes[i].piece->getY(); y++) {
      for (int z = 0; z < shapes[i].piece->getZ(); z++) {
        for (int x = 0; x < shapes[i].piece->getX(); x++) {
          if (shapes[i].piece->getState(x, y, z) != voxel_c::VX_EMPTY)
            *str << char('A' + i);
          else
            *str << ' ';
        }
        if (z < shapes[i].piece->getZ() - 1)
          *str << ",";
      }
      *str << std::endl;
    }
  }

  *str << "RESULT " << results[0]->getX() << "," << results[0]->getY() << "," << results[0]->getZ() << std::endl;
  for (int y = 0; y < results[0]->getY(); y++) {
    for (int z = 0; z < results[0]->getZ(); z++) {
      for (int x = 0; x < results[0]->getX(); x++) {
        if (results[0]->getState(x, y, z) != voxel_c::VX_EMPTY)
          *str << 'A';
        else
          *str << ' ';
      }
      if (z < results[0]->getZ() - 1)
        *str << ",";
    }
    *str << std::endl;
  }
#endif
}




int main(int argn, char * args[]) {

  /* load the puzzle */

  // there are 2 mayor puzzle formats: old one and xml
  // they are differentiated by extension. the xml
  // format has the extension puzxml, the old one puzzle

  ofstream out(args[2]);
  ifstream in(args[1]);

  puzzle_c * p = new my_puzzle_0(&in);

  out << p->save();
}
