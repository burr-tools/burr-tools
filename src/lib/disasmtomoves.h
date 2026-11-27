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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __DISASSMTOMOVES_H__
#define __DISASSMTOMOVES_H__

#include <vector>
#include <memory>

class separation_c;
class disassemblerNode_c;
struct stateRotation_t;

/**
 * this is an abstract class used to define piece positions
 */
class piecePositions_c {

public:

  piecePositions_c(void) {}
  virtual ~piecePositions_c(void) {}

  /** the x-positions of the piece is returned */
  virtual float getX(unsigned int piece) = 0;
  /** the y-positions of the piece is returned */
  virtual float getY(unsigned int piece) = 0;
  /** the z-positions of the piece is returned */
  virtual float getZ(unsigned int piece) = 0;

  /** the alpha value of the piece (0 invisible, 1 opaque) */
  virtual float getA(unsigned int piece) = 0;

  /** piece moving at this time */
  virtual bool moving(unsigned int piece) = 0;

  /**
   * the piece is on its way out of view: its alpha runs from 1 to 0 over
   * the current step. The display composites such pieces over the ones
   * that are coming into view at the same time
   */
  virtual bool fadingOut(unsigned int piece) = 0;

  /** the orientation of the piece, -1 when it keeps the one it is shown with */
  virtual int getTrans(unsigned int /*piece*/) { return -1; }

  /**
   * A piece in the middle of a rotation move: on top of its position, turn
   * it by angle degrees (right-handed) about the axis through pivot, then
   * shift it by shift. All in the drawing units of the positions.
   */
  struct turn_t {
    float angle;
    unsigned int axis;   ///< 0 = X, 1 = Y, 2 = Z
    float pivot[3];
    float shift[3];
  };

  /** true and the turn if the piece is turning right now */
  virtual bool getTurn(unsigned int /*piece*/, turn_t * /*turn*/) { return false; }

  // no copying and assigning
  piecePositions_c(const piecePositions_c&) = delete;
  piecePositions_c& operator=(const piecePositions_c&) = delete;
};

/**
 * this class takes a disassembly tree and generates piecepositions
 * for all pieces at each step of disassembly
 */
class disasmToMoves_c : public piecePositions_c {

  /** the disassembly tree */
  std::unique_ptr<separation_c> tree;

  /**
   * size is used to removed pieces from the puzzle, this value controls
   * how far they are moved out, when they are removed
   */
  unsigned int size;

  /** this array contains the current position and alpha values of all pieces */
  std::vector<float> moves;

  /** this array contains the information, if a piece is currently moving, or not */
  std::vector<bool> mv;

  /** per piece: its alpha is interpolated from 1 down to 0 in the current step */
  std::vector<bool> fading;

  /**
   * the number of the last used piece this is NOT identical with
   * pieceNumber of tree because there might be gaps
   */
  unsigned int maxPieceName;

  /** orientation of every piece, -1 for disassemblies without rotations */
  std::vector<int> trans;

  /** the turn of every piece that is in a rotation move right now */
  std::vector<bool> turning;
  std::vector<turn_t> turns;

  /** what doRecursive learns about a piece besides its position */
  struct pieceInfo_t {
    int trans = -1;
    const stateRotation_t * rotation = nullptr;  ///< rotation leading into the placed state, if the piece turns in it
    int cx = 0, cy = 0, cz = 0;                    ///< display offset of the piece's subtree
  };

  /** this function walks the tree and sets the piece positions and, if
   *  info is given, orientations and rotations */
  int doRecursive(const separation_c * tree, int step, float * array, pieceInfo_t * info,
                  bool center_active, int cx, int cy, int cz);

public:

  /**
   * create class. sz is used when the pieces are removed from the
   * assembled puzzle. The larger the further away the pieces will be
   * moved
   */
  disasmToMoves_c(const separation_c * tr, unsigned int sz, unsigned int maxPiece);

  virtual ~disasmToMoves_c() override;

  /**
   * sets the moves for the step. if the value is not integer you
   * get a intermediate of the necessary move (for animation)
   * if fade out is true, pieces fade out, when they are removed from the
   * rest of the puzzle
   * if center_active if true, the group of pieces that currently is
   * worked on is always in the middle of the display, other groups are invisible
   */
  void setStep(float step, bool fadeOut = true, bool center_active = false);

  virtual float getX(unsigned int piece) override;
  virtual float getY(unsigned int piece) override;
  virtual float getZ(unsigned int piece) override;
  virtual float getA(unsigned int piece) override;
  virtual bool moving(unsigned int piece) override;
  virtual bool fadingOut(unsigned int piece) override;
  virtual int getTrans(unsigned int piece) override;
  virtual bool getTurn(unsigned int piece, turn_t * turn) override;

  // no copying and assigning
  disasmToMoves_c(const disasmToMoves_c&) = delete;
  disasmToMoves_c& operator=(const disasmToMoves_c&) = delete;
};

/** a piece position class with fixed positions */
class fixedPositions_c : public piecePositions_c {

  public:

    fixedPositions_c(const disassemblerNode_c * nd, const std::vector<unsigned int> & pc, unsigned int pcs);
    fixedPositions_c(const fixedPositions_c * nd);
    virtual ~fixedPositions_c(void) override;

    virtual float getX(unsigned int piece) override;
    virtual float getY(unsigned int piece) override;
    virtual float getZ(unsigned int piece) override;
    virtual float getA(unsigned int piece) override;
    virtual bool moving(unsigned int piece) override;
    virtual bool fadingOut(unsigned int piece) override;

    // no copying and assigning
    fixedPositions_c(const fixedPositions_c&) = delete;
    fixedPositions_c& operator=(const fixedPositions_c&) = delete;

  private:

    unsigned int pieces;
    std::vector<int> x, y, z;
    std::vector<bool> visible;
};

#endif
