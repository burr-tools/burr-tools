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

#include "movementbrowser.h"

#include "LFl_Tile.h"
#include "guigridtype.h"
#include "view3dgroup.h"
#include "WindowWidgets.h"

#include "../lib/assembly.h"
#include "../lib/puzzle.h"
#include "../lib/disasmtomoves.h"
#include "../lib/disassemblernode.h"
#include "../lib/movementanalysator.h"
#include "../lib/movementcache.h"

#include "../flu/Flu_Tree_Browser.h"

#include <math.h>

class AddMovementDialog : public LFl_Double_Window {

  private:

    std::vector<LFl_Radio_Button*> dirs;
    std::vector<LFl_Check_Button*> pces;

    bool donePressed;

  public:

    void cb_Buttons(unsigned int which) {
      donePressed = which == 0;
      hide();
    }

    AddMovementDialog(movementCache_c * c, const std::vector<unsigned int> & pieces);


    unsigned int getDir(void) {
      for (unsigned int i = 0; i < dirs.size(); i++)
        if (dirs[i]->value())
          return i;

      return 0;
    }

    bool pieceSelected(unsigned int i) {
      bt_assert(i < pces.size());
      return pces[i]->value() != 0;
    }

    bool done(void) { return donePressed; }
};

static void cb_BtnDone_stub(Fl_Widget* /*o*/, void* v) { ((AddMovementDialog*)v)->cb_Buttons(0); }
static void cb_BtnCancel_stub(Fl_Widget* /*o*/, void* v) { ((AddMovementDialog*)v)->cb_Buttons(1); }

AddMovementDialog::AddMovementDialog(movementCache_c * c, const std::vector<unsigned int> & pieces) : LFl_Double_Window(false) {

  layouter_c * o = new layouter_c(0, 0, 1, 1);

  new LFl_Box("Direction", 0, 0, 1, 1);

  for (unsigned int i = 0; i < c->numDirections(); i++) {
    char label[20];
    char * t = label;
    int x, y, z;
    c->getDirection(i, &x, &y, &z);

    if (x > 0) t += snprintf(t, 20 - (t-label), "x");
    if (x < 0) t += snprintf(t, 20 - (t-label), "-x");
    if (y > 0) t += snprintf(t, 20 - (t-label), "y");
    if (y < 0) t += snprintf(t, 20 - (t-label), "-y");
    if (z > 0) t += snprintf(t, 20 - (t-label), "z");
    if (z < 0) t += snprintf(t, 20 - (t-label), "-z");
    dirs.push_back(new LFl_Radio_Button(label, 0, 2*i+1, 1, 1));
    (*dirs.rbegin())->copy_label(label);

    t = label;
    if (x > 0) t += snprintf(t, 20 - (t-label), "-x");
    if (x < 0) t += snprintf(t, 20 - (t-label), "x");
    if (y > 0) t += snprintf(t, 20 - (t-label), "-y");
    if (y < 0) t += snprintf(t, 20 - (t-label), "y");
    if (z > 0) t += snprintf(t, 20 - (t-label), "-z");
    if (z < 0) t += snprintf(t, 20 - (t-label), "z");
    dirs.push_back(new LFl_Radio_Button(label, 0, 2*i+2, 1, 1));
    (*dirs.rbegin())->copy_label(label);
  }

  (new LFl_Box("", 0, c->numDirections()*2+1, 1, 1))->weight(1, 1);

  dirs[0]->value(1);

  o->end();

  o = new layouter_c(1, 0, 1, 1);

  new LFl_Box("Pieces", 0, 0, 1, 1);

  for (unsigned int i = 0; i < pieces.size(); i++) {
    char label[20];
    snprintf(label, 20, "%i", pieces[i]);
    pces.push_back(new LFl_Check_Button(label, 0, i+1, 1, 1));
    (*pces.rbegin())->copy_label(label);
  }

  (new LFl_Box("", 0, pieces.size()+1, 1, 1))->weight(1, 1);

  o->end();

  o = new layouter_c(0, 1, 2, 1);

  new LFlatButton_c(0, 0, 1, 1, "Done", "", cb_BtnDone_stub, this);
  (new LFl_Box("", 1, 0, 1, 1))->setMinimumSize(5, 0);
  new LFlatButton_c(2, 0, 1, 1, "Cancel", "", cb_BtnCancel_stub, this);

  donePressed = false;
}

typedef struct nodeData_s {

  std::vector<unsigned int> pieces;
  disassemblerNode_c * node;

} nodeData_s;


LTreeBrowser::Node * movementBrowser_c::addNode(LTreeBrowser::Node *nd, disassemblerNode_c *mv) {

  /* at first make sure we do not already have a node with
   * this diassemblerNode
   */
  for (int i = 0; i < nd->children(); i++) {
    if (*(((nodeData_s*)(nd->child(i)->user_data()))->node) == *mv) {
      delete mv;
      return nd->child(i);
    }
  }

  nodeData_s * s = (nodeData_s *)(nd->user_data());
  bt_assert(s);

  /* we don't add the removal nodes */
  for (unsigned int i = 0; i < s->pieces.size(); i++)
    if (fabs(mv->getX(i)) > 10000 ||
        fabs(mv->getY(i)) > 10000 ||
        fabs(mv->getZ(i)) > 10000) {
      delete mv;
      return 0;
    }

  char label[200];
  char * t = label;
  bool firstpiece = true;

  for (unsigned int p = 0; p < s->pieces.size(); p++) {

    if (t == label) {
      if (s->node->getX(p) > mv->getX(p)) t += snprintf(t, 200-(t-label), "-x");
      if (s->node->getX(p) < mv->getX(p)) t += snprintf(t, 200-(t-label), "x");

      if (s->node->getY(p) > mv->getY(p)) t += snprintf(t, 200-(t-label), "-y");
      if (s->node->getY(p) < mv->getY(p)) t += snprintf(t, 200-(t-label), "y");

      if (s->node->getZ(p) > mv->getZ(p)) t += snprintf(t, 200-(t-label), "-z");
      if (s->node->getZ(p) < mv->getZ(p)) t += snprintf(t, 200-(t-label), "z");
    }

    if ((s->node->getX(p) != mv->getX(p)) ||
        (s->node->getY(p) != mv->getY(p)) ||
        (s->node->getZ(p) != mv->getZ(p)))
      if (firstpiece) {
        t += snprintf(t, 200-(t-label), ": %i", s->pieces[p]);
        firstpiece = false;
      } else
        t += snprintf(t, 200-(t-label), ", %i", s->pieces[p]);
  }

  LTreeBrowser::Node * nnew = tree->add(nd, label);

  nodeData_s * dat = new nodeData_s;
  nodes.push_back(dat);

  dat->node = mv;
  dat->node->incRefCount();

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  dat->pieces = s->pieces;

  nnew->user_data(dat);

  return nnew;
}

static void cb_NodeChange_stub(Fl_Widget* /*o*/, void* v) { ((movementBrowser_c*)v)->cb_NodeChange(); }
void movementBrowser_c::cb_NodeChange(void) {

  if (tree->callback_reason() != FLU_SELECTED) return;
  if (!tree->get_selected(1)) return;

  /* when the node has been changed, we need to update the 3d view */

  nodeData_s * s = (nodeData_s *)(tree->get_selected(1)->user_data());

  fixedPositions_c fp(s->node, s->pieces);

  view3d->updatePositionsOverlap(&fp);
}

static void cb_Prune_stub(Fl_Widget* /*o*/, void* v) { ((movementBrowser_c*)v)->cb_Prune(); }
void movementBrowser_c::cb_Prune(void) {

  LTreeBrowser::Node * nd = tree->get_selected(1);

  LTreeBrowser::Node * omit = 0;

  while (nd) {

    int i = 0;
    while (i < nd->children())
      if (nd->child(i) != omit)
        nd->remove(nd->child(i));
      else
        i++;

    omit = nd;
    nd = nd->parent();
  }


}

static void cb_AddMovement_stub(Fl_Widget* /*o*/, void* v) { ((movementBrowser_c*)v)->cb_AddMovement(); }
void movementBrowser_c::cb_AddMovement(void) {

  LTreeBrowser::Node * nd = tree->get_selected(1);
  if (!nd) return;

  nodeData_s * s = (nodeData_s *)(nd->user_data());
  if (!s) return;

  movementCache_c * c = puz->getGridType()->getMovementCache(puz, problem);

  AddMovementDialog dlg(c, s->pieces);

  dlg.show();

  while (dlg.visible())
    Fl::wait();

  int x, y, z;

  c->getDirection(dlg.getDir() >> 1, &x, &y, &z);
  if (dlg.getDir() & 1) {
    x = -x;
    y = -y;
    z = -z;
  }

  disassemblerNode_c * n = new disassemblerNode_c(s->pieces.size(), s->node, 0, 0);

  for (unsigned int i = 0; i < s->pieces.size(); i++)
    if (dlg.pieceSelected(i))
      n->set(i, s->node, x, y, z);
    else
      n->set(i, s->node, 0, 0, 0);

  addNode(nd, n)->select_only();

  delete c;

  redraw();
}

static void cb_NodeAnalyze_stub(Fl_Widget* /*o*/, void* v) { ((movementBrowser_c*)v)->cb_NodeAnalyze(1); }
static void cb_NodeAnalyzeMany_stub(Fl_Widget* /*o*/, void* v) { ((movementBrowser_c*)v)->cb_NodeAnalyze(2); }
void movementBrowser_c::cb_NodeAnalyze(unsigned int level) {

  LTreeBrowser::Node * nd = tree->get_selected(1);
  if (!nd) return;

  nodeData_s * s = (nodeData_s *)(nd->user_data());
  if (!s) return;

  /* find all possible movements and add them to the current node */

  std::vector<disassemblerNode_c*> res;

  movementAnalysator_c mv(puz, problem);
  mv.completeFind(s->node, s->pieces, &res);

  for (unsigned int i = 0; i < res.size(); i++)
    addNode(nd, res[i]);

  nd->open(true);

  redraw();
}


movementBrowser_c::movementBrowser_c(puzzle_c * puzzle, unsigned int prob, unsigned int solNum) : LFl_Double_Window(true) , puz(puzzle), problem(prob) {

  LFl_Tile * tile = new LFl_Tile(0, 0, 1, 1);

  guiGridType_c * ggt = new guiGridType_c(puz->getGridType());

  view3d = new LView3dGroup(1, 0, 1, 1, ggt);
  view3d->weight(1, 1);
  view3d->setMinimumSize(300, 300);

  delete ggt;

  layouter_c * lay = new layouter_c(0, 0, 1, 1);

  tree = new LTreeBrowser(0, 0, 1, 1);
  tree->weight(1, 1);
  tree->callback(cb_NodeChange_stub, this);
  tree->auto_branches(true);
  tree->selection_mode(FLU_SINGLE_SELECT);
  tree->selection_follows_hilight(true);

  layouter_c * o = new layouter_c(0, 1, 1, 1);

  analyzeNode =       new LFlatButton_c(0, 0, 1, 1, "Analyze", " Analyze this node for possible movement ", cb_NodeAnalyze_stub, this);
  analyzeNextLevels = new LFlatButton_c(1, 0, 1, 1, "Analyze more", " Analyze multiple levels for movement ", cb_NodeAnalyzeMany_stub, this);
  addMovement =       new LFlatButton_c(0, 1, 1, 1, "Add Movement", " Add a fixed movement to current node and see what happens ", cb_AddMovement_stub, this);
  pruneTree =         new LFlatButton_c(1, 1, 1, 1, "Prune Tree", " Remove All Nodes exept the ones that lead to the selected one ", cb_Prune_stub, this);

  o->end();

  lay->end();

  /* now add the root node to the tree */
  LTreeBrowser::Node * n = tree->get_root();
  n->label("root");

  /* the user data for a node is the structure nodeData_s */
  nodeData_s * dat = new nodeData_s;
  nodes.push_back(dat);

  assembly_c * assembly = puz->probGetAssembly(prob, solNum);

  dat->node = new disassemblerNode_c(assembly);
  dat->node->incRefCount();

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  for (unsigned int j = 0; j < assembly->placementCount(); j++)
    if (assembly->isPlaced(j))
      dat->pieces.push_back(j);

  n->user_data(dat);

  view3d->showAssembly(puz, prob, solNum);

  tree->get_root()->select_only();
}

movementBrowser_c::~movementBrowser_c() {

  /* we have to delete from the end because otherwise we delete nodes that others point to */
  for (unsigned int i = 0; i < nodes.size(); i++) {
    delete nodes[nodes.size()-1-i]->node;
    delete nodes[nodes.size()-1-i];
  }
}

