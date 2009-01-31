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
#ifndef __MOVEMENT_BROWSER_H__
#define __MOVEMENT_BORWSER_H__

#include "Layouter.h"

#include "../flu/Flu_Tree_Browser.h"

class LView3dGroup;
class LFlatButton_c;

class assembly_c;
class problem_c;
class disassemblerNode_c;

struct nodeData_s;

/* layoutable tree browser */
class LTreeBrowser : public Flu_Tree_Browser, public layoutable_c {

  public:

    LTreeBrowser(int x, int y, int w, int h) : Flu_Tree_Browser(0, 0, 20, 100), layoutable_c(x, y, w, h) {}

    virtual void getMinSize(int *width, int *height) const {
      *width = 20;
      *height = 100;
    }
};


class movementBrowser_c : public LFl_Double_Window {

  private:

    LView3dGroup * view3d;
    LTreeBrowser * tree;

    /* a few Buttons */
    LFlatButton_c * analyzeNode;
    LFlatButton_c * analyzeNextLevels;
    LFlatButton_c * addMovement;
    LFlatButton_c * pruneTree;

    problem_c * puz;

    /* this vector contains all the nodes that have been created
     * as we can not delete them we save them in here and remove
     * them updon window closing
     */
    std::vector<nodeData_s*> nodes;

    LTreeBrowser::Node * addNode(LTreeBrowser::Node *nd, disassemblerNode_c *mv);

  public:

    movementBrowser_c(problem_c * puz, unsigned int solNum);
    ~movementBrowser_c();


    /* callback functions */
    void cb_NodeChange(void);
    void cb_NodeAnalyze(unsigned int level);
    void cb_AddMovement(void);
    void cb_Prune(void);
};

#endif
