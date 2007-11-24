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
#ifndef __STL_H__
#define __STL_H__

#include <stdio.h>

class voxel_c;

/* this class gets thrown when something in the export went wrong */
class stlException_c {

  public:

    const char * comment;

    stlException_c(const char * c) : comment(c) {}

};

class stlExporter_c {

  public:

    stlExporter_c(void) : f(0), binaryMode(true) {}
    virtual ~stlExporter_c(void) {
      if (f)
        close();
    }

    virtual void write(const char * basename, voxel_c * shape) = 0;

    /* some functions to set some parameters for the output all parameters mus tbe double
     * values
     */

    /* return a text to display to the user about the parameter x */
    virtual unsigned int numParameters(void) const = 0;
    virtual const char * getParameterName(unsigned int idx) const = 0;
    virtual double getParameter(unsigned int idx) const = 0;
    virtual void setParameter(unsigned int idx, double value) = 0;

    /* select whether to use binary mode or not */
    void setBinaryMode(bool on) { binaryMode = on; }
    bool getBinaryMode(void) { return binaryMode; }

  protected:

    /* open and overwrites the given file with
     * a new empty one and adds the title, the title may be
     * limited to 80 characters
     */
    void open(const char * fname);

    /* closes the STL file */
    void close(void);

    /* some helper functions for STL output */
    void outTriangle(
        /* the 3 vertexes of the triangle */
        double x1, double y1, double z1,
        double x2, double y2, double z2,
        double x3, double y3, double z3,
        /* a point that is on the _INSIDE_ of the plane of thr triangle
         * this is used to calculate the proper normal and reorientate the
         * points anti clockwise
         */
        double xp, double yp, double zp);

  private:

    FILE * f;
    unsigned long triangleCount;

    bool binaryMode;
};

#endif
