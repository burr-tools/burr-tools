/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/maths/mpr.h at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G1). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: none.
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
// this module contains functionality to check whether 2 convex polyhedra or polygons
// intersect. It can also be used to check whether a point is inside a
// polyhedron/polygon. It is heavily based on libccd, but C++-ified. The original is C
// code and due to using inlines and abstraction hard to read and probably also
// relatively slow and unnecessarily complex
//
// The code in here is a simplified version of the algorithm, removing
// everything that we don't need for BurrTools. The removed parts are the bail
// out when the portal gets close to origin. This is only needed for smooth
// surfaces like spheres, but not for polyhedra.
//
// For documentation regarding this algorithm:
//
// https://www.youtube.com/watch?v=ajv46BSqcK4
// contains a very good description of the basic idea of a different but similar
// algorithm
//
// Game Programming Gems 6
// Chapter 2.5 XenoCollide: Complex Collisions Made Simple by Gary Snethen from
// Crystal Dynamics describes this very algorithm in more detail
#pragma once

#include "algebra.h"

#include <array>
#include <limits>
#include <type_traits>

namespace BurrTools::Maths::MPR {

namespace Internal {

// the functions for the 2d and 3d version differ slightly and thus some of
// them exist twice when functionality is only for the 2d or 3d case this
// is shown by 2d and 3d suffixes, if no suffix is present the functions are used
// for 2d and 3d

// encapsulate a simplex (in this case a tetrahedron) used as the portal for
// the portal refinement.
// Index zero always contains the centre of the Minkowsky difference. Indices
// 1, 2 (and 3) contain the current portal face/line. The points need to be ordered
// counter clock wise when looked from the outside (so that the normal
// calculation works)
template <class T>
using Simplex3d = std::array<BurrTools::Maths::Vector<T>, 4>;

template <class T>
using Simplex2d = std::array<BurrTools::Maths::Vector2d<T>, 3>;

// this is used to make some of the maths below more understandable. I fully
// expect the compiler to be smart enough to optimize away all unnecessary
// mathematical operations using this vector
template <class T>
static constexpr BurrTools::Maths::Vector<T> origin(0, 0, 0);

template <class T>
static constexpr BurrTools::Maths::Vector2d<T> origin2d(0, 0);

// calculate the support function for the Minkowski difference
auto support(const auto & dir, const auto & c) { return c.support1(dir) - c.support2(-dir); }

// calculate the centre for the Minkowsky difference
auto findOrigin(const auto & c) { return c.center1() - c.center2(); }

// Expand the portal using v4 and dropping one of the existing portal points.
// v4 will be inside the portal (just as the origin). The point to replace is
// chosen by checking in which part of the current portal the origin is situated
template <class T>
void expandPortal(Simplex3d<T> & portal, const BurrTools::Maths::Vector<T> & v4)
{
    // imagine the plane of the portal. On that plane the portal is a triangle
    // with the corners v1 v2 v3. The portal centre is the vertex v0 somewhere
    // below that plane.
    // The origin is a point above that plane somewhere within the (possibly
    // skewed) "extension" of the triangular pyramid with the base triangle v1
    // v2 v3 and peak v0. "Extension" means extended beyond the base.
    // v4 is also within that volume. We also know that if we translate the
    // base plane such that it is intersecting with v4 then the origin would be
    // inside the actual pyramid.
    // v4 can be used to cut up the base triangle into 3 smaller triangles
    // v1 v2 v4, v2 v3 v4 and v3 v1 v4. We need to find out within which of these
    // 3 "extended" portals (with theses 3 triangles as base) the origin falls.
    // That triangle will be used for the new portal.
    //
    // this is achieved by calculating this cross product
    auto v4v0 = cross(v4 - origin<T>, portal[0] - origin<T>);
    // this vector will always point clockwise and can be used to describe a
    // plane through the origin, v4 and v0.
    //
    // This plane can now be used in the following manner. Imagine the origin
    // is in the third described by the triangle v3 v1 v4. Then v3 will always
    // be above the plane and v1 always below.
    //
    // invert that logic, when v1 is below the plane then the origin must be
    // either in v3 v1 v4 or v2 v3 v4. In which one of these 2 can be checked
    // by looking if v3 is above or below the plane
    //
    // this is exactly what the following logic does... for all cases
    if (portal[1] * v4v0 > 0)
        if (portal[2] * v4v0 > 0)
            portal[1] = v4;
        else
            portal[3] = v4;
    else if (portal[3] * v4v0 > 0)
        portal[2] = v4;
    else
        portal[1] = v4;
}

template <class T>
void expandPortal2d(Simplex2d<T> & portal, const BurrTools::Maths::Vector2d<T> & v4)
{
    // the 2d case is much simpler
    //
    // we know the origin is between portal points 1 and 2
    // the new v4 is somewhere in the middle
    // we need to find out whether the origin is on the side of portal 1 or 2
    // relative to v4
    auto v4v0 = cross(v4 - portal[0], -portal[0]);
    auto v1v0 = cross(v4 - portal[0], portal[1] - portal[0]);

    // the cross product will be positive or negative depending on whether the origin or portal[1] point are
    // clockwise or counterclockwise oriented relative to the portal[0] to origin vector
    // when both are on the same side, then the product will be positive
    if (v4v0 * v1v0 > 0)
        // the origin is on the same side of v4 as v1, so
        // use v4 instead of v2
        portal[2] = v4;
    else
        portal[1] = v4;
}

// discovers an initial portal
//
// Returns -1 if already recognized that origin is outside of Minkowsky difference.
// Returns  1 if origin is known to be inside
// Returns  0 if portal was built.
auto discoverPortal(const auto & c)
{
    using T = std::remove_reference_t<decltype(c)>::CoordinateType;

    // return values
    Simplex3d<T> portal;
    int retval = 0;

    // we first try to create an initial portal. This portal doesn't need to
    // "contain" the origin but we try to at least get something in that
    // direction

    // vertex 0 is centre of portal, this doesn't change any more for the rest
    // of the calculations
    portal[0] = findOrigin(c);

    // test if portal's centre lies on origin (0,0,0) this is unlikely but we
    // can and must immediately bail out because later computations would fail
    if (length<float>(portal[0]) < 0.0001)
    {
        retval = 1;
    }
    else
    {
        // vertex 1 = support in direction of origin
        auto dir  = normalize(-portal[0]);
        portal[1] = support(dir, c);

        // when the origin is above the plane formed by that point and the
        // direction as normal it simply can not be part of the difference, or we
        // would have found a support further out, so we can bail out and exit
        if (pointPlaneDistance(origin<T>, planeFromNormalAndPoint(dir, portal[1])) > -std::numeric_limits<T>::epsilon())
        {
            retval = -1;
        }
        else
        {
            // vertex 2
            dir = cross(portal[1] - origin<T>, portal[0] - origin<T>);

            // a zero length cross product means that v0, origin, v1 are on a line and
            // because of the above check we know that the origin is between v0 and v1
            // we can conclude that the origin is inside the difference
            if (dir * dir < std::numeric_limits<T>::epsilon())
            {
                retval = 1;
            }
            else
            {
                dir = normalize(dir);

                portal[2] = support(dir, c);

                // the new corner can not be below the plane because we already know that v0
                // and v1 are part of the difference so the support point must be above the
                // plane
                assert(portal[2] * dir >= 0);

                while (true)
                {
                    // vertex 3 direction, this direction will always point towards
                    // the origin, the origin will be above this plane. This is because
                    // how the first 2 points have been constructed
                    dir             = normalize(cross(portal[1] - portal[0], portal[2] - portal[0]));
                    auto newSupport = support(dir, c);
                    assert(pointPlaneDistance(origin<T>, planeFromNormalAndPoint(dir, portal[1])) >= -std::numeric_limits<T>::epsilon());

                    // similar argument to above, when the origin is above the plane it can
                    // not be part of the difference
                    if (pointPlaneDistance(origin<T>, planeFromNormalAndPoint(dir, newSupport)) > std::numeric_limits<T>::epsilon())
                    {
                        retval = -1;
                        break;
                    }

                    // now check where the origin is in relation to the triangle created by
                    // v1 v2 newSupport, we already know that the origin is above the line
                    // of v1 v2. We also know that it is below the parallel of that line
                    // but through newSupport. So it can essentially be in 3 areas
                    if (cross(portal[1], newSupport) * portal[0] < -std::numeric_limits<T>::epsilon())
                    {
                        // origin is outside (v1, v0, newSupport) - set v2 as newSupport and continue
                        portal[2] = newSupport;
                    }
                    else if (cross(newSupport, portal[2]) * portal[0] < -std::numeric_limits<T>::epsilon())
                    {
                        // origin is outside (newSupport, v0, v2) - set v1 as newSupport and continue
                        portal[1] = newSupport;
                    }
                    else
                    {
                        // origin is inside the triangle, use newSupport and leave here
                        portal[3] = newSupport;

                        // TODO are we sure here that the portal faces outside, e.g.
                        // towards the origin?  just put in an assert to make sure
                        assert(cross(portal[2] - portal[1], portal[3] - portal[1]) * (portal[1] - portal[0]) > 0);

                        break;
                    }
                }
            }
        }
    }

    return std::pair(retval, portal);
}

auto discoverPortal2d(const auto & c)
{
    using T = std::remove_reference_t<decltype(c)>::CoordinateType;

    // return values
    Simplex2d<T> portal;
    int retval = 0;

    // again the 2d version is much simpler

    // vertex 0 is centre of portal, this doesn't change any more for the rest
    // of the calculations
    portal[0] = findOrigin(c);

    // test if portal's centre lies on origin (0,0) this is unlikely but we
    // can and must immediately bail out because later computations would fail
    if (length<float>(portal[0]) < 0.0001)
    {
        retval = 1;
    }
    else
    {
        // vertex 1 = support in direction of origin
        auto dir  = normalize(-portal[0]);
        portal[1] = support(dir, c);

        // when the angle between the vectors from the center to the origin and the vector from the origin to the new support point is more than 90° (the dot
        // product it negative) then we know that the origin is begin the support found and thus can not be part of the Minkowsky difference
        if ((portal[1] * dir) < std::numeric_limits<T>::epsilon())
        {
            retval = -1;
        }
        else
        {
            // direction for vertex 2 of the portal is orthogonal to the direction from Minkowsky center to the frst portal point
            // and pointing in the direction of the origin. This can be achieved using the triple cross product
            dir = triplecross(portal[0] - portal[1], -portal[1]);

            // a zero length cross product means that v0, origin, v1 are on a line and
            // because of the above check we know that the origin is between v0 and v1
            // we can conclude that the origin is inside the difference
            if (dir * dir < std::numeric_limits<T>::epsilon()) return std::pair(1, portal);

            dir = normalize(dir);

            portal[2] = support(dir, c);
        }
    }

    return std::pair(retval, portal);
}

// Expands portal towards origin and determine if objects intersect.
// Already established portal must be given as argument.
// If intersection is found true is returned, false otherwise
bool refinePortal(const auto & c, auto & portal)
{
    using T = std::remove_reference_t<decltype(c)>::CoordinateType;

    while (true)
    {
        // compute direction outside the portal (from v0 through v1,v2,v3
        // face) this requires v1, v2, v3 to be counter clockwise when viewed
        // from the outside otherwise dir will point inside
        // TODO this must be guaranteed
        auto dir = normalize(cross(portal[2] - portal[1], portal[3] - portal[1]));

        // test if origin is inside the portal.
        // when the origin is on or below the plane of the portal, we know that it is inside
        if (pointPlaneDistance(origin<T>, planeFromNormalAndPoint(dir, portal[1])) < std::numeric_limits<T>::epsilon()) return true;

        // get next support point
        auto v4 = support(dir, c);

        // test if v4 can expand portal to contain origin.
        // when the origin is above the plane of the new support plane parallel
        // to the portal we have no chance that it might be inside and can quit
        //
        // ATTENTION, we are using polyhedra here and relatively simple ones at
        // that. That means that we are going to reach an optimal portal after
        // a few iterations. This is not the case if you have for example
        // spheres. There you can improve the portal ad infinitum getting
        // slightly closer to the origin every time, but never reaching it. In
        // those cases you need a boundary condition when you stop. But that
        // check is not needed here in BurrTools
        if (pointPlaneDistance(origin<T>, planeFromNormalAndPoint(dir, v4)) > std::numeric_limits<T>::epsilon()) return false;

        // v1-v2-v3 triangle must be rearranged to face outside Minkowsky
        // difference (direction from v0).
        expandPortal(portal, v4);
    }
}

bool refinePortal2d(const auto & c, auto & portal)
{
    using T = std::remove_reference_t<decltype(c)>::CoordinateType;

    while (true)
    {
        // compute direction outside the portal (from v0 throught v1,v2,v3
        // face) this requires v1, v2, v3 to be counter clockwise when viewed
        // from the outside otherwise dir will point inside
        auto dir = normalize(triplecross(portal[2] - portal[1], portal[1]));

        // test if origin is inside the portal.
        // when the origin is on or below the plane of the portal, we know that it is inside
        if (pointLine2dDistance(origin2d<T>, lineFromNormalAndPoint(dir, portal[1])) < std::numeric_limits<T>::epsilon()) return true;

        // get next support point
        auto v4 = support(dir, c);

        if (pointLine2dDistance(origin2d<T>, lineFromNormalAndPoint(dir, v4)) > std::numeric_limits<T>::epsilon()) return false;

        expandPortal2d(portal, v4);
    }
}

} // namespace Internal

// check if 2 polyhedra defined by c are intersecting
[[nodiscard]] bool intersects3d(const auto & c)
{
    // Phase 1: Portal discovery - find portal that intersects with origin
    // ray (ray from centre of Minkowsky diff to origin of coordinates)
    if (auto [res, portal] = Internal::discoverPortal(c); res)
        // the discovery process already found of if the origin is in our outside
        return res > 0;
    else
        // otherwise we do Portal refinement
        return Internal::refinePortal(c, portal);
}

// same for 2d
[[nodiscard]] bool intersects2d(const auto & c)
{
    if (auto [res, portal] = Internal::discoverPortal2d(c); res)
        return res > 0;
    else
        return Internal::refinePortal2d(c, portal);
}

} // namespace BurrTools::Maths::MPR
