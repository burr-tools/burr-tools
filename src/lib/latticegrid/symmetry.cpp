/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/lib/symmetry.cpp at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G1). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: std::print replaced by fprintf, exit by abort; calculateRotationalSubset, which needs the grid, left for step G2.
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#include "symmetry.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <tuple>

using BurrTools::Maths::RVector;

namespace BurrTools::PuzzleLibrary {

std::array<TRotationSet, RotationalSymmetryGroup::GRP_COUNT + 1> RotationalSymmetryGroup::GroupSymmetries;
std::array<uint16_t, RotationalSymmetryGroup::GRP_COUNT * 2 - 1> RotationalSymmetryGroup::DecisionTree;
std::array<TRotation, (RotationalSymmetryGroup::GRP_COUNT + 1) * TRotation::NUM_ROTATION_MATRICES> RotationalSymmetryGroup::Normalized;
std::array<TRotation, (RotationalSymmetryGroup::GRP_COUNT + 1) * TRotation::NUM_ROTATION_MATRICES> RotationalSymmetryGroup::Normalizer;
std::array<TRotationSet, (RotationalSymmetryGroup::GRP_COUNT + 1) * (RotationalSymmetryGroup::GRP_COUNT + 1)> RotationalSymmetryGroup::Uniques;
std::array<uint8_t, (RotationalSymmetryGroup::GRP_COUNT + 1) * TRotation::NUM_ROTATION_MATRICES> RotationalSymmetryGroup::UnifySymmetryTransformation;
std::array<uint8_t, (RotationalSymmetryGroup::GRP_COUNT + 1) * (RotationalSymmetryGroup::GRP_COUNT + 1)> RotationalSymmetryGroup::UnifySymmetrySymmetry;
std::array<uint8_t, (RotationalSymmetryGroup::GRP_COUNT + 1) * (RotationalSymmetryGroup::GRP_COUNT + 1)> RotationalSymmetryGroup::IntersectSymmetrySymmetry;

// this function recursively creates the check tree
// this is done by splitting the remaining symmetry groups that are left in the
// current sub-tree into 2 halves that are as similar sized as possible
// and recursively call until only one group is left
//
// GroupSymmetries: complete list of all groups
// fixed and value: a bitmask for which rotations are already fixed within this sub-tree and whether the rotation is
// included in the symmetry group or not
// output: the output, where to put the tree, outpos, at which position to start outputting, the tree
// will always put the root at outpos, followed by the two sub-trees
//
// return is the next free position in the output array
static size_t createSymmetryCheckTree(const auto & GroupSymmetries, TRotationSet fixed, TRotationSet value, auto & output, size_t outpos)
{
    // step one: go through all symmetry groups that need categorisation within this sub-tree
    // try all possible rotation matrices and count how that would split the remaining
    // groups into 2 sets
    //
    // how many groups are zero with each possible split rotation
    std::array<int, TRotation::NUM_ROTATION_MATRICES> zeros = {0};
    int num                                                 = 0; // total number of split rotations
    size_t sym                                              = 0; // one symmetry group within the tree

    // iterate over all groups
    for (size_t s = 0; s < GroupSymmetries.size() - 1; s++)
    {
        // when they are within the current sub-tree
        if ((GroupSymmetries[s] & fixed) == value)
        {
            // count the groups and save one for later on
            num++;
            sym = s;

            // and then check for all rotations, whether the group
            // requires the object to be symmetrical along it or not
            for (unsigned int r = 0; r < TRotation::NUM_ROTATION_MATRICES; r++)
                if (!fixed.contains(TRotation(r)) && !GroupSymmetries[s].contains(TRotation(r))) zeros[r]++;
        }
    }

    if (num == 0)
    {
        // something went wrong... this must not happen
        fprintf(stderr, "oops there seems to be a bug in createSymmetryCheckTree\n");
        std::abort();
    }
    else if (num == 1)
    {
        // exactly one group left output that and return, we are done here
        output[outpos] = sym | 0x8000;

        return outpos + 1;
    }
    else
    {
        // ok, more than one group left, find a good transformation
        // to check that splits the remaining transformations as evenly as possible
        for (auto & z : zeros)
        {
            // after this the value is closer to zero, if the count was
            // close to 1/2 of the total number
            z = abs(2 * z - num);
        }

        // find a minimum... yes this is a greedy implementation, but I tried other ways
        // that take much more time and the end result is pretty close to this.
        auto best = std::ranges::min_element(zeros);

        // store the rotation that we found
        auto rotation = std::distance(zeros.begin(), best);

        // recursively do the first sub-tree, leaving one empty field that we fill in with the decision node
        // store the start of the 2nd tree
        TRotationSet fixed2 = fixed;
        TRotationSet value2 = value;
        fixed2.add(TRotation(rotation));
        value2.add(TRotation(rotation));
        int newend = createSymmetryCheckTree(GroupSymmetries, fixed2, value2, output, outpos + 1);

        // store the decision node, which rotation to check and where the 2nd sub-tree starts, in case
        // we need to jump there
        output[outpos] = (newend << 6) | rotation;

        // recursively do the 2nd sub-tree, starting at the proper place
        newend = createSymmetryCheckTree(GroupSymmetries, fixed2, value, output, newend);

        return newend;
    }
}

void RotationalSymmetryGroup::init_tables()
{
    // the function fills in the lookups above
    //
    // First lets see about the Groups array.
    // This array contains all subsets of rotations from the TRotation class
    // that form groups. (see https://en.wikipedia.org/wiki/Group_(mathematics)
    // for information of what a group is.)
    // For that we start with a group and add a single additional
    // transformation and then add the other transformations that are required
    // to get it closed again.
    // We might find out that this is impossible. Then we throw away that list

    // we always start with 1 group, one with the identity rotation in there
    // That is the simplest group there is
    size_t numGroups = 0;
    GroupSymmetries[numGroups++].add(TRotation(TRotation::ROT_IDENTITY));

    // we check all found groups for possible new additions, in the Groups vector
    // we have the checked ones in the beginning followed by groups found but not
    // checked, yet.
    size_t checked = 0;

    // while there are still groups left to check
    while (checked < numGroups)
    {
        // go over all possible rotations
        for (unsigned int r = 0; r < TRotation::NUM_ROTATION_MATRICES; r++)
        {
            // check, if the rotation not yet included in the group
            if (!GroupSymmetries[checked].contains(TRotation(r)))
            {
                // create a new group and add that rotation
                auto g = GroupSymmetries[checked];
                g.add(TRotation(r));

                // now we need to recombine all included rotations until none other can be added
                bool changed;
                bool valid = true;

                do
                {
                    changed = false;

                    for (auto i : g)
                        for (auto j : g)
                        {
                            auto m = i * j;

                            // if this happens it means that the group
                            // requires that we add a transformation that we don't have in
                            // our available set of transformations... this is the above
                            // mentioned criterion to throw away this trial
                            if (!m)
                            {
                                valid = false;
                            }
                            else if (!g.contains(m))
                            {
                                changed = true;
                                g.add(m);
                            }
                        }

                } while (changed && valid);

                // if the new group is valid and as yet unknown, we add it
                if (valid)
                    if (std::find(&GroupSymmetries[0], &GroupSymmetries[numGroups], g) == &GroupSymmetries[numGroups])
                    {
                        GroupSymmetries[numGroups++] = g;
                    }
            }
        }

        checked++;
    }

    // check, if we found the intended number of groups
    if (GRP_COUNT != numGroups)
    {
        fprintf(stderr, "different number of symmetries found than expected %d <--> %d\n", static_cast<int>(numGroups), static_cast<int>(GRP_COUNT));
        std::abort();
    }

    // the last one is automatically 0

    // Now we have an array of all symmetry groups, we now create a tree of checks
    // to find out what symmetry group a given shape has. For that we check one possible
    // rotation, and depending on the outcome go left or right in the tree
    // the leafs of the tree are the symmetry groups.
    //
    // The following function call recursively creates the tree.
    createSymmetryCheckTree(GroupSymmetries, {}, {}, DecisionTree, 0);

    // now for the calculation of the normalisation lookup table

    // go over all rotations that are inside this symmetry group
    // this code should go into a lookup table... but that table would be quite big
    // because it is 240*1079 entries big
    // maybe there is a simple way to compress it?
    for (int symgroup = 0; symgroup < GRP_COUNT + 1; symgroup++)
    {
        for (unsigned int inrot = 0; inrot < TRotation::NUM_ROTATION_MATRICES; inrot++)
        {
            TRotation rmin(inrot);
            TRotation rot(TRotation::ROT_IDENTITY);

            for (auto r : GroupSymmetries[symgroup])
            {
                // r is part of the symmetry group, meaning that applying r to
                // the untransformed object doesn't change its shape, so when we
                // first apply r and then inrot, we end up with the same as only applying
                // inrot. The difference is that is uses a different rotation matrix.
                // Out of all these matrices we look for the one with the smallest
                // index
                auto rnew = TRotation(inrot) * r;

                if (rnew.value() < rmin.value())
                {
                    rot  = r;
                    rmin = rnew;
                }
            }

            Normalizer[symgroup * TRotation::NUM_ROTATION_MATRICES + inrot] = rot;
            Normalized[symgroup * TRotation::NUM_ROTATION_MATRICES + inrot] = rmin;
        }
    }

    // calculate the table with unique transformations for each symmetry group
    // look into the comment of the getter function to understand what the value
    // actually means
    //
    // here is how we calculate it:
    // given are the symmetry of the piece Sp and the symmetry of the result Sr
    // then the result is a set of transformations with as many transformations
    // as possible but with the following limit, if a transformation
    // t is part of the result, then all transformations t*sp with sp out of Sp
    // and all transformation sr*t with sr out of Sr must not be part of the
    // result.
    // Why? Because all t*sp orientations would result in an identical looking
    // piece and all sr*t would result in an identical looking arrangement (the
    // result could be rotated with the piece to look identical to the result
    // with t.
    //
    // This is what is done here
    for (int symP = 0; symP < GRP_COUNT + 1; symP++)
        for (int symR = 0; symR < GRP_COUNT + 1; symR++)
        {
            TRotationSet covered; // all the orientations that we can no longer use

            for (auto rr : TRotationSet{TRotationSet::All{}})
                if (!covered.contains(rr))
                {
                    // is is not yet excluded, we add r to our result
                    Uniques[symP + symR * (GRP_COUNT + 1)].add(rr);

                    // now we need to calculate what transformations would end up giving
                    // us the same shape as when rotating by r
                    // and all those rotations must be added to covered
                    for (auto r2 : GroupSymmetries[symP])
                        // problem here is that the symmetry changes when the piece is rotated, that is
                        // why we can not simply apply r2 after rr, but we can also not simply
                        // apply r2 first and then rr because r
                        if (auto m = rr * r2; m) covered.add(m);

                    // and repeat for which result symmetries would give
                    // the same result arrangement
                    for (auto r2 : GroupSymmetries[symR])
                        if (auto m = r2 * rr; m) covered.add(m);
                }
        }

    // calculate the UnifySymmetryTransformation table
    for (int sym1 = 0; sym1 < GRP_COUNT + 1; sym1++)
        for (unsigned int tra = 0; tra < TRotation::NUM_ROTATION_MATRICES; tra++)
        {
            // get the transformation list for all transformations
            // that have to be at least in the group to find
            TRotationSet m = GroupSymmetries[sym1];
            m.add(TRotation(tra));

            // find the "best" possible fit, by best we mean
            // at least the bits in m are set, but as few additional
            // other ones as possible
            int best      = GRP_COUNT;
            int bestCount = 100;
            int bbb       = 0;

            for (int sym3 = 0; sym3 < GRP_COUNT; sym3++)
            {
                TRotationSet n = m - GroupSymmetries[sym3];

                if (n.empty())
                {
                    int c = GroupSymmetries[sym3].size();
                    if (c < bestCount)
                    {
                        bestCount = c;
                        best      = sym3;
                        bbb       = 1;
                    }
                    else if (c == bestCount)
                    {
                        bbb++;
                    }
                }
            }

            // check, if there is a unique best fit .. maybe there is not
            if (bbb == 0)
            {
                // you can not always unify, e.g. cube and triangular transformations will not always merge
                UnifySymmetryTransformation[sym1 * TRotation::NUM_ROTATION_MATRICES + tra] = GRP_COUNT;
            }
            else if (bbb == 1)
            {
                UnifySymmetryTransformation[sym1 * TRotation::NUM_ROTATION_MATRICES + tra] = best;
            }
            else
            {
                // but there must not be more than one
                fprintf(stderr, "found no best match for unified symmetry with transformation %d  %d %d %llu\n", static_cast<int>(bbb), static_cast<int>(sym1),
                        static_cast<int>(tra), static_cast<unsigned long long>(GroupSymmetries[sym1].value()));
                std::abort();
            }
        }

    // calculate the union and intersection tables
    for (int sym1 = 0; sym1 < GRP_COUNT + 1; sym1++)
        for (int sym2 = 0; sym2 < GRP_COUNT + 1; sym2++)
        {
            auto s1 = GroupSymmetries[sym1];
            auto s2 = GroupSymmetries[sym2];

            // intersection
            auto s = s1 & s2;

            bool found = false;

            for (int sym = 0; sym < GRP_COUNT; sym++)
                if (GroupSymmetries[sym] == s)
                {
                    found                                              = true;
                    IntersectSymmetrySymmetry[sym1 * GRP_COUNT + sym2] = sym;
                }

            if (sym1 == GRP_COUNT || sym2 == GRP_COUNT)
            {
                IntersectSymmetrySymmetry[sym1 * GRP_COUNT + sym2] = GRP_COUNT;
            }
            else if (!found)
            {
                fprintf(stderr, "found no match for intersection of 2 symmetry groups %d %d\n", static_cast<int>(sym1), static_cast<int>(sym2));
                std::abort();
            }

            // union
            RotationalSymmetryGroup sym(sym1);

            for (auto r : s2)
            {
                sym.addSymmetry(r);
                if (!sym) break;
            }

            UnifySymmetrySymmetry[sym1 * GRP_COUNT + sym2] = sym.value();
        }
}

// RotationalSymmetryGroup::calculateRotationalSubset needs the grid; it moves
// over with the grid in step G2 of design/2026-11-19-ng-merge-plan.md.

} // namespace BurrTools::PuzzleLibrary
