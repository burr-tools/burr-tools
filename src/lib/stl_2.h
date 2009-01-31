/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

/* stl exporter for spheres */
class stlExporter_2_c : public stlExporter_c {

  public:

    stlExporter_2_c(void) : sphere_rad(10), offset(0), round(1), connection_rad(0.75), recursion(2) {}

    virtual void write(const char * basename, voxel_c * shape);

    /* some functions to set some parameters for the output all parameters mus tbe double
     * values
     */

    /* return a text to display to the user about the parameter x */
    virtual unsigned int numParameters(void) const { return 5; }
    virtual const char * getParameterName(unsigned int idx) const;
    virtual double getParameter(unsigned int idx) const;
    virtual void setParameter(unsigned int idx, double value);

  private:

    /* parameters */
    double sphere_rad;
    double offset;
    double round;
    double connection_rad;
    double recursion;

    /* internal variables */
    double curvRad, curvX, curvY;

    void drawTriangle(
        int x, int y, int z,
        double x1, double y1, double z1,
        double x2, double y2, double z2,
        double x3, double y3, double z3,
        int edge12, int edge23, int edge31, int rec);

    void makeSphere(int x, int y, int z, uint16_t neighbors);

    void drawHolePiece(int i, int x, int y, int z,
        double start, double end,
        double x1, double y1, double z1,
        double x2, double y2, double z2,
        int rec);

    void drawHole(
        int x, int y, int z, int i,
        double x1, double y1, double z1,
        double x2, double y2, double z2, int rec, int rec2);
};

#endif

