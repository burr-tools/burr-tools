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
#ifndef __VOXEL_TABLE_H__
#define __VOXEL_TABLE_H__

class puzzle_c;
class voxel_c;

/**
 * This class is a table containing references to voxelspaces.
 *
 * This table can be used to quickly find dublicates of voxel spaces.
 * The table is a hash-table saving only the hash of the shape to add and then
 * using a user supplied function to actually get the space and do the comparison
 *
 * The idea is to add all possible orientations of the shape to add so that it
 * becomes easy to actually find something.
 *
 * Additionally it is possible to calculate the hash including or disregarding
 * the colours attatched to the shape.
 *
 * It is up to the user to specify which transformations/colour variations to add
 * to the table, only those will be found at the end
 *
 * To use this class you must derive it and provide a means to return the shape
 * to this table. The table itself only saves an index and with this index
 * the provided function must return the shape
 */

class voxelTable_c {

  private:

    /** the hash tabel entry */
    typedef struct hashNode {
      unsigned int index;           //< the shape index, which is given to the user to get the shape
      unsigned char transformation; //< which transformation of the shape is saved in here
      unsigned long hash;           //< the hash value of this transformation of the shape
      struct hashNode * next;       //< next entry
    } hashNode;

    hashNode ** hashTable;          //< the hash table
    unsigned long tableSize;        //< size of the hash table
    unsigned long tableEntries;     //< number of entries in hash table

  public:

    voxelTable_c(void);
    virtual ~voxelTable_c(void);

    /** Some parameter flags for the get and set function.
     * You can or them together when required
     */
    enum {
      PAR_MIRROR = 1,  ///< include mirrors of this shape
      PAR_COLOUR = 2   ///< differenly coloured shapes are different
    };

    /**
     * Returns true, if the given shape is inside the table
     *
     * The function also returns the index for the shape and the orientation that this shape
     * has relative to the one found at the returned index
     *
     * The parameter work as follows:
     *  - PAR_MIRROR: when set mirror orientations of the shape may be returned, otherwise
     *    the function either finds a non mirror orientation or nothing. Will only work
     *    when the shape is added using PAR_MIRROR
     *  - PAR_COLOUR: when set, shapes with different colouring will be different
     *
     * you can provide NULL pointer to index and trans when you only want to know if the space
     * is inside the table
     */
    bool getSpace(const voxel_c *v, unsigned int *index = 0, unsigned char * trans = 0, unsigned int params = 0) const;

    /**
     * Add a voxel space to the table.
     *
     * The index is the number returned by getSpace and also given to findSpace
     *
     * The params parameter works as follows:
     * - when PAR_MIRROR is set, the shape will be added in all possible orientations
     *   including mirror orientations, when not set only normal orientations will be used
     * - PAR_COLOUR: when set the hash will be calculated in a way that differently colourized
     *   shapes will have a differen hash, when not set, only the shape will go into the hash
     *
     * If you want to be able to search for all combinations you must call the function
     * twice, once with PAR_MIRROR and then again with PAR_MIRROR | PAR_COLOUR.
     * When PAR_MIRROR is set you can search with and without PAR_MIRROR.
     */
    void addSpace(unsigned int index, unsigned int params = 0);

  protected:

    /**
     * you must provide this function and return a pointer to the actual voxel space:
     *
     * For an actual example loot at the class voxelTablePuzzle_c
     */
    virtual const voxel_c * findSpace(unsigned int index) const = 0;

  private:

    // no copying and assigning
    voxelTable_c(const voxelTable_c&);
    void operator=(const voxelTable_c&);
};

/** a convenient class that already provides findSpace for a shape list in puzzle_c */
class voxelTablePuzzle_c : public voxelTable_c {

  private:

    const puzzle_c * puzzle;

  public:

    voxelTablePuzzle_c(const puzzle_c * puz) : puzzle(puz) {}

  protected:

    const voxel_c * findSpace(unsigned int index) const;

  private:

    // no copying and assigning
    voxelTablePuzzle_c(const voxelTable_c&);
    void operator=(const voxelTablePuzzle_c&);

};

#endif
