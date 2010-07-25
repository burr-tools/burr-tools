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
#ifndef __STL_2_H__
#define __STL_2_H__

#include "stl.h"
#include "types.h"

/** stl exporter for spheres */
class stlExporter_2_c : public stlExporter_c {

  public:

    stlExporter_2_c(void) : sphere_rad(10), offset(0), round(1.0), connection_rad(0.75),
      recursion(2.0), inner_rad(0), hole_diam(0), square_hole(false) {}

    virtual Polyhedron * getMesh(const voxel_c & v, const faceList_c & holes) const;

    /* some functions to set some parameters for the output,
     * all parameters must be double values.
     */

    /* return a text to display to the user about the parameter x */
    virtual unsigned int numParameters(void) const { return 8; }
    virtual const char * getParameterName(unsigned int idx) const;
    virtual double getParameter(unsigned int idx) const;
    virtual void setParameter(unsigned int idx, double value);
    virtual const char * getParameterTooltip(unsigned int idx) const;
    virtual parameterTypes getParameterType(unsigned int idx) const;

  private:

    /* parameters */
    double sphere_rad;
    double offset;
    double round;
    double connection_rad;
    double recursion;
    double inner_rad;
    double hole_diam;
    bool square_hole;

  private:

    // no copying and assigning
    stlExporter_2_c(const stlExporter_2_c&);
    void operator=(const stlExporter_2_c&);
};

#endif
