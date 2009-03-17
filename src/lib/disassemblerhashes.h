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
#ifndef __DISASSEMBLER_HASHES_H__
#define __DISASSEMBLER_HASHES_H__

class disassemblerNode_c;


/**
 * This is a hashtable that stores disassemblerNode_c pointer
 *
 * The nodes will not become owned by the hashtable, but the table
 * will use the reference counting system of the node
 */
class nodeHash {

  private:

    /** current table size */
    unsigned long tab_size;

    /** current number of entries */
    unsigned long tab_entries;

    /** the hashtable */
    disassemblerNode_c ** tab;

  public:

    nodeHash(void);

    ~nodeHash(void);

    /** delete all nodes and empty table for new usage */
    void clear(void);

    /**
     * add a new node.
     *
     * Returns
     * true, if the given node has already been in the table and nothing has changed
     * false if the node is inserted
     */
    bool insert(disassemblerNode_c * n);

    /** check, if a node is in the hashtable */
    bool contains(const disassemblerNode_c * n) const;
};



/**
 * Hastable like nodeHash with the additional feature
 * of scanning through all elements
 *
 * this is a hashtable that stores nodes but is also
 * alows traversal of all nodes hat are within the
 * table at a given point in time, only one such traversal
 * can be active at one time and the nodes are scanned in
 * the reverse order they were added
 */
class countingNodeHash {

  private:

    /** current table size */
    unsigned long tab_size;
    /** current number of entries */
    unsigned long tab_entries;

    /**
     * hash node data structur
     *
     * this hash table is non intrusive, it stores a pointer
     * to the disassembler node.
     *
     * This is more suitable here because the nodes normally live only
     * a short time inside this table, while they stay for a very long
     * time in the other table
     */
    struct hashNode {
      /** the data of the node */
      disassemblerNode_c * dat;
      /** next entry in the bucket list */
      hashNode * next;
      /** the next entry of the all element link list */
      hashNode * link;
    };

    /** the hash table */
    hashNode ** tab;
    /** pointer to the inverse linked list of all added elements */
    hashNode * linkStart;

    /** current scan position */
    hashNode * scanPtr;
    /** is there a scan active? */
    bool scanActive;

  public:

    countingNodeHash(void);
    ~countingNodeHash(void);

    /** delete all nodes and empty table for new usage */
    void clear(void);

    /**
     * add a new node.
     *
     * Returns
     * true, if the given node has already been in the table and nothing has changed
     * false if the node is inserted
     */
    bool insert(disassemblerNode_c * n);

    /** initialize a new scan.
     *
     * You can only do that one the currently active scan is
     * over
     * */
    void initScan(void);

    /**
     * return next disassembler node of the current scan.
     *
     * The nodes are returned one after the other in reverse order.
     * Once the last node has been returned you will get NULL. Then
     * you must not call this function any mode. You may then start
     * a new scan with initScan
     */
    const disassemblerNode_c * nextScan(void);
};

#endif

