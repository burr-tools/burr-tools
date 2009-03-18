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
#ifndef __STL_H__
#define __STL_H__

#include <stdio.h>

class voxel_c;

/** this class gets thrown when something in the export went wrong */
class stlException_c {

  public:

    const char * comment;

    stlException_c(const char * c) : comment(c) {}

};

/**
 * the base class for STL exporters
 *
 * This clas contains the interface that the STL exporter need to implement as well
 * as some helper functions for the export.
 *
 * The STL file handling is done in here, so that the concrete exporters only need
 * to write a long list of triangles
 */
class stlExporter_c {

  public:

    /** create new exporter, defaults to binary mode active */
    stlExporter_c(void) : f(0), binaryMode(true) {}
    virtual ~stlExporter_c(void) {
      if (f)
        close();
    }

    /**
     * This function exports one shape.
     *
     * This is the function that needs to be implemented by the exporter
     * to actually do anything. Basename may be used to create a proper file
     * name.
     *
     * The function has to call open, outTriangle and close to do its work.
     */
    virtual void write(const char * basename, voxel_c * shape) = 0;

    /* some functions to set some parameters for the output all parameters mus tbe double
     * values
     */

    /** return the number of parameters for the concrete exporter */
    virtual unsigned int numParameters(void) const = 0;
    /** return a text to display to the user about the parameter x */
    virtual const char * getParameterName(unsigned int idx) const = 0;
    /** get the value of a parameter */
    virtual double getParameter(unsigned int idx) const = 0;
    /** set the value of a parameter */
    virtual void setParameter(unsigned int idx, double value) = 0;

    /** select whether to use binary mode or not */
    void setBinaryMode(bool on) { binaryMode = on; }
    /** find out if binary mode is active */
    bool getBinaryMode(void) { return binaryMode; }

  protected:

    /**
     * open and overwrites the given file with
     * a new empty one and adds the title, the title may be
     * limited to 80 characters
     *
     * write needs to call this functio before writing the first triangle
     */
    void open(const char * fname);

    /**
     * closes the STL file.
     *
     * The functions writes the needed counters into the binary STL file and closes the
     * file. For text STL files nothing needs to be done
     *
     * write needs to call this function after it finishes with its work
     */
    void close(void);

    /** this function add a triangle to the STL file
     */
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

    FILE * f;                     ///< the file to write to
    unsigned long triangleCount;  ///< number of triangles in the file (for binary files)

    bool binaryMode;              ///< binary STL export active or not
};

#endif
