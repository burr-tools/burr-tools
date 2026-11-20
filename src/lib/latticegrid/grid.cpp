/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/lib/grid.cpp at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G2). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: include paths; the uses of Enumerate and IterateIndexExcept (rangefor.h) are index loops; intdiv_inf comes from algebra.h; 0zu literals (C++23) are size_t{0}; std::print replaced by fprintf, exit by abort. std::format stays (C++20).
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#include "grid.h"

#include "algebra.h"
#include "symmetry.h"
#include "transformation.h"

#include "mpr.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <limits>
#include <tuple>
#include <utility>

using BurrTools::Maths::Matrix;
using BurrTools::Maths::Plane;
using BurrTools::Maths::Quaternion;
using BurrTools::Maths::RMatrix;
using BurrTools::Maths::RVector;
using BurrTools::Maths::Vector;

namespace BurrTools::PuzzleLibrary {

using BurrTools::Maths::intdiv_inf;

static std::vector<std::weak_ptr<const Grid::Information>> deadInformation;

Grid::Polyhedron prism(const std::vector<BurrTools::Maths::Vector<float>> & face, const BurrTools::Maths::Vector<float> & normal)
{
    const size_t fs = face.size();
    // vertices
    std::vector<BurrTools::Maths::Vector<float>> v(2 * fs);
    for (auto j = size_t{0}; j < fs; j++)
    {
        v[j]      = face[j];
        v[j + fs] = face[j] + normal;
    }

    // indices
    std::vector<std::vector<size_t>> i(2 + fs);

    // top and bottom face
    i[0].resize(fs);
    i[1].resize(fs);

    for (auto j = size_t{0}; j < fs; j++)
    {
        // top and bottom face are made vertex by vertex
        i[0][j] = j;
        i[1][j] = 2 * fs - 1 - j;

        // side faces one face at a time, they are always quads
        i[2 + j].resize(4);
        i[2 + j][0] = j;
        i[2 + j][1] = fs + j;
        i[2 + j][2] = fs + ((j + 1) % fs);
        i[2 + j][3] = (j + 1) % fs;
    }

    return Grid::Polyhedron{v, i};
}

std::vector<std::shared_ptr<Grid::ExpensiveInformation>> Grid::information_;
std::mutex Grid::informationMutex_; // mutex for information

// check if 2 floats are reasonably close to one another (for our purpose here)
static bool closeto(const float a, const float b) { return std::fabs(a - b) < 0.00001; }

// check if 2 polygons given in a and b are opposite faces, this is done by checking if
// you can rotate the points in such a way that they all line up but are in opposite
// winding order
static bool oppositeFace(const std::vector<Vector<float>> & a, const std::vector<Vector<float>> & b)
{
    // if the number of corners in the face is not identical... the faces can't be opposite
    if (a.size() != b.size()) return false;

    const size_t s = b.size();

    // find the coordinate in b that is identical to the last point in a
    size_t j = s;

    for (size_t i = 0; i < b.size(); i++)
        if (equal(a[s - 1], b[i], 0.00001f))
        {
            j = i;
            break;
        }

    // if no such point exists ... no opposites
    if (j == b.size()) return false;

    // now going backwards on a and forwards in b all points must match
    size_t i = s - 2;
    j += 1;

    while (true)
    {
        if (j >= s) j -= s;

        if (!equal(a[i], b[j], 0.00001f)) return false;

        if (i == 0) break;

        i--;
        j++;
    }

    return true;
}

// check if 2 edges are the same edge, return true if that is the case
// also return in d_out the distance between the 2 edges in voxels
// and in direction if the 2 edges are aligned the same way (1) or not (-1)
//
// the same in this context means: same vector between the 2 ends and the relative position
// of the 2 edges is an integer distance in voxels from one another
static std::tuple<bool, Vector<int>, int> sameEdge(const Grid::Edge & e1, const Grid::Edge & e2, const Matrix<float> & inv)
{
    // get the end points of the 2 edges
    const auto [e1a, e1b] = e1;
    const auto [e2a, e2b] = e2;

    // calculate the vectors of the 2 edges
    const Vector<float> v1 = e1a - e1b;
    const Vector<float> v2 = e2a - e2b;

    // check if either the vectors are similar, or one is the inverse of the other
    const bool c1 = equal(v1, v2, 0.00001f);
    const bool c2 = equal(v1, -v2, 0.00001f);

    // if neither is the case, they can't be the same edge
    if (!c1 && !c2) return {false, {}, {}};

    if (c1 && c2)
    {
        fprintf(stderr, "oops seems like there is a bug in sameEdge function\n");
        std::abort();
    }

    // calculate the distance between the 2 edges
    Vector<float> distance;
    int direction;

    // if both edges point in the same direction we calculate the
    // distance between the edges by subtracting the start points
    // if they are inverse, we subtract start point of the fist from
    // the endpoint of the other

    if (c1)
    {
        distance  = e1a - e2a;
        direction = 1;
    }
    else
    {
        distance  = e1a - e2b;
        direction = -1;
    }

    // now check if that distance has an integer solution for the physical
    // translations matrix, if so then the 2 edges are the same
    const Vector<float> i = inv * distance;

    const Vector<int> d_out = Vector<float>(std::round(i.x()), std::round(i.y()), std::round(i.z()));

    return {equal(i, d_out, 0.00001f), d_out, direction};
}

// create a plane for the face f of polyhedron o
// this takes only the first 3 points and assumes the remainder
// are co-planar
static std::optional<Plane<float>> calcPlane(const Grid::Polyhedron & o, const size_t f)
{
    Plane<float> p;

    const Vector<float> v1 = o.vertices[o.faces[f][0]];
    const Vector<float> v2 = o.vertices[o.faces[f][1]];
    const Vector<float> v3 = o.vertices[o.faces[f][2]];

    const Vector<float> a = v3 - v2;
    const Vector<float> b = v1 - v2;

    p.normal = cross(a, b);

    const float l = p.normal * p.normal;

    if (l < 0.0001) return {};

    p.normal /= std::sqrt(l);
    p.distance = p.normal * v1;

    return p;
}

// find the intersection point of 3 planes
// there might not be one, that is why the optional return type
// TODO this probably should go into the algebra header
static std::optional<Vector<float>> planeIntersect(const Plane<float> & p1, const Plane<float> & p2, const Plane<float> & p3)
{
    // solve equation system
    //  p1n x (x, y, z) = p1d
    //  p2n x (x, y, z) = p2d
    //  p3n x (x, y, z) = p3d

    const Matrix<float> m = rowMatrix(p1.normal, p2.normal, p3.normal);

    const float det = m.determinant();

    if (closeto(det, 0)) return {};

    const Matrix<float> mi = invert(m, det);

    const Vector d(p1.distance, p2.distance, p3.distance);

    return mi * d;
}

// take the physical model and calculate the bevel offset and normal vectors for each vertex of each face
// return an error string on failure, or empty string if successful
std::string calculateBevelOffsetVertices(const std::vector<Grid::Polyhedron> & physicalShapes,
                                         std::vector<Grid::MeshVertex> & meshVertices,
                                         std::vector<std::vector<uint16_t>> & meshFaceStartIndex)
{
    meshFaceStartIndex.resize(physicalShapes.size());

    // for each base position
    for (size_t base = 0; base < physicalShapes.size(); base++)
    {
        meshFaceStartIndex[base].resize(physicalShapes[base].faces.size());
        const auto & p = physicalShapes[base];

        // for each face
        for (size_t face = 0; face < p.faces.size(); face++)
        {
            meshFaceStartIndex[base][face] = meshVertices.size();

            Grid::MeshVertex vert;

            // calculate normal vector, which is identical for all vertices
            // of this face
            {
                Vector<float> a = p.vertices[p.faces[face][2]] - p.vertices[p.faces[face][1]];
                Vector<float> b = p.vertices[p.faces[face][0]] - p.vertices[p.faces[face][1]];

                vert.normal = cross(a, b);

                const float l = vert.normal * vert.normal;

                if (l < 0.0001)
                {
                    return "oops, normal vector is quite short";
                }

                vert.normal /= std::sqrt(l);
            }

            // for each vertex in the face
            for (size_t cornerindex = 0; cornerindex < p.faces[face].size(); cornerindex++)
            {
                const auto pfv = p.faces[face][cornerindex];
                auto next_cornerindex = (cornerindex + 1) % p.faces[face].size();
                auto prev_cornerindex = (cornerindex + p.faces[face].size() - 1) % p.faces[face].size();

                auto next_corner = p.faces[face][next_cornerindex];
                auto prev_corner = p.faces[face][prev_cornerindex];

                // find all faces that have a corner at the current vertex
                // the current face is always first
                std::array<size_t, 3> vfaces;
                vfaces[0] = face;

                vfaces[1] = vfaces[2] = p.faces.size();

                // find all other faces of this corner
                for (size_t f = 0; f < p.faces.size(); f++)
                {
                    if (f == face) continue;
                    bool contains_curr_index = false;
                    bool contains_prev_index = false;
                    bool contains_next_index = false;

                    for (auto & pf2v : p.faces[f])
                    {
                        if (pfv == pf2v) contains_curr_index = true;
                        if (next_corner == pf2v) contains_next_index = true;
                        if (prev_corner == pf2v) contains_prev_index = true;
                    }

                    if (contains_curr_index && contains_prev_index)
                    {
                        if (vfaces[1] < p.faces.size()) return std::format("oops found previous face twice, aborting");

                        vfaces[1] = f;
                    }

                    if (contains_curr_index && contains_next_index)
                    {
                        if (vfaces[2] < p.faces.size()) return std::format("oops found next face twice, aborting");

                        vfaces[2] = f;
                    }

                    if (contains_next_index && contains_prev_index && contains_curr_index)
                        return std::format("oops something is wrong with the detection of next and previous faces, aborting");
                }

                if (vfaces[1] >= p.faces.size() || vfaces[2] >= p.faces.size()) return std::format("oops have not found next or previous face, aborting");

                // calculate planes for the 3 faces
                Plane<float> planes[3];

                {
                    const auto p0 = calcPlane(p, vfaces[0]);
                    const auto p1 = calcPlane(p, vfaces[1]);
                    const auto p2 = calcPlane(p, vfaces[2]);

                    if (!p0 || !p1 || !p2) return "plane calculation failed";

                    planes[0] = *p0;
                    planes[1] = *p1;
                    planes[2] = *p2;
                }

                float angle1 = acos(std::clamp(planes[0].normal * planes[1].normal, -1.0f, 1.0f));
                float angle2 = acos(std::clamp(planes[0].normal * planes[2].normal, -1.0f, 1.0f));

                // recalculate corner by intersecting the 3 planes
                auto pi = planeIntersect(planes[0], planes[1], planes[2]);
                if (!pi) return "plane intersection failed";
                vert.position = *pi;

                // the result should be reasonable close to the original vertex
                if (!equal(vert.position, p.vertices[pfv], 0.00001f))
                {
                    return std::format("oops calculated corner not close to original ({0} {1} {2}) <-> ({3} {4} {5})",
                                       vert.position.x(),
                                       vert.position.y(),
                                       vert.position.z(),
                                       p.vertices[pfv].x(),
                                       p.vertices[pfv].y(),
                                       p.vertices[pfv].z());
                }

                // to get bevel and offset vectors we move the planes a bit and find a new corner
                // for the offset that means moving all planes, for the bevel we leave out
                // plane 0 (the one of the current face

                // first just move the plane to the previous vertex in a bit, this will
                // give us the bevel vector fot that case
                planes[1].distance -= 0.1;
                pi = planeIntersect(planes[0], planes[1], planes[2]);
                if (!pi) return "plane intersection failed";
                vert.bevel_prev = *pi;

                // undo that change and just move in the plane for the next face
                // this will give us the vector for the next edge bevel
                planes[1].distance += 0.1;
                planes[2].distance -= 0.1;
                pi = planeIntersect(planes[0], planes[1], planes[2]);
                if (!pi) return "plane intersection failed";
                vert.bevel_next = *pi;

                // finally move in all 3 planes, this will give us an offset vector
                planes[1].distance -= 0.1;
                planes[0].distance -= 0.1;
                pi = planeIntersect(planes[0], planes[1], planes[2]);
                if (!pi) return "plane intersection failed";
                vert.offset = *pi;

                // we stored the positions of the bevel and offset corners, to get the vector we simply
                // subtract the position and multiply by 10 because we only moved 1/10th unit
                vert.offset     = (vert.offset - vert.position) * 10;
                vert.bevel_next = (vert.bevel_next - vert.position) * (10 * sin(angle2 / 2));
                vert.bevel_prev = (vert.bevel_prev - vert.position) * (10 * sin(angle1 / 2));

                meshVertices.push_back(vert);
            }
        }
    }

    return {};
}

// make that all the base positions are within the parallel epiped set up by the
// translation symmetry matrix
static void normalizeBasePositions(const RMatrix<int> & g, const Matrix<int> & f, const Matrix<float> & p, Vector<int> & b, Grid::Polyhedron & s)
{
    // we get the normalized position by doing the following:
    // - find out which coefficients we need to each of the translation symmetry vectors
    //   to get the position (done by putting the vectors
    const RVector b3 = g * b;
    const Vector<int> n{intdiv_inf(b3.x(), b3.denominator()), intdiv_inf(b3.y(), b3.denominator()), intdiv_inf(b3.z(), b3.denominator())};

    b -= f * n;

    // also shift the physical representation around accordingly
    const Vector<float> d = p * n;

    // shift all the vertices in the corresponding physical shape
    // according to the physical translation
    for (auto & v : s.vertices) v -= d;
}

// find the voxels that are on the other side of the face of a voxel
static std::string calculateFaceNeighbourhood(const std::vector<Grid::Polyhedron> & ps,
                                              const Matrix<float> & pht,
                                              const Matrix<int> & tra,
                                              const std::vector<Vector<int>> & bp,
                                              Grid::StorageId grid,
                                              std::vector<std::vector<Grid::FaceNeighbour>> & faceNeighbours)
{
    faceNeighbours.resize(ps.size());

    for (size_t base = 0; base < ps.size(); base++)
    {
        for (size_t face = 0; face < ps[base].faces.size(); face++)
        {
            // for each face of each voxel ... find the opposing voxel by
            // first checking all the other bases, and then checking the bases
            // around us
            int count = 0;
            Vector<int> found_d;
            int found_face = 0;

            std::vector<Vector<float>> f1;
            for (const auto v : ps[base].faces[face])
            {
                f1.push_back(ps[base].vertices[v]);
            }

            for (size_t base2 = 0; base2 < ps.size(); base2++)
                for (size_t face2 = 0; face2 < ps[base2].faces.size(); face2++)
                    for (auto & d : BurrTools::Maths::iterate3d<Vector<int>>(1))
                    {
                        // skip our own place
                        if (d.isZero() && base2 == base) continue;

                        // offset of the 2nd face
                        Vector<float> o = pht * d;

                        // assemble the 2nd face
                        std::vector<Vector<float>> f2;
                        for (const auto v : ps[base2].faces[face2])
                        {
                            f2.push_back(ps[base2].vertices[v] + o);
                        }

                        if (oppositeFace(f1, f2))
                        {
                            count++;
                            found_d    = bp[base2] - bp[base] + tra * d;
                            found_face = face2;
                        }
                    }

            if (count != 1)
            {
                return std::format("in grid {0} base-vector {1} face {2} has {3} opposites", grid.value, base, face, count);
            }

            // check, if all the numbers are in a proper range
            if (!std::in_range<int8_t>(found_d.x()) || !std::in_range<int8_t>(found_d.y()) || !std::in_range<int8_t>(found_d.z())
                || !std::in_range<int8_t>(found_face))
            {
                return std::format("current type of the face neighbour array doesn't allow the required values");
            }

            Grid::FaceNeighbour a;

            a.diff = Vector<int8_t>(found_d);
            a.face = found_face;

            faceNeighbours[base].emplace_back(a);
        }
    }

    return {};
}

// Helper type to represent information about one found edge
struct EdgeInformation
{
        Grid::EdgeIndex idx;  // position of the edge on the voxel
        Vector<int> position; // the voxel position (edge relative)
        int direction = 0;    // direction of the edge (same as given in index or reverse)
};

// calculate edge neighborhood information
static std::string calculateEdgeNeighbourhood(const std::vector<Grid::Polyhedron> & physicalShapes,
                                              const std::vector<Vector<int>> & basePositions,
                                              const Matrix<int> & translationalSymmetry,
                                              const Matrix<float> & inversePhysicalTranslation,
                                              const std::vector<std::vector<Grid::FaceNeighbour>> & faceNeighbours,

                                              std::vector<std::vector<Grid::EdgeLoopEntry>> & edgeLoops,
                                              std::vector<std::vector<std::vector<Grid::EdgeLoopIndex>>> & edgeIndices)
{
    // this is done in 3 steps:
    // 1) first collect edge sharing information about each polyhedron, meaning
    // if we have a face with an edge on this polyhedron, which other face
    // of that same polyhedron joins that face, collect that information in edgeShare
    //
    // 2) collect all edges and store which edge of which face of which base-polyhedron
    // uses that edge
    //
    // 3) for each edge found that way sort the polyhedra/voxel in a ring structure so that
    // you get one face after the other around the edge

    std::vector<std::vector<std::vector<std::pair<int, int>>>> edgeShare(physicalShapes.size());

    for (size_t base = 0; base < physicalShapes.size(); base++)
    {
        const auto & p = physicalShapes[base];

        edgeShare[base].resize(p.faces.size());

        // this is a double loop over all edges of all faces of the base polyhedron
        // if an edge is identical store that information
        for (size_t face = 0; face < p.faces.size(); face++)
        {
            const size_t corners = p.faces[face].size();

            edgeShare[base][face].resize(corners);

            for (size_t edge = 0; edge < corners; edge++)
            {
                const Vector<float> v1_1 = p.vertices[p.faces[face][edge]];
                const Vector<float> v1_2 = p.vertices[p.faces[face][(edge + 1) % corners]];

                // v1 and v2 now contain the coordinates of the edge from e1 -> e2
                // search for an edge from e2 -> e1 in all the other faces of the polyhedron
                for (size_t face2 = 0; face2 < p.faces.size(); face2++)
                {
                    if (face2 != face)
                    {
                        size_t corners2 = p.faces[face2].size();

                        for (size_t edge2 = 0; edge2 < corners2; edge2++)
                        {
                            const Vector<float> v2_1 = p.vertices[p.faces[face2][edge2]];
                            const Vector<float> v2_2 = p.vertices[p.faces[face2][(edge2 + 1) % corners2]];

                            if (equal(v1_1, v2_2, 0.00001f) && equal(v1_2, v2_1, 0.00001f)) edgeShare[base][face][edge] = std::make_pair(face2, edge2);
                        }
                    }
                }
            }
        }
    }

    // now find all the edges
    std::vector<std::pair<Grid::Edge, std::vector<EdgeInformation>>> edges;

    for (size_t base = 0; base < physicalShapes.size(); base++)
    {
        for (size_t face = 0; face < physicalShapes[base].faces.size(); face++)
        {
            const size_t corners = physicalShapes[base].faces[face].size();

            for (size_t edge = 0; edge < corners; edge++)
            {
                const size_t m = (edge + 1) % corners;

                Grid::Edge e;
                e.start = physicalShapes[base].vertices[physicalShapes[base].faces[face][edge]];
                e.end   = physicalShapes[base].vertices[physicalShapes[base].faces[face][m]];

                bool found = false;

                // see, if we already know this edge
                for (auto & [edgeIter, edgeInfo] : edges)
                {
                    if (auto [isSame, d, direction] = sameEdge(edgeIter, e, inversePhysicalTranslation); isSame)
                    {
                        // we do, store the new edge in that list
                        Vector pv = translationalSymmetry * d + basePositions[base];
                        EdgeInformation ei;
                        ei.idx       = Grid::EdgeIndex{static_cast<uint8_t>(base), static_cast<uint8_t>(face), static_cast<uint8_t>(edge)};
                        ei.position  = pv;
                        ei.direction = direction;
                        edgeInfo.push_back(ei);

                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    // this edge is so far unknown, create a new entry for it
                    EdgeInformation ei;
                    ei.idx       = Grid::EdgeIndex{static_cast<uint8_t>(base), static_cast<uint8_t>(face), static_cast<uint8_t>(edge)};
                    ei.position  = basePositions[base];
                    ei.direction = 1;

                    std::vector<EdgeInformation> pp;
                    pp.push_back(ei);
                    edges.emplace_back(e, pp);
                }
            }
        }
    }

    // now we need to sort these, this works by walking around the edge from base to base skipping
    // via the face neighbourhood data and then around the corner using the edgeShare data
    // resize the nested vectors to the sizes that we need because
    // the edgeIndices array will be filled in a random order
    edgeLoops.resize(edges.size());
    edgeIndices.resize(physicalShapes.size());

    for (size_t base = 0; base < physicalShapes.size(); base++)
    {
        edgeIndices[base].resize(physicalShapes[base].faces.size());

        for (size_t face = 0; face < physicalShapes[base].faces.size(); face++) edgeIndices[base][face].resize(physicalShapes[base].faces[face].size());
    }

    for (size_t e = 0; e < edges.size(); e++)
    {
        const auto & edge = edges[e].second;
        auto & eloop      = edgeLoops[e];
        Grid::EdgeLoopEntry loopEntry;

        // where are we in the edge loop
        size_t index = 0;

        // in the following loop we crawl along the list by first
        // going to a face neighbour and then to the edge neighbour of
        // the face
        // this way we circle around the edge one by one
        // along the way we collect the edge information
        do
        {
            // so first the face neighbour
            const Vector<int> n_d = faceNeighbours[edge[index].idx.base][edge[index].idx.face].diff;

            // search the new voxel with its face in the edge list
            int found = 0;
            for (size_t i = 0; i < edge.size(); i++)
            {
                if ((i != index) &&                                     // must be different from our current position
                    (edge[index].position + n_d == edge[i].position) && // coordinates must be right TODO this can go wrong potentially
                    (edge[index].direction == -edge[i].direction))      // inverse direction
                {
                    // found it
                    index = i;
                    found++;
                }
            }

            if (found != 1)
            {
                return std::format("oops, something went wrong, found {0} choices for edge sorting step 1", found);
            }

            // store that information in the entry and the index
            loopEntry.edges[0] = edge[index].idx;

            {
                auto & indexEntry    = edgeIndices[edge[index].idx.base][edge[index].idx.face][edge[index].idx.edge];
                indexEntry.edge      = e;
                indexEntry.index     = eloop.size();
                indexEntry.direction = 0;
            }

            // now find the other face that shares this edge we find that one using the above collected
            // edge sharing information

            auto [newface, newedge] = edgeShare[edge[index].idx.base][edge[index].idx.face][edge[index].idx.edge];

            // and again find that edge in the edge list
            found = 0;
            for (size_t i = 0; i < edge.size(); i++)
            {
                if ((i != index) &&                               // must be different from our current position
                    (edge[i].idx.base == edge[index].idx.base) && // same voxel so same base
                    (edge[i].idx.face == newface) &&              // face must be the new one
                    (edge[i].idx.edge == newedge))                // edge must be the new one
                {
                    // found it
                    index = i;
                    found++;
                }
            }

            if (found != 1)
            {
                return std::format("oops, something went wrong, found {0} choices for edge sorting step 2", found);
            }

            // store the information
            {
                auto & indexEntry    = edgeIndices[edge[index].idx.base][edge[index].idx.face][edge[index].idx.edge];
                indexEntry.edge      = e;
                indexEntry.index     = eloop.size();
                indexEntry.direction = 1;
            }

            loopEntry.position = edge[index].position;
            loopEntry.edges[1] = edge[index].idx;

            // finally store the loop entry
            eloop.push_back(loopEntry);

            // we are done, when we are back at index 0
        } while (index != 0);
    }

    // check, that the index datatype is big enough for all indices
    for (const auto & e : edgeLoops)
        if (e.size() > std::numeric_limits<decltype(Grid::EdgeLoopIndex::edge)>::max())
        {
            return std::format("oops, more voxels at one vertex than expected... please increase type");
        }

    return {};
}

static std::string calculateCornerNeighbourhood(const std::vector<Grid::Polyhedron> & physicalShapes,
                                                const Matrix<float> & physicalTranslations,
                                                const std::vector<Vector<int>> & basePositions,
                                                const Matrix<int> & translationalSymmetry,
                                                std::vector<std::vector<Vector<int8_t>>> & cornerList,
                                                std::vector<std::vector<std::vector<Grid::CornerListIndex>>> & cornerIndex)
{
    // helper variable that assigns a corner id to physical corner coordinates
    std::vector<std::pair<Vector<float>, uint8_t>> cornerMap;

    // iterate over all corners of all faces of all bases
    cornerIndex.resize(physicalShapes.size());
    for (size_t base = 0; base < physicalShapes.size(); base++)
    {
        cornerIndex[base].resize(physicalShapes[base].faces.size());
        for (size_t face = 0; face < physicalShapes[base].faces.size(); face++)
        {
            cornerIndex[base][face].resize(physicalShapes[base].faces[face].size());
            for (size_t corner = 0; corner < physicalShapes[base].faces[face].size(); corner++)
            {
                // find "normalized" corner coordinate by
                // searching through all possible physical translations
                // and finding the one with the "smallest" position

                size_t cornerKnown = std::numeric_limits<size_t>::max();

                // iterate over all base epipeds around the current one
                // that is needed because the corner we are currently
                // exploring might be the corner of the base epiped and
                // we need to look into the neighbour epipeds to find
                // all neighbours
                for (auto & d : BurrTools::Maths::iterate3d<Vector<int>>(1))
                {
                    // calculate the physical position of the corner within the current epiped
                    const auto c_idx       = physicalShapes[base].faces[face][corner];
                    const Vector<float> c1 = physicalShapes[base].vertices[c_idx];
                    const Vector<float> c2 = physicalTranslations * d;
                    const Vector<float> c  = c1 + c2;

                    // check, if we already know that physical position
                    const auto ci = std::ranges::find_if(cornerMap, [&c](auto & v) { return equal(v.first, c, 0.00001f); });

                    if (ci != cornerMap.end())
                    {
                        // ok, we know that physical position already as a corner

                        // if we have already found that corner, it must be the same index...
                        if (cornerKnown != std::numeric_limits<size_t>::max() && cornerKnown != ci->second)
                        {
                            return "double corner found... something is wrong with my assertions, please inform BurrTools programmers";
                        }

                        // look into corners and check, if the voxel coordinates are already
                        // in there
                        Vector v = basePositions[base] + translationalSymmetry * d;

                        // check if that position is already known
                        bool cornerFound = false;
                        for (size_t i = 0; i < cornerList[ci->second].size(); i++)
                            if (cornerList[ci->second][i] == v)
                            {
                                cornerIndex[base][face][corner].corner = ci->second;
                                cornerIndex[base][face][corner].index  = i;
                                cornerFound                            = true;
                            }

                        if (!cornerFound)
                        {
                            cornerIndex[base][face][corner].corner = ci->second;
                            cornerIndex[base][face][corner].index  = cornerList[ci->second].size();
                            cornerList[ci->second].emplace_back(v);
                        }

                        cornerKnown = ci->second;
                    }
                }

                // physical position of the current corner was unknown, so it
                // was a completely new corner
                // add that corner to the list
                if (!cornerKnown)
                {
                    size_t i = cornerList.size();
                    // first add physical position to the cornerMap
                    cornerMap.emplace_back(Vector<float>(physicalShapes[base].vertices[physicalShapes[base].faces[face][corner]]), i);

                    // add a new corner to the output corner list and add the current voxel
                    // position to the voxels that share that corner
                    cornerList.resize(i + 1);
                    cornerList[i].emplace_back(basePositions[base]);

                    // and finally add the index for the new corner, use the new index and we are
                    // at position zero of that corner list
                    cornerIndex[base][face][corner].corner = i;
                    cornerIndex[base][face][corner].index  = 0;
                }
            }
        }
    }

    // check, that the index data-type is big enough for all indices
    for (const auto & c : cornerList)
        if (c.size() > std::numeric_limits<decltype(Grid::CornerListIndex::corner)>::max())
            return "oops, more voxels at one vertex than expected... please inform BurrTools programmers";

    return {};
}

// check if the physical shape fulfils certain constraints
// also calculate the centre value
static std::string checkPhysicalShapes(std::vector<Grid::Polyhedron> & physicalShapes, Grid::StorageId grid)
{
    for (size_t base = 0; base < physicalShapes.size(); base++)
    {
        auto & [physVertices, physFaces, physCenter] = physicalShapes[base];

        // make sure that the number of coordinates within the corner lists of the face
        // are correct
        for (size_t face = 0; face < physFaces.size(); face++)
        {
            if (physFaces[face].size() < 3)
            {
                return "faces for physical shapes must have at least 3 corners";
            }

            for (const auto c : physFaces[face])
                if (c > physVertices.size())
                {
                    return "physical shape broken: face indices must point into vertex array";
                }

            // calculate the normal of the first 3 points
            const Vector<float> a = physVertices[physFaces[face][2]] - physVertices[physFaces[face][1]];
            const Vector<float> b = physVertices[physFaces[face][0]] - physVertices[physFaces[face][1]];
            const Vector<float> n = cross(a, b);

            if (physFaces[face].size() > 3)
            {
                // more than 3 points, check for co-planarity

                // for the remaining points we need to make sure that
                // the vector from one of the first 2 points to that point
                // is perpendicular to the normal vector
                for (size_t j = 3; j < physFaces[face].size(); j++)
                {
                    const Vector<float> c = physVertices[physFaces[face][j]] - physVertices[physFaces[face][0]];
                    const float t         = c * n;

                    if (!closeto(t, 0))
                    {
                        return std::format("points of grid {0}, base {1}, face {2} are not coplanar", grid.value, base, face);
                    }
                }

                // convexity we check if the face:
                // check that the cross product always points in the same direction
                for (size_t j = 1; j < physFaces[face].size(); j++)
                {
                    const auto a2 =
                        physVertices[physFaces[face][((j + 2) % physFaces[face].size())]] - physVertices[physFaces[face][((j + 1) % physFaces[face].size())]];
                    const auto b2 =
                        physVertices[physFaces[face][((j + 0) % physFaces[face].size())]] - physVertices[physFaces[face][((j + 1) % physFaces[face].size())]];

                    const Vector<float> n2 = cross(a2, b2);

                    // if the scalar product is non-negative, then we are good, because both normal vectors
                    // point in the same direction
                    if (const float t = n * n2; t < 0)
                    {
                        return std::format("face of grid {0}, base {1}, face {2} seems to be non convex", grid.value, base, face);
                    }
                }
            }

            // convexity of the whole polyhedron
            // we check that the vertices of all other faces are below or on the
            // plane of this face

            // a point on the plane of the first face
            Vector<float> c = physVertices[physFaces[face][0]];
            for (size_t face2 = 0; face2 < physFaces.size(); face2++)
                if (face2 != face)
                    for (const unsigned long j : physFaces[face2])
                    {
                        if (const float x = n * (physVertices[j] - c); x < 0.0001)
                        {
                            // everything all right
                        }
                        else
                        {
                            // this is not strictly necessary, but some algorithms are much simpler with this
                            // assumption fulfilled, so, if we want to get rid of it
                            // we need to improve those first... or maybe simply allow
                            // movement analysis only on grids with convex voxels....
                            return std::format("seems like grid {0}, base {1} is not a convex polyhedron", grid.value, base);
                        }
                    }
        }

        for (size_t face = 0; face < physFaces.size(); face++)
        {
            // calculate number of corners of the face
            const size_t corners = physFaces[face].size();

            // go over each edge of the face
            for (size_t edge = 0; edge < corners; edge++)
            {
                const Vector<float> v1_1 = physVertices[physFaces[face][edge]];
                const Vector<float> v1_2 = physVertices[physFaces[face][(edge + 1) % corners]];

                // v1 and v2 now contain the coordinates of the edge from e1 -> e2
                // search for an edge from e2 -> e1 in all the other faces of the polyhedron
                int count = 0;

                for (size_t face2 = 0; face2 < physFaces.size(); face2++)
                {
                    if (face2 != face)
                    {
                        size_t corners2 = physFaces[face2].size();

                        for (size_t edge2 = 0; edge2 < corners2; edge2++)
                        {
                            const Vector<float> v2_1 = physVertices[physFaces[face2][edge2]];
                            const Vector<float> v2_2 = physVertices[physFaces[face2][(edge2 + 1) % corners2]];

                            // theoretically we could directly compare because these values
                            // are not calculated but taken from lookups... but better safe
                            // than sorry
                            if (equal(v1_1, v2_2, 0.00001f) && equal(v1_2, v2_1, 0.00001f))
                            {
                                count++;
                            }
                        }
                    }
                }

                if (count != 1)
                {
                    return std::format("polyhedron {0} in grid {1} doesn't conform to the halfedge property: there are {2} opposites for edge {3} on face {4}",
                                       base,
                                       grid.value,
                                       count,
                                       edge,
                                       face);
                }
            }
        }

        // calculate the centre
        Vector<float> center;
        for (const auto & v : physVertices) center += v;
        physCenter = center * (1.0f / physVertices.size());
    }

    return {};
}

// calculate the movement check positions, meaning the information which voxels need to be empty
// to move a certain voxel along a certain vector
static void calculateMovementChecks(const std::vector<Vector<int>> & movementVectors,
                                    const Matrix<int> & translationalSymmetry,
                                    const RMatrix<int> & inverseTranslationalSymmetry,
                                    const std::vector<Vector<int>> & basePositions,
                                    const Matrix<float> & physicalTranslations,
                                    const std::vector<Grid::Polyhedron> & physicalShapes,
                                    std::vector<std::vector<std::vector<Vector<int8_t>>>> & movementChecks,
                                    std::vector<Grid::RotationalSymmetry> & rotationalSymmetries)
{
    // this is done by calculating a shape that needs to be empty to accomplish
    // the required voxel movement
    // and then check which voxels intersect that shape
    // as this is quite costly to do, we try to re-use already done calculations by rotating
    // the check vectors...

    // first resize the output vectors properly
    movementChecks.resize(physicalShapes.size());
    for (size_t base = 0; base < physicalShapes.size(); base++) movementChecks[base].resize(movementVectors.size());

    for (size_t base = 0; base < physicalShapes.size(); base++)
    {
        // create a single voxel of the current base
        // with a small offset to the centre (so that the intersection check doesn't
        // accidentally find an intersection for touching faces
        //
        // this will be used below in the intersection check
        std::vector<Vector<float>> movementHull;

        // simply take the centre move so that the centre is at the origin, scale down a bit
        // and move back
        for (auto & v : physicalShapes[base].vertices) movementHull.push_back(0.99f * (v - physicalShapes[base].center) + physicalShapes[base].center);

        for (size_t direction = 0; direction < movementVectors.size(); direction++)
        {
            // look for another direction already done, that is a symmetric direction and can be reused/copied
            bool found = false;

            for (size_t dir2 = 0; dir2 < movementVectors.size(); dir2++)
            {
                // we check all possible orientations of the grid, for that
                // we use the rotational symmetries of the grid, so go over all symmetries
                for (auto [p0, dir, r] : rotationalSymmetries)
                {
                    // take the movement vector and transform is according to current rotation
                    auto dir2t = r * movementVectors[dir2];

                    // if the resulting movement vector is known, try that one
                    if (dir2t == movementVectors[direction])
                    {
                        // ok, rotation r transforms our current movement vector to an other movement vector
                        // now calculate the corresponding base

                        // first we need to find the corresponding base from the lookup
                        // position that we want to copy from, because the selected rotation r
                        // gets us from dir2 to direction, the inverse gets us the other way
                        // so if we transform base (which goes with dir, the copy to data)
                        // by ~r we get to base2
                        auto basepos2 = p0 + (~r) * (basePositions[base] - p0);

                        // normalize the base position
                        {
                            BurrTools::Maths::RVector<int> a = inverseTranslationalSymmetry * basepos2;
                            BurrTools::Maths::Vector<int> b  = a.getInteger();
                            BurrTools::Maths::Vector<int> c  = translationalSymmetry * b;

                            basepos2 -= c;
                        }

                        // find the base index
                        size_t base2 = 0;

                        while (!(basePositions[base2] == basepos2))
                        {
                            base2++;

                            if (base2 >= basePositions.size()) break;
                        }

                        // if we already calculated the base direction pair, we can copy it over
                        if (base2 < base || ((base2 == base) && (dir2 < direction)))
                        {
                            // now transform the content
                            for (auto & c : movementChecks[base2][dir2])
                            {
                                auto d = r * c;
                                assert(d.getFraction().isZero());
                                movementChecks[base][direction].emplace_back(d.getInteger());
                            }

                            found = true;

                            // break out of both loops, no need to continue
                            dir2 = movementVectors.size();
                            break;
                        }
                    }
                }
            }

            if (!found)
            {
                // nothing found for copying, so we need to calculate it
                // for that we use the Minkovski portal refinement in Maths::MPR
                //
                // this algorithm need a structure with functions that returns centres and support points
                // for the 2 polyhedra that we want to check
                //
                // this is done by the following structure.
                // The structure is quite specialized for the very task at hand
                // polyhedron 1 is defined by
                // - the movementHull polyhedron calculated above (slightly smaller than the current voxel)
                // - and a movement direction
                // those 2 are Minkowsky summed to the volume that is traversed by the voxel
                //
                // polyhedron 2 is the voxel that we want to check against, it is defined by
                // - accessing the vertices inside physical shapes array
                // - the base to use inside that shapes array
                // - an offset to move the voxel around
                //
                // all this makes updating the structure for the next test very fast
                struct MPRHelper
                {
                        typedef float CoordinateType;

                        // definition for the polyhedron 1
                        const std::vector<Vector<CoordinateType>> & v1; // the shape
                        Vector<CoordinateType> v1Dir;                   // the movement direction
                        Vector<CoordinateType> v1Center;                // the centre of the shape

                        // dito for polyhedron 2
                        const std::vector<BurrTools::PuzzleLibrary::Grid::Polyhedron> & v2; // the array of shapes
                        Vector<CoordinateType> v2Offset;                                    // the movement of that shape relative to its definition
                        size_t v2base = 0;                                                  // which of the shapes in v2 to use

                        // helper function to calculate the support function
                        // it simply searches for the vertex with the biggest dot-product with the direction vector
                        // the whole shape is moved by the offset parameter
                        [[nodiscard]] static Vector<CoordinateType>
                        support(const std::vector<Vector<CoordinateType>> & v, const Vector<CoordinateType> & dir, const Vector<CoordinateType> & offset)
                        {
                            return std::ranges::max(v, {}, [&dir, &offset](auto & vi) -> float { return (vi + offset) * dir; }) + offset;
                        }

                        [[nodiscard]] Vector<CoordinateType> support1(const Vector<CoordinateType> & d) const
                        {
                            // calculate support function for polyhedron 1 by
                            // first calculating the support for the basic shape
                            auto s = support(v1, d, Vector<CoordinateType>(0, 0, 0));
                            // and then adding the support function of the movement "line"
                            // this sum is the Minkowsky sum
                            if (d * v1Dir > 0) return s += v1Dir;
                            return s;
                        }

                        [[nodiscard]] Vector<CoordinateType> support2(const Vector<CoordinateType> & d) const
                        {
                            return support(v2[v2base].vertices, d, v2Offset);
                        }

                        // the centre functions use the pre-calculated centres
                        [[nodiscard]] Vector<CoordinateType> center1() const { return v1Center + 0.5 * v1Dir; }

                        [[nodiscard]] Vector<CoordinateType> center2() const { return v2[v2base].center + v2Offset; }

                        MPRHelper(const std::vector<Vector<CoordinateType>> & w1,
                                  const Vector<CoordinateType> & v1c,
                                  const std::vector<BurrTools::PuzzleLibrary::Grid::Polyhedron> & w2,
                                  const Vector<CoordinateType> & dir)
                            : v1(w1),
                              v1Dir(dir),
                              v1Center(v1c),
                              v2(w2)
                        {
                        }
                };

                // calculate the movement that the movementHull polyhedron is performing
                auto physicalMovementDir = physicalTranslations * inverseTranslationalSymmetry * movementVectors[direction];
                MPRHelper c(movementHull, physicalShapes[base].center, physicalShapes, physicalMovementDir);

                // now iterate over all potentially intersecting voxels
                for (size_t b = 0; b < physicalShapes.size(); b++)
                {
                    c.v2base = b;

                    // we iterate here over 2 sizes of the base epiped away, that should be more
                    // than enough... if not we might want to include a grid parameter to
                    // limit the search to useful values
                    for (auto & d : BurrTools::Maths::iterate3d<Vector<int>>(2))
                        // check if we have an interesting voxel
                        if (!d.isZero() || b != base)
                        {
                            // calculate movement offset of the voxel to check
                            c.v2Offset = physicalTranslations * d;

                            // check if we intersect
                            if (BurrTools::Maths::MPR::intersects3d(c))
                            {
                                // if so add the difference voxel coordinates to the positions to check
                                Vector<int8_t> e = translationalSymmetry * d + basePositions[b] - basePositions[base];
                                movementChecks[base][direction].push_back(e);
                            }
                        }
                }
            }
        }
    }
}

// calculate the lookup table to find out which base a voxel belongs to
static std::string
calculateBaseLookup(const std::vector<Vector<int>> & basePositions, std::array<int, 3> & baseIndexCoefficients, std::vector<int8_t> & baseVectors)
{
    int xmin, ymin, zmin;
    int xmax, ymax, zmax;

    xmin = ymin = zmin = 100'000;
    xmax = ymax = zmax = -100'000;

    for (auto basePosition : basePositions)
    {
        xmin = std::min(xmin, basePosition.x());
        xmax = std::max(xmax, basePosition.x());

        ymin = std::min(ymin, basePosition.y());
        ymax = std::max(ymax, basePosition.y());

        zmin = std::min(zmin, basePosition.z());
        zmax = std::max(zmax, basePosition.z());
    }

    baseIndexCoefficients = {ymax - ymin + 1, xmax - xmin + 1, -(xmin + (xmax - xmin + 1) * (ymin + (ymax - ymin + 1) * zmin))};
    baseVectors.resize((xmax - xmin + 1) * (ymax - ymin + 1) * (zmax - zmin + 1));

    std::ranges::fill(baseVectors, -1);

    for (size_t base = 0; base < basePositions.size(); base++)
    {
        const auto & bi = baseIndexCoefficients;
        const auto & bp = basePositions[base];

        const int i = (bp.z() * bi[0] + bp.y()) * bi[1] + bp.x() + bi[2];

        if (i < 0)
        {
            return std::format("oops, base vector coefficient calculation is faulty");
        }

        const auto j = static_cast<size_t>(i);

        if (j >= baseVectors.size())
        {
            return std::format("oops, base vector coefficient calculation is faulty");
        }

        baseVectors[j] = base;
    }

    return {};
}

static std::string prepareSnapPoints(std::vector<Grid::GridSnapPoint> & s)
{
    for (auto & n : s)
    {
        // normalize the normal vectors
        const float l = n.normal * n.normal;
        if (l < 0.001)
        {
            return "snap point normal too short";
        }
        n.normal /= std::sqrt(l);

        const Vector<float> targetnorm(0, 0, 1.0);
        const Vector<float> targetup(0, -1.0, 0);

        // first calculate the rotation to get the normals point into the right direction:

        Vector<float> axis1 = cross(targetnorm, n.normal);
        const float angle1  = std::acos(targetnorm * n.normal);

        Quaternion<float> rotation(1, 0, 0, 0);

        if (angle1 > 0.00001)
        {
            if (angle1 < std::numbers::pi - 0.00001)
            {
                axis1 /= static_cast<float>(sqrt(axis1 * axis1));
                rotation = angleAxis(-angle1, axis1);
            }
            else
            {
                rotation = angleAxis<float>(std::numbers::pi, targetup);
            }
        }

        // if is apply this rotation to the normal, we should end up at the target normal
        if (!equal(targetnorm, rotation * n.normal, 0.00001f))
        {
            auto t = rotation * n.normal;
            return std::format("oops, something is wrong with the snap point normal alignment 0, 0, 1 <--> {}, {}, {} ->  {}, {}, {}",
                               n.normal.x(),
                               n.normal.y(),
                               n.normal.z(),
                               t.x(),
                               t.y(),
                               t.z());
        }

        for (const auto & up : n.ups)
        {
            // now calculate the first up vector
            const Vector<float> upNormalAligned = rotation * up.direction;

            // project it onto the x,y-plane
            Vector<float> upNormalProjected(upNormalAligned.x(), upNormalAligned.y(), 0);

            float len = upNormalProjected * upNormalProjected;

            if (len < 0.00001)
            {
                return "hmm, one of the up-direction doesn't seem to be at an angle with the normal plane";
            }

            len = std::sqrt(len);
            upNormalProjected /= len;

            float angle2 = std::acos(upNormalProjected * targetup);

            if (targetnorm * cross(upNormalProjected, targetup) < 0) angle2 *= -1;

            auto rotation2 = rotate(angleAxis(angle2, targetnorm), rotation);

            // now we have the starting orientation, let's create all symmetrical matrices
            Quaternion<float> symrot = angleAxis<float>((2.0f * std::numbers::pi / up.symmetry), targetnorm);

            for (size_t sym = 0; sym < up.symmetry; sym++)
            {
                n.orientations.push_back(invert(rotation2));

                rotation2 = rotate(symrot, rotation2);
            }
        }
    }

    return {};
}

// given an axis, calculate whether that axis could be
// a symmetry axis for a given grid
// return 0 if no symmetry can occur and otherwise
// return the angle in degree that this could be an axis
static bool gridContainsSymmetry(const TMatrix<int> & m,
                                 const Matrix<int> & f,                  // forward translational
                                 const RMatrix<int> & g,                 // inverse translational
                                 const std::vector<Vector<int>> & bases, // base vectors
                                 const std::vector<Grid::Polyhedron> & shapes,
                                 const Matrix<float> & p,
                                 const Matrix<float> & ip,
                                 const Vector<float> & po)
{
    // we perform 2 checks:
    // 1) check that valid voxels from the grid, when rotated
    //    around that axis return to valid base position voxels
    // 2) check that the physical shape of the voxels at their
    //    new rotated position matches with the shape of the voxel
    //    at that base-position

    // do those 2 checks for each base
    for (size_t base = 0; base < bases.size(); base++)
    {
        // transform the base
        const RVector<int> btt = m * bases[base];

        // if not on a whole position, we can't be right
        if (!btt.getFraction().isZero()) return false;

        const Vector<int> bt = btt.getInteger(std::divides());

        // normalize bt and find out its base position
        const RVector<int> a = g * bt;
        const Vector<int> b(intdiv_inf(a.x(), a.denominator()), intdiv_inf(a.y(), a.denominator()), intdiv_inf(a.z(), a.denominator()));
        const Vector<int> c = f * b;
        const Vector<int> d = bt - c;

        // check 1
        // check if d is a member of bases, if not
        // the voxel would end up in an invalid position for
        // a symmetry, so it can't be one
        size_t base2 = 0;
        while (base2 < bases.size())
        {
            if (bases[base2] == d) break;
            base2++;
        }

        // if no base found -> no symmetry of grid
        if (base2 >= bases.size()) return false;

        // check 2

        // transform the physical shape of base and check if it is now identical with the
        // physical shape on the 2nd base
        // we only check the vertices and not of the appended triangles fit...
        for (const Vector<float> & v : shapes[base].vertices)
        {
            // first transform from physical coordinates to voxel coordinates (because
            // the matrix m is for voxel coordinated
            const auto v2 = ip * v;
            const auto v3 = f * v2 - po;
            // apply m and get back into the base epiped
            const auto v4 = m * v3;
            const auto v5 = v4 - c + po;
            // and now back to physical coordinates
            const auto v6 = g * v5;
            const auto v7 = p * v6;

            // find the same vertex in the shape at the 2nd base .. if we find none, we are not symmetrical
            if (std::ranges::none_of(shapes[base2].vertices, [&v7](const auto & v8) { return equal(v8, v7, 0.00001f); })) return false;
        }
    }

    // both tests passed -> symmetric
    return true;
}

// for a given grid definition with corresponding voxel definition
// try to find all rotation and mirror symmetries
static void findGridSymmetries(const RMatrix<int> & g,   // inverse translational
                               const Matrix<int> & f,    // forward translational
                               const Matrix<float> & p,  // physical
                               const Matrix<float> & ip, // physical offset
                               const Vector<float> & po,
                               const std::vector<Vector<int>> & b, // base vectors
                               const std::vector<Grid::Polyhedron> & s,

                               std::vector<Grid::RotationalSymmetry> & rots,
                               std::vector<Grid::MirrorSymmetry> & mirrorSymmetries,
                               std::array<std::vector<RVector<int16_t>>, TRotation::NUM_ROTATION_MATRICES> & validTrans,

                               uint8_t & maxSymmetryGroup)
{
    // let's first do mirror symmetries

    std::vector<Grid::RotationalSymmetry> rout;
    std::vector<Grid::MirrorSymmetry> mout;

    int startmax = 2;

    TRotationSet symmask;
    symmask.add(TRotation(TRotation::ROT_IDENTITY));

    // the furthest point away is from logical (1,1,1) to voxel
    const auto opposite = f * Vector<int>(1, 1, 1);

    int maxc = std::max({opposite.x(), opposite.y(), opposite.z()});

    // due to the rational numbers, we need to multiply by the denominator
    maxc *= startmax;

    std::vector<RVector<int>> epipedpoints;

    std::vector<TMatrix<int>> validTransformationList;

    // create a list of all possible voxel positions inside the epiped using
    // rational numbers
    for (auto & p0int : BurrTools::Maths::iterate3d<Vector<int>>(maxc))
    {
        // rotation axis point in the voxel coordinate system
        RVector<int> p0(p0int, startmax);

        // the rotation point inside the grid coordinate system
        const auto n = g * p0;

        // if we are inside the base epiped, but not
        // on the far sides, add to possible starting points
        if (n.x() >= 0 && n.x() < n.denominator() && n.y() >= 0 && n.y() < n.denominator() && n.z() >= 0 && n.z() < n.denominator())
        {
            // well found a valid position inside the epiped, create a transformation matrix that rotates around this point
            unsigned int r = 1;

            while (r < TRotation::NUM_ROTATION_MATRICES / 2)
            {
                // when the direction and an other point already checked are on the same
                // direction, we skip this direction
                bool known = false;

                for (const auto & ep : epipedpoints)
                {
                    float dot = static_cast<float>(TRotation(r).matrixInfo().direction * (ep - p0));

                    dot /= (length<float>(ep - p0) * length<float>(TRotation(r).matrixInfo().direction));

                    if (dot > 0.9999 || dot < -0.9999)
                    {
                        known = true;
                        break;
                    }
                }

                const auto m = TMatrix(p0) * TMatrix<int>(TRotation(r)) * TMatrix(-p0);

                // check, if the grid is rotationally symmetric round this transformation

                if (known || gridContainsSymmetry(m, f, g, b, s, p, ip, po))
                {
                    if (!known)
                    {
                        rout.push_back(Grid::RotationalSymmetry{p0, TRotation(r).matrixInfo().direction, TRotation(r)});
                        symmask.add(TRotation(r));

                        auto m2 = m;

                        // also add this symmetry to the valid transformations
                        // do this with a loop because we need all possible orientations
                        // and not just e.g. the 90° one, so apply the transformation multiple times
                        while (true)
                        {
                            const auto a1 = g * m2.translation();
                            const auto a2 = a1.getInteger();
                            const auto a3 = f * a2;

                            auto res = m2.translation() - a3;
                            res.reduce();

                            if (std::find(validTrans[m2.rotation().value()].begin(), validTrans[m2.rotation().value()].end(), res)
                                == validTrans[m2.rotation().value()].end())
                            {
                                validTransformationList.push_back(m2);
                                validTrans[m2.rotation().value()].emplace_back(res);
                            }

                            m2 = m * m2;

                            if (m2 == m) break;
                        }
                    }

                    const auto angle = TRotation(r).matrixInfo().angle;

                    // skip all other checks of this axis
                    while (true)
                    {
                        r++;
                        if ((r >= TRotation::NUM_ROTATION_MATRICES / 2)
                            || (TRotation(r).matrixInfo().direction
                                != TRotation((r + TRotation::NUM_ROTATION_MATRICES / 2 - 1) % (TRotation::NUM_ROTATION_MATRICES / 2)).matrixInfo().direction))
                            break;

                        if (!known && ((TRotation(r).matrixInfo().angle % angle) == Maths::DegreeInt{0})) symmask.add(TRotation(r));
                    }
                }
                else
                {
                    // ok, no symmetry, next angle
                    // but if the angle is bigger than 180 degree, we skip them

                    r++;

                    while ((r < TRotation::NUM_ROTATION_MATRICES / 2) && (TRotation(r).matrixInfo().angle > Maths::DegreeInt{180})) r++;
                }
            }

            // repeat a similar process for mirror planes
            while (r < TRotation::NUM_ROTATION_MATRICES)
            {
                // calculate the mirror plane normal
                // this is simply based on the assumption that the diagonal of a mirror plane
                // always has the following values: 1-normal_x^2, 1-normal_y^2, ...)

                float na = 1.0f * TRotation(r).matrix()[0] / TRotation(r).matrix().denominator();
                float nb = 1.0f * TRotation(r).matrix()[4] / TRotation(r).matrix().denominator();
                float nc = 1.0f * TRotation(r).matrix()[8] / TRotation(r).matrix().denominator();

                na = (1.0f - na) / 2.0f;
                nb = (1.0f - nb) / 2.0f;
                nc = (1.0f - nc) / 2.0f;

                // check, if close to normal, otherwise r can not be a mirror transform
                if (closeto(na + nb + nc, 1.0f))
                {
                    // look at the Wikipedia article on Transformation matrix
                    // for a formula on how to calculate the transformation
                    // matrix for a given plane through the origin
                    //
                    // here we kinda invert the whole process by taking the diagonal
                    // entries and with them first calculate the values of the normal vector
                    // due to the squaring in the formula we don't know the sign though
                    // that sign is deduced by looking at the -2ab... values

                    // so first calculate the normal
                    na = std::sqrt(na);
                    nb = std::sqrt(nb);
                    nc = std::sqrt(nc);

                    // normalize the value
                    float minn;

                    // find the smallest nonzero component
                    if (na > 0.0001)
                    {
                        minn = na;
                        if (nb > 0.0001) minn = std::min(minn, nb);
                        if (nc > 0.0001) minn = std::min(minn, nc);
                    }
                    else if (nb > 0.0001)
                    {
                        minn = nb;
                        if (nc > 0.0001) minn = std::min(minn, nc);
                    }
                    else if (nc > 0.0001)
                    {
                        minn = nc;
                    }
                    else
                        minn = 1.0f;

                    // normalize
                    na /= minn;
                    nb /= minn;
                    nc /= minn;

                    // round to values, we assume that this works... with
                    // our rotation matrices it does
                    na = std::round(na);
                    nb = std::round(nb);
                    nc = std::round(nc);

                    // this used to be a reference and it actually worked with that
                    // reference because the return value references a static array and
                    // thus does not die, but if we at a later time change that this would
                    // have dangled... so better make a copy even if not needed
                    const auto matrix = TRotation(r).matrix();

                    // now we determine the sign of the 2 components
                    // of the normal vector. This is quite arbitrary
                    // I tried to find a simple logic to determine
                    // which components needs inverting...
                    const bool ab_pos = matrix[1] > 0;
                    const bool ac_pos = matrix[2] > 0;

                    const int sum = matrix[1] + matrix[2] + matrix[5];

                    if (ab_pos)
                    {
                        nb = -nb;
                        if (ac_pos) nc = -nc;
                    }
                    else if (sum == matrix.denominator())
                    {
                        nc = -nc;
                    }

                    // finally here is the normal vector
                    const Vector<int> normal(na, nb, nc);

                    // create the complete transformation
                    const auto m = TMatrix(p0) * TMatrix<int>(TRotation(r)) * TMatrix(-p0);

                    if (gridContainsSymmetry(m, f, g, b, s, p, ip, po))
                    {
                        // found a plane, do calculate the distance value
                        const auto d = normal * p0;

                        TRotation rot(r);

                        // check if this is new
                        bool known = false;

                        for (const auto & [plane, transformation] : mout)
                        {
                            auto i = BurrTools::Maths::pointPlaneDistance(p0, plane);
                            known |= (i.numerator() == 0 && transformation == rot);
                        }

                        // and create the entry in the mirror symmetry array
                        // as d is a rational number we need to scale the normal so that we can
                        // keep our integer numbers
                        if (!known)
                        {
                            mout.push_back(Grid::MirrorSymmetry{
                                {normal * d.denominator(), d.numerator()},
                                rot
                            });

                            const auto a1 = g * m.translation();
                            const auto a2 = a1.getInteger();
                            const auto a3 = f * a2;

                            auto res = m.translation() - a3;
                            res.reduce();

                            if (std::find(validTrans[m.rotation().value()].begin(), validTrans[m.rotation().value()].end(), res)
                                == validTrans[m.rotation().value()].end())
                            {
                                validTransformationList.push_back(m);
                                validTrans[m.rotation().value()].emplace_back(res);
                            }
                        }

                        symmask.add(TRotation(r));
                    }
                }
                r++;
            }

            epipedpoints.push_back(p0);
        }
    }

    // complete the group in symmask
    {
        bool changed;
        do
        {
            changed = false;

            for (auto i : symmask)
                for (auto j : symmask)
                    if (const auto m = i * j; !symmask.contains(m))
                    {
                        changed = true;
                        symmask.add(m);
                    }

        } while (changed);
    }

    // try to find more valid transformations by combining 2 of the found ones
    // we will have found all the rotation transformations but we might need
    // additional mirror ones...
    for (auto t1 : validTransformationList)
        for (auto t2 : validTransformationList)
        {
            const auto m = t1 * t2;

            const auto a1 = g * m.translation();
            const auto a2 = a1.getInteger();
            const auto a3 = f * a2;

            auto res = m.translation() - a3;
            res.reduce();

            if (std::find(validTrans[m.rotation().value()].begin(), validTrans[m.rotation().value()].end(), res) == validTrans[m.rotation().value()].end())
                validTrans[m.rotation().value()].emplace_back(res);
        }

    // find the corresponding symmetry group
    {
        bool found = false;

        for (auto sym : RotationalSymmetryGroup::allGroups())
            if (sym.symmetries() == symmask)
            {
                found            = true;
                maxSymmetryGroup = sym.value();
                break;
            }

        // ops, that should not happen
        if (!found) throw std::runtime_error("calculating maximum symmetry group failed");
    }

    std::swap(rots, rout);
    std::swap(mirrorSymmetries, mout);
}

std::string Grid::registerGrid(RegisterInformation && inf2)
{
    Information inf{std::move(inf2)};

    // check sizes of vectors:
    if (inf.basePositions.size() != inf.physicalShapes.size())
    {
        return "there needs to be a physical shape for each base";
    }

    // calculate the inverse translational symmetry matrix
    // also calculate the inverse of the physical matrix
    inf.inverseTranslationalSymmetry = invert(inf.translationalSymmetry, inf.translationalSymmetry.determinant());
    if (inf.inverseTranslationalSymmetry.denominator() == 0)
    {
        return "grid table error: translational Symmetry not invertible";
    }

    if (const float d = inf.physicalTranslations.determinant(); std::fabs(d) < 0.0001)
        return "grid table error: physical translations not invertible";
    else
        inf.inversePhysicalTranslation = invert(inf.physicalTranslations, d);

    inf.voxelToPhysical = inf.physicalTranslations * inf.inverseTranslationalSymmetry;
    inf.physicalToVoxel = inf.translationalSymmetry * inf.inversePhysicalTranslation;

    // normalize the base Positions
    // normalize means that the base positions are within the area crated by
    // the vectors or the translation symmetry, because all other positions are
    // arrived at by adding integer scaled versions of those translation vectors
    // to the base positions
    for (size_t base = 0; base < inf.basePositions.size(); base++)
        normalizeBasePositions(
            inf.inverseTranslationalSymmetry, inf.translationalSymmetry, inf.physicalTranslations, inf.basePositions[base], inf.physicalShapes[base]);

    std::string error = calculateBaseLookup(inf.basePositions, inf.baseIndexCoefficients, inf.baseVectors);
    if (!error.empty()) return error;

    // check, if the given polyhedra are properly, e.g.
    // - every edge is used exactly twice with opposite direction (halfedge criterion)
    //   to make sure that the winding order is always right and there are no holes
    // - faces are plane and convex
    error = checkPhysicalShapes(inf.physicalShapes, inf.storageId);
    if (!error.empty()) return error;

    // calculate neighbour information
    // first face-neighbours
    error = calculateFaceNeighbourhood(
        inf.physicalShapes, inf.physicalTranslations, inf.translationalSymmetry, inf.basePositions, inf.storageId, inf.faceNeighbours);
    if (!error.empty()) return error;

    error = calculateEdgeNeighbourhood(
        inf.physicalShapes, inf.basePositions, inf.translationalSymmetry, inf.inversePhysicalTranslation, inf.faceNeighbours, inf.edgeLoops, inf.edgeIndices);
    if (!error.empty()) return error;

    error = calculateCornerNeighbourhood(
        inf.physicalShapes, inf.physicalTranslations, inf.basePositions, inf.translationalSymmetry, inf.cornerList, inf.cornerIndex);
    if (!error.empty()) return error;

    // calculate the mesh information
    error = calculateBevelOffsetVertices(inf.physicalShapes, inf.meshVertices, inf.meshFaceStartIndex);
    if (!error.empty()) return error;

    // check if the meshes are not too big, e.g. don't contain more vertices than supported by the index
    if (inf.meshVertices.size() > std::numeric_limits<decltype(Information::meshFaceStartIndex)::value_type::value_type>::max())
    {
        return "oops, more vertices than meshFaceStartIndex can index... please inform the BurrTools programmers";
    }

    error = prepareSnapPoints(inf.gridSnaps);
    if (!error.empty()) return error;

    std::lock_guard _(informationMutex_);

    for (auto & i : information_)
        if (i->storageId == inf.storageId)
        {
            // replace existing information with the new one
            i = std::make_shared<ExpensiveInformation>(std::move(inf));
            return {};
        }

    // add a new one

    information_.push_back(std::make_shared<ExpensiveInformation>(std::move(inf)));

    return {};
}

void Grid::removeGrid(Grid & g)
{
    std::lock_guard _(informationMutex_);

    std::erase_if(information_, [&g](auto & i) {
        if (g.grid_ == i)
        {
            deadInformation.push_back(i);
            return true;
        }
        return false;
    });
}

void Grid::ensureInformation(ExpensiveInformation & inf)
{
    // BurrTools: std::call_once instead of NG's unsynchronised "initialized" flag
    std::call_once(inf.initOnce, [&inf]
    {
        findGridSymmetries(inf.inverseTranslationalSymmetry,
                           inf.translationalSymmetry,
                           inf.physicalTranslations,
                           inf.inversePhysicalTranslation,
                           inf.physicalOffset,
                           inf.basePositions,
                           inf.physicalShapes,

                           inf.rotationalSymmetries,
                           inf.mirrorSymmetries,
                           inf.validTransformations,

                           inf.maxSymmetryGroup);

        // calculate movement vector stuff
        // must be behind the face calculation because we use it to create slightly offset
        // voxel polyhedra so that we get more stability in our results. Otherwise non rectilinear
        // faces and movement vectors might result in overlap while in reality pieces are sliding
        // along one another
        calculateMovementChecks(inf.movementVectors,
                                inf.translationalSymmetry,
                                inf.inverseTranslationalSymmetry,
                                inf.basePositions,
                                inf.physicalTranslations,
                                inf.physicalShapes,
                                inf.movementChecks,
                                inf.rotationalSymmetries);

        // calculate the movement extend
        // theoretically we could exclude the movement vector itself from this
        // this might save a few checks, but I am not sure, this needs further thought
        // value is initialized with 0 so we just go on working with that value
        for (auto & b : inf.movementChecks)
            for (auto & d : b)
                for (auto & c : d)
                {
                    inf.movementChecksExtend = std::max<int8_t>(inf.movementChecksExtend, c.x());
                    inf.movementChecksExtend = std::max<int8_t>(inf.movementChecksExtend, -c.x());
                    inf.movementChecksExtend = std::max<int8_t>(inf.movementChecksExtend, c.y());
                    inf.movementChecksExtend = std::max<int8_t>(inf.movementChecksExtend, -c.y());
                    inf.movementChecksExtend = std::max<int8_t>(inf.movementChecksExtend, c.z());
                    inf.movementChecksExtend = std::max<int8_t>(inf.movementChecksExtend, -c.z());
                }
    });
}

static bool pointInsidePolyhedron(const Vector<float> & p, const Grid::Polyhedron & polyhedron, const float epsilon)
{
    // check for each face of the polyhedron, whether p is above that plane of the face, if so it can not be inside the polyhedron
    // ATTENTION only for convex polyhedra
    for (size_t face = 0; face < polyhedron.faces.size(); face++)
        if (const std::optional<Plane<float>> plane = calcPlane(polyhedron, face); !plane || (pointPlaneDistance(p, *plane) > epsilon)) return false;

    return true;
}

std::optional<Vector<int>> Grid::findPhysicalVoxel(Vector<float> p, float epsilon) const
{
    // ATTENTION for convex polyhedra only
    // if we ever go to non-convex voxel shapes we need to adapt here

    const auto gp  = getInformation();
    const auto & g = *gp;

    const Vector<float> base = g.inversePhysicalTranslation * p;
    const auto intBase       = Vector<int>(std::floor(base.x()), std::floor(base.y()), std::floor(base.z()));

    // shift p to position inside base
    p -= g.physicalTranslations * intBase;

    // p should be within the base epiped at the origin now, so we
    // need to check all shapes in that base, but because shapes may
    // be overlapping we also check the 26 surrounding ones after that

    for (size_t bs = 0; bs < g.physicalShapes.size(); bs++)
    {
        if (pointInsidePolyhedron(p, g.physicalShapes[bs], epsilon))
        {
            return g.translationalSymmetry * intBase + g.basePositions[bs];
        }
    }

    // ok, it seems like we are not inside the base epiped... try again with the
    // surrounding ones

    for (auto & diff : BurrTools::Maths::iterate3d<Vector<int>>(1))
        if (!diff.isZero())
        {
            const Vector<float> physicalDiff = g.physicalTranslations * diff;

            for (size_t bs = 0; bs < g.physicalShapes.size(); bs++)
            {
                if (pointInsidePolyhedron(p - physicalDiff, g.physicalShapes[bs], epsilon))
                {
                    return g.translationalSymmetry * (intBase + diff) + g.basePositions[bs];
                }
            }
        }

    // hmm... when we reached here it seems like there
    // is no voxel at a given position, which can
    // not happen with the current grids... so something is wrong
    // but we still have a proper return value because maybe in the future
    // we will have grids with empty spaces between the voxels
    printf("no voxel at %f %f %f for grid %zu\n", p.x(), p.y(), p.z(), grid_->storageId.value);
    assert(0);
    return std::nullopt;
}

static Vector<float> intersectPlaneLine(const Plane<float> & plane, const Grid::Edge & e)
{
    const Vector<float> v0 = e.start;
    const Vector<float> vv = e.end - e.start;

    // find t such  (v0 + t*vv) * plane.normal - plane.dist = 0
    // ->  v0 . plane.normal + t*(vv . plane.normal) - plane.dist = 0
    // -> t = (plane.dist - v0 . plane.normal) / vv . plane.normal

    const float t = (plane.distance - v0 * plane.normal) / (vv * plane.normal);

    return v0 + vv * t;
}

static bool intersectPlanePolygon(const Plane<float> & plane, const Grid::Polyhedron & p, Vector<float> poffs, size_t face, Grid::Edge & outEdge, float epsilon)
{
    // calculate all the distances of the points to the plane
    std::vector<float> distances(p.faces[face].size());
    for (size_t e = 0; e < p.faces[face].size(); e++) distances[e] = pointPlaneDistance(p.vertices[p.faces[face][e]] + poffs, plane);

    bool first = true;

    for (size_t e1 = 0; e1 < p.faces[face].size(); e1++)
    {
        const size_t e2 = (e1 + 1) % p.faces[face].size();

        if ((distances[e1] < epsilon) != (distances[e2] < epsilon))
        {
            const auto v1 = p.vertices[p.faces[face][e1]] + poffs;
            const auto v2 = p.vertices[p.faces[face][e2]] + poffs;

            // simple case, the 2 points are on different sides of the plane, we found one intersection

            if (first)
            {
                outEdge.start = intersectPlaneLine(plane, Grid::Edge{v1, v2});
                first         = false;
            }
            else
            {
                outEdge.end = intersectPlaneLine(plane, Grid::Edge{v1, v2});
                return true;
            }
        }
    }

    return false;
}

std::vector<std::pair<Grid::Edge, Vector<int>>> Grid::calculatePolygonIntersection(const Vector<int> & pos, const Plane<float> & plane, float epsilon) const
{
    const auto gp  = getInformation();
    const auto & g = *gp;

    const RMatrix<int> & invTrans = g.inverseTranslationalSymmetry;
    const Matrix<int> & forTrans  = g.translationalSymmetry;

    const RVector<int> a = invTrans * pos;
    const Vector<int> b  = a.getInteger();
    const Vector<int> c  = forTrans * b;
    const Vector<int> d  = pos - c;

    std::vector<std::pair<Grid::Edge, Vector<int>>> res;

    // this really should not happen... or the given position is invalid
    if (const int bp = getBase(d); bp >= 0)
    {
        const Vector<float> p = g.physicalTranslations * b;

        // we could safe us some work here by following the edges to the next face instead of
        // checking all faces... but what the heck this is much simpler for now TODO for later
        //
        // also don't add very short edges..
        for (size_t face = 0; face < g.physicalShapes[bp].faces.size(); face++)
        {
            // get the edge
            Grid::Edge e;
            if (intersectPlanePolygon(plane, g.physicalShapes[bp], p, face, e, epsilon))
                if (!equal(e.start, e.end, 0.00001f)) res.emplace_back(e, pos + g.faceNeighbours[bp][face].diff);
        }
    }

    return res;
}

std::tuple<size_t, size_t> Grid::gridStatistics()
{
    std::erase_if(deadInformation, [](auto & p) { return p.expired(); });
    return std::make_tuple(information_.size(), deadInformation.size());
}

} // namespace BurrTools::PuzzleLibrary
