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
#ifndef __STL_0_H__
#define __STL_0_H__

#include "stl.h"

/** stl exporter for cubes */
class stlExporter_0_c : public stlExporter_c {

  public:

    stlExporter_0_c(void) : bevel(0.3), cube_scale(10), shrink(0.01), hole(0), leaveGroovesInside(false), leaveGroovesOutside(false), smoothVoid(false) {}

    virtual Polyhedron * getMesh(const voxel_c & v) const;
    virtual unsigned int numParameters(void) const { return 7; }
    virtual const char * getParameterName(unsigned int idx) const;
    virtual double getParameter(unsigned int idx) const;
    virtual void setParameter(unsigned int idx, double value);
    virtual const char * getParameterTooltip(unsigned int idx) const;
    virtual parameterTypes getParameterType(unsigned int idx) const;

  private:

    double bevel;
    double cube_scale;
    double shrink;
    double hole;
    bool leaveGroovesInside;
    bool leaveGroovesOutside;
    bool smoothVoid;

private:

  // no copying and assigning
  stlExporter_0_c(const stlExporter_0_c&);
  void operator=(const stlExporter_0_c&);

};

#endif
