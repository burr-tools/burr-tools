/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/lib/grid.h at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G2). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: StorageId is a plain struct in place of pssst::strong.
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#pragma once

#include "transformation.h"

#include "algebra.h"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace BurrTools::PuzzleLibrary {

// This class encapsulates all information required to do operations on the
// different grids that BurrTools supports. This is one of the more complex
// parts, so let's see if I can make clear what is going on.
// First for some definitions:
// - grid: is an arrangement of objects in space. The methods implemented
//   in here don't allow for all possible grids, but only ones that have the
//   following properties:
//   - periodic in all 3 dimensions (otherwise you'd need an infinite description of where
//     objects are) this excludes grids that are only rotationally symmetric (e.g. Penrose patterns)
//   - finite list of symmetries (this excludes grids of spheres)
//   - a subsets of the grid must not have a higher symmetry than the grid itself. Example
//     the truncated square tiling works, even though the octagons have a higher symmetry than
//     the grid, but if you split the octagons into 8 identical triangles (like a cake), the single
//     triangles will never be rotated by 45° and that will prevent finding certain solutions
// - the objects that are placed with the grid are called voxels (volume elements)
//   - a limitation coming from the implementation is that there should be no
//     gaps between the voxels and all voxel faces as well as the complete voxel is convex
// - representation: is the way that the grid is represented using integer only coordinates
//   - each voxel needs to have exactly one integer coordinate (it is not possible that
//     a group of coordinates represents one voxel
//   - not all possible integer coordinated need to be used for voxels, there can be
//     empty spaces
// - you need to specify the translational symmetry by giving 3 vectors (in a column matrix)
//   that shift the whole grid onto itself. The vectors are relative to the representation,
//   so from the integer coordinates of one voxel to the same voxel at a different place on
//   the grid
//   - these 3 vectors span/create a parallelepiped with one corner at the origin. This
//     epiped is called "base-epiped". Other epipeds that are the neighbors are not the base
//   - all voxels within the base are "different", e.g. even if physically identical they are
//     never interchangeable by BurrTools, so you need to make sure that that is right
//     for your grid. Physically identical here means translation only identical. If one is
//     a rotation of another it might work
//   - the epiped is usually taken as inclusive for the lower 3 faces and exclusive for
//     the top 3, e.g a <= x < b
// - base positions are all representation coordinates within the base-epiped that represent
//   valid voxel coordinates. BurrTools will always leave all other coordinates empty. In fact,
//   it will assume that the other coordinates are empty
//
// Well that leaves us with 3 different coordinate systems that we have to differentiate
// I explain them on the example of the triangle grid because in that grid they are all
// different... in the cube grid for example all 3 are identical
// 1) physical coordinate system: floating point coordinates that represent the position
// of the voxel in real physical space as it is supposed to look without any distortions.
// 2) voxel coordinate system: integer coordinates where each voxel has a unique integer
// position. There can be empty spaces, e.g. some coordinates do not represent a voxel and
// should be empty and not used (e.g. the coordinate (0;0;0) is invalid for the triangle grid)
// The important thing to consider is that these coordinates must have the same symmetries
// as the physical grid, e.g. if there is a rotational symmetry of 60 degree around a certain
// voxel, the same 60° symmetry must exist in the voxel coordinate system. (for the triangle
// grid this is achieved by placing the triangle plane in the plane with normal (1;1;1) inside
// the voxel coordinate system. The voxel coordinate system may contain more symmetries than
// there are in the physical one.
// 3) grid coordinate system: rational coordinates for voxels. Each 1x1x1 cube inside this
// grid represents one repeating block of the grid (skewed and rotated, but one block). Inside
// this one block you must have 1 instance of each orientation of each voxel. The instance
// might be chopped up and translated to other places in the block though.
// For the triangle grid this block contains 2 triangles, one for each rotation. In the physical
// grid that block has a shape of a "parallelogram prism".
// The other two coordinate systems are defined relative to this system, e.g. as a transformation
// matrix going from this coordinate system to the other
class Grid
{
    public:
        // definition of one polyhedron for the physical definition
        // the polyhedron has to be convex
        // it is a vector of vertices,
        // and a vector of faces, each face is a vector of indices
        // into the vertices vector
        struct Polyhedron
        {
                std::vector<BurrTools::Maths::Vector<float>> vertices;
                std::vector<std::vector<size_t>> faces;
                // calculated on startup to contain a point close to the center
                // of the polyhedron (whatever that may mean)
                // - point is deeply inside
                BurrTools::Maths::Vector<float> center = BurrTools::Maths::Vector<float>{0, 0, 0};
        };

        // defines a rotational symmetry, e.g. an axis
        // and a TRotation that represents the rotation (including the angle)
        // (the angle is right now not required and only implicitly given in the transformation
        struct RotationalSymmetry
        {
                BurrTools::Maths::RVector<int> p0;       // start of the axis
                BurrTools::Maths::Vector<int> direction; // and direction of axis
                TRotation transformation;                // the transformation corresponding to the symmetry
        };

        // defines a mirror symmetry, e.g. a plane and the corresponding transformation
        struct MirrorSymmetry
        {
                BurrTools::Maths::Plane<int> plane; // the mirror plane
                TRotation transformation;           // the transformation corresponding to the symmetry
        };

        // defines neighborhood information
        struct FaceNeighbour
        {
                BurrTools::Maths::Vector<int8_t> diff; // where is the neighbor voxel in voxel coordinates relative to the current one
                uint8_t face = 0;                      // which face of the neighbor voxel is it touching
        };

        // information to create the 3d-meshes for the grids.
        // the information is in accordance with the physical definition
        // of the voxel shape
        // also the neighbor information fits, ...
        struct MeshVertex
        {
                BurrTools::Maths::Vector<float> position;   // 3d position
                BurrTools::Maths::Vector<float> normal;     // normal of the face (a bit redundant for each vertex)
                BurrTools::Maths::Vector<float> bevel_prev; // where to move the position to apply the bevel, when the edge to the previous vertex is beveled
                BurrTools::Maths::Vector<float> bevel_next; // where to move the position to apply the bevel, when the edge to the next vertex is beveled
                BurrTools::Maths::Vector<float> offset;     // where to move the position to apply the offset
        };

        // An "edge-loop" is the ring of voxels that surround one edge, the following
        // structures are used to handle and define those loops

        // information to index into the edge loop
        struct EdgeLoopIndex
        {
                uint8_t edge;  // which edge -> first index in edgeLoops
                uint8_t index; // which entry inside the loop

                // because there are always 2 faces for each polyhedron sharing the same edge, and the look
                // in opposite directions into the loop, the direction field gives a hint to the direction
                // you need to traverse the loop
                int8_t direction;
        };

        // index to one specific edge in a grid
        struct EdgeIndex
        {
                uint8_t base = 0;
                uint8_t face = 0;
                uint8_t edge = 0;
        };

        // this structure contains information about one voxel on an edge
        struct EdgeLoopEntry
        {
                // the position of current voxel in an edge loop
                // the positions are all relative to one another for this loop
                BurrTools::Maths::Vector<int8_t> position;

                // the information of the 2 faces that this voxel had with this
                // edge which one you have to use is taken from the direction
                // entry in the EdgeLoopIndex, use that value as the index
                // into the array
                std::array<EdgeIndex, 2> edges;
        };

        // information to index into the corner list
        struct CornerListIndex
        {
                uint8_t corner;
                uint8_t index;
        };

        // Helper structure representing an edge given by a start end an end point in physical space
        struct Edge
        {
                BurrTools::Maths::Vector<float> start;
                BurrTools::Maths::Vector<float> end;
        };

        // used for the GridSnapPoints to define what possible up
        // directions might exist
        struct UpDefinition
        {
                // one possible up direction
                BurrTools::Maths::Vector<float> direction;

                // how many equally distributed stops are there
                // in the 360-degree angle, so 3 means 3 ups that
                // are 120 degree apart from one another
                uint8_t symmetry = 0;
        };

        // defines snap points for the grid editor, all values are in
        // physical coordinates
        struct GridSnapPoint
        {
                // normal vector of the cutting plane, the grid will align such
                // that this normal is pointing out of the display plane
                // it is not necessary that this vector is normalized, that will
                // be done on start-up
                BurrTools::Maths::Vector<float> normal;
                // the plane can then move along the normal and snap at certain positions
                // along that direction all these distance are defined in this vector
                // the last entry is the point where the whole pattern repeats again, that
                // means the last point is like zero
                std::vector<float> stops;
                // the view can then rotate around the display normal, but it will snap
                // at certain angles that are defined in here, these vectors give a sense
                // of what is "up". The vector doesn't need to be perpendicular to the
                // normal, or unit distance. This will be rectified at startup. But all
                // vectors in here need to be at an angle with the normal
                std::vector<UpDefinition> ups;

                // this vector contains all the quaternions resulting out of this
                // normal and up definition
                // the rotation define here is the rotation to get the
                // given normal identical to the +z vector and the up vector
                // pointing in negative y direction
                std::vector<BurrTools::Maths::Quaternion<float>> orientations;

                // Limitations: you can not have a different set of up directions for
                // different stops along the normal
        };

        // just a tag class for an int to more precisely define what kind
        // of number we have here
        // (NG: pssst::strong<size_t, StorageId, pssst::Eq>; the base keeps
        // NG's aggregate initialisation, StorageId{{{0}}}, working)
        struct StorageIdValue
        {
                size_t value;
        };
        struct StorageId : StorageIdValue
        {
                friend bool operator==(const StorageId & a, const StorageId & b) noexcept { return a.value == b.value; }
        };

        // information for one grid,
        // that information is distributed into 3 classes each derives from the former to
        // add additional members.
        // The first class contains only fields that are necessary to be filled in when a new
        // grid is registered
        // some members also contain defaults. When the corresponding feature is not needed
        // those default can be kept
        // the 2nd class add fields that are calculated when the grid is registered these
        // calculations are supposed to be fast enough that they can be done on start-up
        // of BurrTools
        // The final class contains some members that are expensive to calculate and that would
        // unnecessarily slow down the start-up of BurrTools, so they are calculated on demand
        struct RegisterInformation
        {
                // a unique integer for the grid, these numbers will never ever change
                // so that they are usable for external storage
                StorageId storageId;

                // Short name of the grid
                std::string name;

                // translational symmetry is a list of vectors in the voxel coordinate system (ATTENTION: in column order)
                // that define the base parallelepiped
                // the vectors need to be independent (e.g. no vector can be recreated by adding integer
                // multiples of the other vectors). There will always be 3 of those vectors because we are in 3d-space
                // the 3 vectors are assembled into a matrix, multiplying a vector with this matrix will get you
                // from the grid coordinate to the voxel coordinate system
                // e.g. to find out where in the 3d array the 2nd hexagon in the 3rd row, in layer 5 is you multiply
                // [2, 3, 5] by this matrix
                // you can also interpret this as a matrix that transforms from the grid coordinate to the voxel coordinate system
                BurrTools::Maths::Matrix<int> translationalSymmetry;

                // here you define the physical, e.g. in real 3d coordinates as it will be shown
                // vectors corresponding to those in translationalSymmetry, also column vectors
                // multiplying a vector with this matrix will get you from grid coordinate system
                // to the physical coordinate system
                // ATTENTION: be careful that the vectors fit with the vectors in the translationalSymmetry matrix
                BurrTools::Maths::Matrix<float> physicalTranslations;

                // this offset defines how much the physical representation is offset relative to the
                // voxel representation, e.g. suppose a cube grid, the voxel at cube position 0 goes from -0.5...0.5
                // but the physical representation ends at 0.0. Then you need the offset
                // This value is taken in voxel-coordinates.
                BurrTools::Maths::Vector<float> physicalOffset = {0.0f, 0.0f, 0.0f};

                // for movement analysis we need to know
                // all possible vectors that a piece can move, these vectors
                // are in the voxel coordinate system,
                // it is assumed that the negative direction also is a movement vector
                // so if [1, 0, 0] is a movement vector, then also [-1, 0, 0] is
                // you only need to include one
                std::vector<BurrTools::Maths::Vector<int>> movementVectors;

                // this defines the shapes of all voxels, it must have exactly as many entries as basePositions
                // the polyhedra will be shifted accordingly if the basePositions are changed when normalizing
                // from this we get all kinds of information:
                // - neighborhood information: face touching, edge touching, corner touching
                // - bevel and offset drawing information
                // - lookups for motion calculations
                std::vector<Polyhedron> physicalShapes;

                // all positions within the parallelepiped that are valid in this grid.
                // in voxel coordinates
                // the initialization table doesn't need to have all positions within the base-epiped
                // on initialization they will be normalized to be in there
                // all valid coordinates are the sum of one of these vectors plus the
                // translational symmetry matrix multiplied by an integer vector
                std::vector<BurrTools::Maths::Vector<int>> basePositions;

                // snap points for the grid editor, this is only for the GUI and not important
                // for the proper working of the library...
                std::vector<GridSnapPoint> gridSnaps;
        };

        // this structure extends the information for a grid that is given when registering
        // the grid. All the information in here is relatively quickly calculated so that its
        // calculation is performed right when the grid is registered
        struct Information : public RegisterInformation
        {
                // inversion of translationalSymmetry matrix
                // a 3 by 3 matrix plus a denominator valid for all matrix entries
                // multiplying a 3d vector by this matrix will get you from the position in the 3d array to the grid position
                BurrTools::Maths::RMatrix<int> inverseTranslationalSymmetry;
                // calculated at startup to get from physical positions to grid positions
                BurrTools::Maths::Matrix<float> inversePhysicalTranslation;

                // matrices that go directly from voxel to physical and also the inverse
                BurrTools::Maths::Matrix<float> voxelToPhysical;
                BurrTools::Maths::Matrix<float> physicalToVoxel;

                // to find out to what base-vector a grid coordinate corresponds we first
                // multiply by inverseTranslationalSymmetry, get the remainder and then multiply
                // by translationalSymmetry matrix, the resulting coordinate should be one
                // of the base vectors. to quickly find out which one we use the following 2
                // members
                //
                // we first calculate a number that is unique for each possible vector in the
                // base-epiped. This is calculated similar to the index in a 3d array (z*depth+y)*width+x
                // just that we need an additional offset to get the final result back to the range 0...max, because
                // the base-vectors might not be in the area 0...x but include negative numbers or not include the 0
                // with the resulting index we look into baseVectors to find out the base vector,
                // the array contains -1 for all positions that are not a base, or number >= 0 for a valid base
                //
                // look at getBase below for usage
                std::array<int, 3> baseIndexCoefficients = {};
                std::vector<int8_t> baseVectors;

                // faces to draw the voxels, each face is a convex polygon, if we
                // later on need to support concave ones, we need to split them up
                //
                // the mesh Vertices contain the vertex information, position, offset vector, bevel vector, normal vector
                // meshFaceStartIndex contains the first index of each face in meshVertices. then follow
                // as many vertices as there are corners in the face
                std::vector<MeshVertex> meshVertices;
                std::vector<std::vector<uint16_t>> meshFaceStartIndex;

                // face neighborhood information
                // for each base we have one vector with an array for each face
                // the array contains 4 values: dx, dy, dz, face
                // if you leave the base shape via the given face you end up at the
                // voxel at the relative position from your starting point and at the given face
                std::vector<std::vector<FaceNeighbour>> faceNeighbours;

                // edge information, for each base, for each face for each edge we want to know the
                // other members that share that edge with some additional information
                //
                // this is done by having a vector with an "edge loop" which contains all members
                // of edges with their data. Into these loops we index using a 2nd vector
                std::vector<std::vector<EdgeLoopEntry>> edgeLoops;
                // the 3 elements in the array are index 1 for edgeLoops, index 2 for edgeLoops,
                // direction to take in the loop (either 1 or -1)
                std::vector<std::vector<std::vector<EdgeLoopIndex>>> edgeIndices;

                // with these 2 elements it is possible to find all the voxels that share
                // a certain corner
                // the index points you to the corner it is indexed via base, face, corner.
                // You get the corner id and an index
                // the id is used to index into cornerList you get a std::vector of positions
                // your position can be found at the "index" position. all the other
                // entries of the list are the positions of the other voxels
                // the positions are voxel positions
                std::vector<std::vector<BurrTools::Maths::Vector<int8_t>>> cornerList;
                std::vector<std::vector<std::vector<CornerListIndex>>> cornerIndex;

                explicit Information(RegisterInformation && i) : RegisterInformation(std::move(i)) {}
        };

        // This is a 2nd extension of the grid information with information that is harder to calculate
        // and only calculated on demand
        struct ExpensiveInformation : public Information
        {
                // calculates the information below exactly once (ensureInformation); BurrTools: replaces
                // NG's plain bool, which raced when two threads asked for the same grid. Fine as a
                // member because ExpensiveInformation only lives in a shared_ptr and is never copied
                std::once_flag initOnce;

                // this vector contains a list of all rotation axes of the grid that go through the base-epiped.
                // the initial definitions in this vector will be replaced by a normalized and extended list
                // of axis. That means for the initial list that it need to contain each axis exactly once, but
                // - the axis doesn't need to go through the base epiped, but can be defined arbitrarily
                //   the normalization will move it into the epiped
                // - if the same axis intersects the epiped multiple times due to grid translation symmetry
                //   only one instance of the axis will be provided
                std::vector<RotationalSymmetry> rotationalSymmetries;

                // a list of all mirror planes of the grid (within the base area)
                // for each plane, we specify the normal vector (doesn't need to be normalized)
                // and the offset value, so all points for which s[0]*x+s[1]*y+s[2]*z+d == 0
                // are on the plane
                // the final value contains the corresponding transformation matrix
                // and is calculated at startup
                std::vector<MirrorSymmetry> mirrorSymmetries;

                // all coordinates to check for one step of each movement directions
                // for each base, for each direction a list of coordinates sets relative to the start
                // that need to be empty to move the voxel along the movement vector
                std::vector<std::vector<std::vector<BurrTools::Maths::Vector<int8_t>>>> movementChecks;

                // this vector contains the maximum value for each direction that movement
                // checks may differ from the current position
                // this information is useful to see if you can still intersect a shape
                // when doing movement checks from a certain position
                // once you are this far away from the bounding box or so there is no
                // risk that any of the checks will intersect
                // this is a bit redundant with translational symmetry vector... but
                // we don't have complete control over that vector
                int8_t movementChecksExtend = 0;

                // the group of all possible rotational symmetries possible
                // this is one of the GRP defines from RotationalSymmetryGroup
                uint8_t maxSymmetryGroup = 0;

                // this array contains the possible "remainders" of the translation parts of a transformation
                // with all possible given rotations. This is required to verify that a given transformation
                // is valid for a given grid (e.g. the ROT_060_XYZ rotation is not valid for cube grid or
                // the translation by 1 in x direction is not valid for triangular mesh)
                // to check a transformation you first take the translational part
                // and calculate the remainder using getEpipedAndRemainderForVoxelPosition from below
                // then you take the rotational part from the transformation, take its value use it as an
                // index into the array. the remainder you got needs to be inside the std::vector of Vectors
                std::array<std::vector<BurrTools::Maths::RVector<int16_t>>, TRotation::NUM_ROTATION_MATRICES> validTransformations = {};

                explicit ExpensiveInformation(Information && i) : Information(std::move(i)) {}
        };

    private:
        // index into the information vector
        // that is the reason why we can not remove a grid, only replace it
        std::shared_ptr<ExpensiveInformation> grid_;

        static std::vector<std::shared_ptr<ExpensiveInformation>> information_;
        // mutex for information_ vector
        static std::mutex informationMutex_;

        // make sure the expensive part of the information is filled in;
        // thread safe (std::call_once), call it without holding informationMutex_
        static void ensureInformation(ExpensiveInformation & info);

    public:
        // registers a new grid or updates an existing grid
        // the storageId will be the key, if it has the same
        // storage id as an already registered grid, that grid
        // will be updated
        // the function will return an empty string, when everything
        // was fine, else you get an error text
        static std::string registerGrid(RegisterInformation && i);
        static void removeGrid(Grid & g);

        explicit Grid(const StorageId t = StorageId{{{0}}})
        {
            const std::lock_guard _(informationMutex_);
            for (const auto & inf : information_)
                if (inf->storageId == t)
                {
                    grid_ = inf;
                    return;
                }

            throw std::runtime_error("requested grid unknown");
        }

        static std::shared_ptr<const Information> getInformation(const size_t i) noexcept
        {
            const std::lock_guard _(informationMutex_);
            if (i < information_.size()) return information_[i];

            return nullptr;
        }

        static std::shared_ptr<const ExpensiveInformation> getExpensiveInformation(const size_t i) noexcept
        {
            std::shared_ptr<ExpensiveInformation> s;

            {
                const std::lock_guard _(informationMutex_);
                if (i < information_.size()) s = information_[i];
            }

            if (s) ensureInformation(*s);

            return s;
        }

        // return the expensive information for a given "normal" information handle
        // ATTENTION, when the grid is no longer known, a null-pointer will be returned
        static std::shared_ptr<const ExpensiveInformation> getExpensiveInformation(const std::shared_ptr<const Information> & info) noexcept
        {
            std::shared_ptr<ExpensiveInformation> s;

            {
                // only lock the mutex while needed, and call ensureInformation
                // after the mutex has been freed
                const std::lock_guard _(informationMutex_);

                for (auto & ii : information_)
                    if (ii == info)
                    {
                        s = ii;
                        break;
                    }
            }

            if (s) ensureInformation(*s);

            return s;
        }

        [[nodiscard]] std::shared_ptr<const ExpensiveInformation> getExpensiveInformation() const noexcept
        {
            ensureInformation(*grid_);
            return grid_;
        }

        [[nodiscard]] std::shared_ptr<const Information> getInformation() const noexcept { return grid_; }

        static std::tuple<size_t, size_t> gridStatistics();

        // get the epiped vector (grid coordinate) and the remainder (e.g. position inside the epiped) for
        // a given voxel position
        template <class V>
        std::pair<BurrTools::Maths::Vector<int>, V> getEpipedAndRemainderForVoxelPosition(const V & pos) const noexcept
        {
            BurrTools::Maths::RVector<int> a = grid_->inverseTranslationalSymmetry * pos;
            BurrTools::Maths::Vector<int> b  = a.getInteger();
            BurrTools::Maths::Vector<int> c  = grid_->translationalSymmetry * b;
            V d                              = pos - c;

            return std::make_pair(b, d);
        }

        // check if a given voxel translation is valid for the grid
        template <class V>
        bool isValidTranslation(const V & t) const noexcept
        {
            BurrTools::Maths::RVector z = grid_->inverseTranslationalSymmetry * t;

            return z.getFraction().isZero();
        }

        // check if a given Transformation is valid for the grid
        template <class D>
        bool isValidTransformation(const TMatrix<D> & t) const noexcept
        {
            if (!t.rotation()) return false;

            ensureInformation(*grid_);

            auto [epiped, remainder] = getEpipedAndRemainderForVoxelPosition(t.translation());

            for (auto & v : grid_->validTransformations[t.rotation().value()])
                if (v == remainder) return true;

            return false;
        }

        // get the base-number for a remainder, returns -1 if an invalid base
        template <class T>
        int getBase(const BurrTools::Maths::Vector<T> & t) const noexcept
        {
            const auto & bi = grid_->baseIndexCoefficients;
            const auto & bv = grid_->baseVectors;

            int i = (t.z() * bi[0] + t.y()) * bi[1] + t.x() + bi[2];

            if (i < 0) return -1;
            if (static_cast<size_t>(i) >= bv.size()) return -1;

            return bv[i];
        }

        // this returns the base-number position and the grid coordinates for voxel coordinates
        template <class T>
        std::pair<int, BurrTools::Maths::Vector<int>> getBaseForVoxelPosition(const BurrTools::Maths::Vector<T> & pos) const noexcept
        {
            auto [epiped, remainder] = getEpipedAndRemainderForVoxelPosition(pos);
            return std::pair(getBase(remainder), epiped);
        }

        bool operator==(const Grid & rhs) const noexcept { return grid_ == rhs.grid_; }

        bool operator!=(const Grid & rhs) const noexcept { return grid_ != rhs.grid_; }

        // find a voxel that contains the p, or whose p is maximally epsilon
        // outside of the walls
        // Attention SLOW function
        // returns the voxel position
        [[nodiscard]] std::optional<BurrTools::Maths::Vector<int>> findPhysicalVoxel(BurrTools::Maths::Vector<float> p, float epsilon) const;

        // calculate the polygon that you get, when you intersect the given plane with the physical
        // shape of the voxel at the given voxel position
        // the return value is a list of all edges of the polygon including the
        // face id of that edge
        // ATTENTION those edges are not sorted in any way, not even corner to corner
        [[nodiscard]] std::vector<std::pair<Grid::Edge, BurrTools::Maths::Vector<int>>>
        calculatePolygonIntersection(const BurrTools::Maths::Vector<int> & pos, const BurrTools::Maths::Plane<float> & plane, float epsilon) const;
};

// helper function to create a polyhedron of a prism out of a base face and the up direction
// this is used to initialize the physical representations of the voxels inside the grid
Grid::Polyhedron prism(const std::vector<BurrTools::Maths::Vector<float>> & face, const BurrTools::Maths::Vector<float> & normal);

// helper function to calculate the bevel and offset values for a given physical shape
// this is used in the grid initialization but also to create the custom stretched grids
// for the STL output
std::string calculateBevelOffsetVertices(const std::vector<Grid::Polyhedron> & physicalShapes,
                                         std::vector<Grid::MeshVertex> & meshVertices,
                                         std::vector<std::vector<uint16_t>> & meshFaceStartIndex);

} // namespace BurrTools::PuzzleLibrary
