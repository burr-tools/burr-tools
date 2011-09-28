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
#ifndef __STL_H__
#define __STL_H__

#include <vector>

#include <stdio.h>

class voxel_c;
class Polyhedron;

/** this class gets thrown when something in the export went wrong */
class stlException_c {

  public:

    const char * comment;

    stlException_c(const char * c) : comment(c) {}

};

/**
 * the base class for STL exporters
 *
 * This class contains the interface that the STL exporter needs to implement as well
 * as some helper functions for the export.
 *
 * The STL file handling is done in here, so that the concrete exporters only need
 * to write a long list of triangles
 */
class faceList_c;
class stlExporter_c {

  public:

    /** create new exporter, defaults to binary mode active */
    stlExporter_c(void) : binaryMode(true) {}
    virtual ~stlExporter_c(void) {}

    /**
     * This function exports one shape.
     */
    void write(const char * basename, const voxel_c & shape, const faceList_c & holes);

    /** parameters can have different type
     * this enum lists all supported types
     */
    typedef enum
    {
        PAR_TYP_DOUBLE,
        PAR_TYP_POS_DOUBLE,
        PAR_TYP_POS_INTEGER,
        PAR_TYP_SWITCH
    } parameterTypes;

    /* some functions to set some parameters for the output all parameters mus tbe double
     * values
     */

    /** return the number of parameters for the concrete exporter */
    virtual unsigned int numParameters(void) const = 0;
    /** return a text to display to the user about the parameter x */
    virtual const char * getParameterName(unsigned int idx) const = 0;
    /** get the tooltip text for this parameter */
    virtual const char * getParameterTooltip(unsigned int /*idx*/) const { return ""; }
    /** get the parameter type for this parameter */
    virtual parameterTypes getParameterType(unsigned int /*idx*/) const { return PAR_TYP_DOUBLE; }
    /** get the value of a parameter */
    virtual double getParameter(unsigned int idx) const = 0;
    /** set the value of a parameter */
    virtual void setParameter(unsigned int idx, double value) = 0;

    /** select whether to use binary mode or not */
    void setBinaryMode(bool on) { binaryMode = on; }
    /** find out if binary mode is active */
    bool getBinaryMode(void) { return binaryMode; }

    virtual Polyhedron * getMesh(const voxel_c & v, const faceList_c & holes) const = 0;

  private:

    bool binaryMode;              ///< binary STL export active or not

  private:

    // no copying and assigning
    stlExporter_c(const stlExporter_c&);
    void operator=(const stlExporter_c&);
};

#endif
