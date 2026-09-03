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
   * Orientation of the piece, or (unsigned int)-1 if unchanged / not provided.
   * Used when disassembly includes rotation moves.
   */
  virtual unsigned int getTrans(unsigned int /*piece*/) { return (unsigned int)-1; }

  /**
   * If the piece is mid-rotation animation, fill continuous tumble parameters.
   * angleDeg is the current interpolation angle (0..±90).
   * Pivot is the world-space voxel used as centre (rotate about its cube centre).
   * Returns false if the piece is not currently animating a rotation.
   */
  virtual bool getRotationAnim(unsigned int /*piece*/,
                               float * /*angleDeg*/,
                               float * /*axisX*/, float * /*axisY*/, float * /*axisZ*/,
                               float * /*pivotX*/, float * /*pivotY*/, float * /*pivotZ*/) {
    return false;
  }

private:

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

  /** orientations for each piece (start mesh while a rotation is in progress) */
  std::vector<unsigned int> orients;

  /** per-piece mid-rotation animation (angle 0 = inactive) */
  std::vector<float> rotAngle;
  std::vector<float> rotAxisX;
  std::vector<float> rotAxisY;
  std::vector<float> rotAxisZ;
  std::vector<float> rotPivotX;
  std::vector<float> rotPivotY;
  std::vector<float> rotPivotZ;

  /** this array contains the information, if a piece is currently moving, or not */
  std::vector<bool> mv;

  /**
   * the number of the last used piece this is NOT identical with
   * pieceNumber of tree because there might be gaps
   */
  unsigned int maxPieceName;

  /** this function walks the tree and sets the piece positions */
  int doRecursive(const separation_c * tree, int step, float * array, unsigned int * orientsOut, bool center_active, int cx, int cy, int cz);

  /** find rotation-arrival metadata for the state at global step index */
  bool findRotationArrival(int step, unsigned int pieceName,
                           int * pvx, int * pvy, int * pvz,
                           unsigned int * axis, unsigned int * sense) const;
  bool findRotationArrivalRec(const separation_c * tree, int step, unsigned int pieceName,
                              int * pvx, int * pvy, int * pvz,
                              unsigned int * axis, unsigned int * sense) const;

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
  virtual unsigned int getTrans(unsigned int piece) override;
  virtual bool getRotationAnim(unsigned int piece,
                               float * angleDeg,
                               float * axisX, float * axisY, float * axisZ,
                               float * pivotX, float * pivotY, float * pivotZ) override;

private:

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

    // no copying and assigning
    fixedPositions_c(const fixedPositions_c&) = delete;
    fixedPositions_c& operator=(const fixedPositions_c&) = delete;

  private:

    unsigned int pieces;
    std::vector<int> x, y, z;
    std::vector<bool> visible;
};

#endif
