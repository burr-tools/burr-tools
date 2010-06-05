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
#include "volume.h"

#include "polyhedron.h"
#include "face.h"
#include "halfedge.h"
#include "vector3.h"

// "Till Kranz" <kr...@theorie.physik.uni-goettingen.de> wrote

// You need to know a point per face of the polyhedron.  Let face k have
// area A[k], outer-pointing unit-length normal N[k], and a point P[k][1].
// If you have m faces for 0 <= k < m, then the volume can be computed
// from the Divergence Theorem as
//     V = sum_{k=0}^{m-1} A[k]*Dot(N[k], P[k][1])
//
// How were you able to compute the face areas without knowing the
// ordered lists of vertices per face?  If face k has n[k] vertices
// P[k][0] through P[k][n[k]-1], ordered counterclockwise as you view
// the face from outside the polyhedron, then
//     A[k] = (1/6)*Dot(N[k], sum_{j=0}^{n[k]-1} Cross(P[k][j], P[k][j+1]))
// where the j-indexing is modulo n[k]; that is, P[k][n[k]] is the same point
// as P[k][0].
//
// These formulas also work for non-convex polyhedra.

static float get_area(const Face & face)
{
    Vector3Df sum;

    // well, this is a slightly optimized version of the above idea,
    // it will save 2 of the cross products required, which is a lot
    // in case of triangles and quadrilaterals....
    //
    // The above idea uses the origin as the base point p0 for all of the cross
    // products, I use one point of the polygon, which makes 2 vectors 0 and
    // the crossproduct as well, so we leave those 2 out. As we simply use
    // the first point of the polygon as p0, so the first and last crossproduct are
    // 0 and are dropped
    //
    // finally because we now have a p0 within the plane of the polygon, we
    // don't need the scalarproduct with the normal vector, as now the crossproduct
    // will be parallel to n.
    //
    // So how does it work? Have a look at the wikipedia articles for polygon and crossproduct
    //
    // The basic idea is that the polygon is triangulated to one point and that the area
    // of the triangles is calculated and added up. The crossproduct calculates the are
    // of the parallellogram thus resulting in twice the size required, thus the
    // multiplication with 0.5 at the end
    //
    // The nice thing is that outside triangles, that are not insiede the polygon have
    // a negative area and are thus subtracted from the area of the real polygon.
    Face::const_edge_circulator eit = face.begin();
    Face::const_edge_circulator sentinel = eit;

    Vector3Df p0 = (*eit)->dst()->position();
    eit++;

    Vector3Df v = (*eit)->dst()->position()-p0;
    eit++;

    while (eit != sentinel)
    {
        Vector3Df v2 = (*eit)->dst()->position()-p0;
        sum += v ^ v2;
        v = v2;
        eit++;
    }

    return sum.module() * 0.5;
}


float volume(const Polyhedron & poly)
{
    float volume = 0;

    /* Choose a point, any point as the reference */
    Vector3Df p0 = poly.halfedge(0)->dst()->position();

    // same idea as above, just using pyramids to sum up

    for (Polyhedron::const_face_iterator fit = poly.fBegin(); fit != poly.fEnd(); ++fit)
    {
        Vector3Df p = (*fit)->edge()->dst()->position() - p0;
        Vector3Df n = (*fit)->normal();
        /* Do dot product to get distance from point to plane */
        float height = p * n;
        float area = get_area(**fit);
        volume += area * height;
    }

    return volume / 3;
}
