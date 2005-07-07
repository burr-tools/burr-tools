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


#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include "voxel.h"
#include "assembly.h"

#include <xmlwrapp/node.h>

/* here we have a callback class, meaning a class that you give
 * as parameter to a function and this function finally calls the
 * method inside this class. I decided to use this approach because
 * inheritance doesn't allow to change the actions to be taken quickly
 */

class assembler_cb {

public:

  /* this function gets called once for every assembly
   * found by an assembler. it get's the found aseembly
   * as parameter
   */
  virtual bool assembly(assembly_c * a) = 0;
};


/* as the assembly could be done using different routines we provide an
 * general interface to the assemblers using this abstract base class
 */
class assembler_c {

public:

  typedef enum {
    ERR_NONE,
    ERR_TOO_MANY_UNITS,
    ERR_TOO_FEW_UNITS,
    ERR_CAN_NOT_PLACE,
    ERR_CAN_NOT_RESTORE
  } errState;

  /* initialisation */
  assembler_c(void) {}
  virtual ~assembler_c(void) {}

  /* after the constructor call check this function. It return 0 if everything is
   * ok, or a pointer to a string, that you should display providing a message
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
  virtual unsigned int getReducePiece(void) { return 0; }


  /* start the assembly process */
  virtual void assemble(assembler_cb * callback) {}

  /* a function that returns the finished percentage in the range
   * between 0 and 1. It must be possible to call this function
   * while assemble is running
   */
  virtual float getFinished(void) { return 0; }

  /* stops the assembly process somewhen in the near future. */
  virtual void stop(void) {}

  /* returns true, as soon as the process really has stopped */
  virtual bool stopped(void) const { return false; }

  /* returns a string, that can be saved inside a textfile stat specifies the
   * exact position where the assembly process stopped
   * returns true, if the string was long enough to save the data
   *
   * the function shoule only be called when assemble is not running
   */
  virtual bool getPosition(char * string, unsigned int len) { return true; }

  /* sets the position of the assembly process, so that it continues exacly
   * where it stood, when getPosition was called
   *
   * the function should only be called when assembly is not running it shoule be
   * called before calling assemble
   */
  virtual errState setPosition(const char * string, const char * version) { return ERR_CAN_NOT_RESTORE; }

  /* this function saves the current state of the assembler into an xml node to
   * write it to an file
   * this state must be such that the class can restore this state and continue
   * from there by getting this and the puzzle given to the constructor
   */
  virtual xml::node save(void) const { return xml::node("assembler"); }

};

#endif
