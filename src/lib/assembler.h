/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <xmlwrapp/node.h>

class voxel_c;
class assembly_c;
class puzzle_c;

/* here we have a callback class, meaning a class that you give
 * as parameter to a function and this function finally calls the
 * method inside this class. I decided to use this approach because
 * inheritance doesn't allow to change the actions to be taken quickly
 */

class assembler_cb {

public:

  /* this function gets called once for every assembly
   * found by an assembler. It gets the found assembly
   * as parameter
   */
  virtual bool assembly(assembly_c * a) = 0;

  virtual ~assembler_cb(void) {}
};

class assemblerFrontend_c {

public:

  virtual bool pieceFits(int x, int y, int z) const = 0;

  virtual ~assemblerFrontend_c(void) {}
};

/* as the assembly could be done using different routines we provide an
 * general interface to the assemblers using this abstract base class
 */
class assembler_c {

  const assemblerFrontend_c * frontend;

protected:

  bool pieceFits(int x, int y, int z) const {
    return frontend->pieceFits(x, y, z);
  }

public:

  typedef enum {
    ERR_NONE,
    ERR_TOO_MANY_UNITS,
    ERR_TOO_FEW_UNITS,
    ERR_CAN_NOT_PLACE,
    ERR_CAN_NOT_RESTORE_VERSION,
    ERR_CAN_NOT_RESTORE_SYNTAX,
    ERR_PIECE_WITH_VARICUBE,
    ERR_PUZZLE_UNHANDABLE        // the puzzle contains definitions that can not be (like ranges, multipieces, ...)
  } errState;

  /* initialisation, only the things that can be done quickly */
  assembler_c(const assemblerFrontend_c * fe) : frontend(fe) {}
  virtual ~assembler_c(void) {
    delete frontend;
  }

  /* the part of the initialisation that may take a while
   * when keep mirror is true, the assembler must not throw away mirror solutions
   * but it still removes solutions that are rotations.
   */
  virtual errState createMatrix(const puzzle_c * /*puz*/, unsigned int /*problemNum*/, bool /*keepMirror*/, bool /*keepRotations*/) { return ERR_NONE; }

  /* after the constructor call check this function. It return 0 if everything is
   * OK, or a pointer to a string, that you should display providing a message
   * why the puzzle is not solvable
   */
  virtual int getErrorsParam(void) { return 0; }

  /* the function tries to remove possible piece placements by checking if, after
   * the piece has been placed somewhere, that all the other pieces still can be
   * placed and all holes can still be filled.
   * if this is not the case then this placement can be removed
   *
   * it is not necessary for an assembler to implement this function
   */
  virtual void reduce(void) { }

  /* because the reduce process can take a long time it is nice to
   * give some feedback to the user. With this function the user can
   * get a number to display with the information that the program is
   * currently reducing. The intended interpretation is that the program
   * is currently working on the piece with the returned number, but if
   * you want you can also return something else
   */
  virtual unsigned int getReducePiece(void) { return 0; }

  /* start the assembly process
   * it is intended that the assembly process runs in a different thread from
   * the controlling thread. When this is the case the controlling thread can
   * call stop to make the assembly thread stop working. It will then return
   * from this function but can be resumed any time
   */
  virtual void assemble(assembler_cb * /*callback*/) {}

  /* this function returns a number reflecting the complexity of the
   * puzzle. This could be the number of placements tried, or
   * some other value
   */
  virtual unsigned long getIterations(void) { return 0; }

  /* a function that returns the finished percentage in the range
   * between 0 and 1. It must be possible to call this function
   * while assemble is running
   */
  virtual float getFinished(void) { return 0; }

  /* stops the assembly process sometimes in the near future. */
  virtual void stop(void) {}

  /* returns true, as soon as the process really has stopped */
  virtual bool stopped(void) const { return false; }

  /* sets the position of the assembly process, so that it continues exactly
   * where it stood, when getPosition was called
   *
   * the function should only be called when assembly is not running it should be
   * called before calling assemble
   */
  virtual errState setPosition(const char * /*string*/, const char * /*version*/) { return ERR_CAN_NOT_RESTORE_VERSION; }

  /* this function saves the current state of the assembler into an xml node to
   * write it to an file
   * this state must be such that the class can restore this state and continue
   * from there by getting this and the puzzle given to the constructor
   */
  virtual xml::node save(void) const { return xml::node("assembler"); }

  /* some more special information to find out possible piece placements */
  /* it is optional to support this interface. The user alsway has to first check
   * if the Supported function returns true, if not the functionality is not
   * supported
   */
  virtual bool getPiecePlacementSupported(void) { return false; }

  /* return the number of placements for a given _PIECE_ not the shape */
  virtual unsigned int getPiecePlacementCount(unsigned int /*piece*/) { return 0; }

  /* returns the placement of a piece (orientation, and position). Node gives a current position
   * inside some internal data structure, the new position is returned as return value, for initialisation
   * give 0 as node. Delta is the number of placements you want to go back of forward. So
   * node = getPiecePlacement(0, 10, ...) returns the 11-th placement of the given piece (1st plus 10 forward)
   */
  virtual unsigned int getPiecePlacement(unsigned int /*node*/, int /*delta*/,
      unsigned int /*piece*/, unsigned char * /*ran*/, int * /*x*/, int * /*y*/, int * /*z*/) { return 0; }


  /* finally some debugging functions that allow to look how, why, and where pieces are placed */

  /* do exactly the given number of rounds in the assembler, and then stop */
  virtual void debug_step(unsigned long /*num*/) {};

  /* returns the assembly for the current state of the assembler or the solution assembly, if
   * the assembler is currently at a solution
   */
  virtual assembly_c * getAssembly(void) = 0;

};

#endif
