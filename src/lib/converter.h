#ifndef __CONVERTER_H__
#define __CONVERTER_H__

#include "gridtype.h"

class puzzle_c;

/* returns true, if it is possible to convert this class to the
 * given gridType
 */
bool canConvert(const puzzle_c * p, gridType_c::gridType type);

/* do the conversion, if it can't be done (you should check first)
 * nothing will happen.
 * the shapes inside the puzzle will be converted and then the
 * gridtype within the puzzle will be changed
 */
void doConvert(puzzle_c * p, gridType_c::gridType type);

#endif
