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
#ifndef __STL_0_H__
#define __STL_0_H__

#include "stl.h"

class stlExporter_0_c : public stlExporter_c {

  public:

    stlExporter_0_c(void) : bevel(1), cube_scale(10), shrink(0.1) {}

    virtual void write(const char * basename, voxel_c * shape);

    /* some functions to set some parameters for the output all parameters mus tbe double
     * values
     */

    /* return a text to display to the user about the parameter x */
    virtual unsigned int numParameters(void) const { return 3; }
    virtual const char * getParameterName(unsigned int idx) const;
    virtual double getParameter(unsigned int idx) const;
    virtual void setParameter(unsigned int idx, double value);

  private:

    double bevel;
    double cube_scale;
    double shrink;

    void rotate_point(float *x, float *y, float *z, int rot);

    void make_tri(float x0, float y0, float z0,
        float x1, float y1, float z1,
        float x2, float y2, float z2,
        int rot, int x, int y, int z);

    void make_quad(float x0, float y0, float z0,
        float x1, float y1, float z1,
        float x2, float y2, float z2,
        float x3, float y3, float z3,
        int rot, int x, int y, int z);

    void make_corners(const voxel_c *v, const int x, const int y, const int z);
    void make_edges(const voxel_c *v, const int x, const int y, const int z);
    void make_faces(const voxel_c *v, const int x,const int y,const int z);

};

#endif

