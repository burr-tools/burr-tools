/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

/* this class is a table containing references to voxelspaces
 * the important operation is to find out wheter a certain voxelspace is already
 * within the table. That includes all possible rotations (or even mirrors)
 *
 * The table itself only contains indices to a table of voxel spaces you have to derive
 * from this class an provide a function to actually get the right voxel space
 */

class voxelTable_c {

  private:

    bool includeMirrors;

    typedef struct hashNode {
      unsigned int index;
      unsigned char transformation;
      unsigned long hash;    // ve save the hash value here for the rehashing as the has value calculation is expensive
      struct hashNode * next;
    } hashNode;

    hashNode ** hashTable;
    unsigned long tableSize;
    unsigned long tableEntries;

  public:

    voxelTable_c(bool mirrors);
    virtual ~voxelTable_c(void);

    /* gets the index for the given voxel space, and also the transformation that transforms
     * the voxel space that is inside the table into this voxel space
     *
     * returns true, if found, else false
     *
     * you can provide NULL pointer to index and trans when you only want to know if the space
     * is inside the table
     */
    bool getSpace(const voxel_c *v, unsigned int *index = 0, unsigned char * trans = 0) const;
    bool getSpaceColour(const voxel_c *v, unsigned int *index = 0, unsigned char * trans = 0) const;
    bool getSpaceNoRot(const voxel_c *v, unsigned int *index = 0, unsigned char * trans = 0) const;

    /* add a voxel space to the table, the index is the number returned by getSpace and also
     * given to findSpace */
    void addSpace(const voxel_c * v, unsigned int index);

  protected:

    /* you must provide this function and return a pointer to the actual voxel space
     */
    virtual const voxel_c * findSpace(unsigned int index) const = 0;
};

/* a convenient class that already provides findSpace for a puzzle table */
class voxelTablePuzzle_c : public voxelTable_c {

  private:

    const puzzle_c * puzzle;

  public:

    voxelTablePuzzle_c(const puzzle_c * puz, bool mirrors) : voxelTable_c(mirrors), puzzle(puz) {}

  protected:

    const voxel_c * findSpace(unsigned int index) const;

};

#endif
