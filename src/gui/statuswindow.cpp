/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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
#include "statuswindow.h"
#include "piececolor.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/millable.h"
#include "../lib/voxeltable.h"

#include <cstdio>
#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#pragma GCC diagnostic pop

class LFl_Line : public Fl_Box, public layoutable_c {

  private:

    int thickness;

  public:

  LFl_Line(int x, int y, int w, int h, int thick = 1, Fl_Color col = FL_BLACK) : Fl_Box(0, 0, 0, 0), layoutable_c(x, y, w, h), thickness(thick) {
    color(col);
    box(FL_FLAT_BOX);
  }

  virtual void getMinSize(int *width, int *height) const {
    *width = thickness;
    *height = thickness;
  }
};

static int longestWordWidth(const char *s) {
  int maxW = 0;
  if (!s)
    return 0;

  char buf[64];
  const char *p = s;
  while (*p) {
    while (*p == ' ')
      p++;
    const char *start = p;
    while (*p && *p != ' ')
      p++;
    int n = (int)(p - start);
    if (n <= 0)
      continue;
    if (n > 63)
      n = 63;
    memcpy(buf, start, n);
    buf[n] = 0;
    int w = 0, h = 0;
    fl_measure(buf, w, h);
    if (w > maxW)
      maxW = w;
  }
  return maxW;
}

class LFl_WrapButton : public LFl_Button {

  public:

  LFl_WrapButton(const char *text, int x = 0, int y = 0, int w = 1, int h = 1)
    : LFl_Button(text, x, y, w, h) {
    align(FL_ALIGN_WRAP | FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
  }

  int lineHeight(void) const {
    int w = 0, h = 0;
    fl_font(labelfont(), labelsize());
    fl_measure(label(), w, h);
    return h + 10;
  }

  int lineWidth(void) const {
    int w = 0, h = 0;
    fl_font(labelfont(), labelsize());
    fl_measure(label(), w, h);
    return w + 16;
  }

  int wordFloorWidth(void) const {
    fl_font(labelfont(), labelsize());
    int w = longestWordWidth(label()) + 16;
    if (w < 28)
      w = 28;
    return w;
  }

  int wrappedHeight(int boxW) const {
    int mw = boxW - 8;
    if (mw < 8)
      mw = 8;
    int mh = 0;
    fl_font(labelfont(), labelsize());
    fl_measure(label(), mw, mh);
    int hh = mh + 10;
    int minH = lineHeight();
    if (hh < minH)
      hh = minH;
    return hh;
  }
};

/* A row of wrapping buttons. fill=true shares the full width; fill=false
 * keeps a compact equal-width group and centers it in the pane. */
class wrapButtonRow_c : public layouter_c {

  std::vector<LFl_WrapButton*> btns;
  int gap;
  bool fill;
  int cachedH;
  bool bubbling;

  public:

  wrapButtonRow_c(int x, int y, int w, int h, bool fillWidth, int gapPx)
    : layouter_c(x, y, w, h), gap(gapPx), fill(fillWidth), cachedH(0), bubbling(false) {
    weight(1, 0);
    shrinkPrio(0, 255);
  }

  LFl_WrapButton *addButton(const char *text, Fl_Callback *cb, void *user, const char *tip) {
    LFl_WrapButton *b = new LFl_WrapButton(text);
    b->callback(cb, user);
    if (tip)
      b->tooltip(tip);
    btns.push_back(b);
    return b;
  }

  void finish(void) {
    int n = (int)btns.size();
    int floorEach = 28;
    int lineH = 20;
    for (size_t i = 0; i < btns.size(); i++) {
      int fw = btns[i]->wordFloorWidth();
      if (fw > floorEach)
        floorEach = fw;
      int lh = btns[i]->lineHeight();
      if (lh > lineH)
        lineH = lh;
    }
    int minG = (n > 1) ? 2 * (n - 1) : 0;
    setShrinkMinSize(n * floorEach + minG, lineH);
    if (cachedH < lineH)
      cachedH = lineH;
  }

  virtual void getMinSize(int *width, int *height) const {
    int n = (int)btns.size();
    if (n == 0) {
      *width = 0;
      *height = 0;
      return;
    }

    int prefEach = 0, floorEach = 28, lineH = 20;
    for (size_t i = 0; i < btns.size(); i++) {
      int lw = btns[i]->lineWidth();
      int fw = btns[i]->wordFloorWidth();
      int lh = btns[i]->lineHeight();
      if (lw > prefEach)
        prefEach = lw;
      if (fw > floorEach)
        floorEach = fw;
      if (lh > lineH)
        lineH = lh;
    }

    int gaps = (n - 1) * gap;
    if (fill)
      *width = n * floorEach + gaps;
    else
      *width = n * prefEach + gaps;

    *height = (cachedH > lineH) ? cachedH : lineH;
  }

  virtual void resize(int X, int Y, int W, int H) {
    int n = (int)btns.size();
    Fl_Widget::resize(X, Y, W, H);
    if (n == 0)
      return;

    int prefEach = 0, floorEach = 28;
    for (size_t i = 0; i < btns.size(); i++) {
      int lw = btns[i]->lineWidth();
      int fw = btns[i]->wordFloorWidth();
      if (lw > prefEach)
        prefEach = lw;
      if (fw > floorEach)
        floorEach = fw;
    }

    int g = gap;
    if (W < n * floorEach + (n - 1) * g)
      g = 2;

    int btnW;
    if (fill) {
      btnW = (W - (n - 1) * g) / n;
    } else {
      btnW = prefEach;
      int total = n * btnW + (n - 1) * g;
      if (total > W)
        btnW = (W - (n - 1) * g) / n;
    }
    if (btnW < 1)
      btnW = 1;

    int needH = 0;
    for (size_t i = 0; i < btns.size(); i++) {
      int hh = btns[i]->wrappedHeight(btnW);
      if (hh > needH)
        needH = hh;
    }

    int total = n * btnW + (n - 1) * g;
    int x0 = X + (W - total) / 2;
    if (x0 < X)
      x0 = X;
    int y0 = Y;
    int rowH = needH;
    if (H > needH) {
      y0 = Y + (H - needH) / 2;
    } else {
      rowH = H;
    }

    for (size_t i = 0; i < btns.size(); i++) {
      btns[i]->resize(x0, y0, btnW, rowH);
      x0 += btnW + g;
    }

    if (needH != cachedH && !bubbling) {
      cachedH = needH;
      invalidateMinSize();
      bubbling = true;
      Fl_Widget *p = parent();
      while (p) {
        layouter_c *lay = dynamic_cast<layouter_c*>(p);
        if (lay)
          lay->invalidateMinSize();
        if (dynamic_cast<statusWindow_c*>(p)) {
          p->resize(p->x(), p->y(), p->w(), p->h());
          break;
        }
        p = p->parent();
      }
      bubbling = false;
    }
  }
};

static void cb_ProgressClose_stub(Fl_Widget*, void* v) { ((LFl_Double_Window*)v)->hide(); }
static void cb_DetailsClose_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_close(); }
static void cb_DetailsRefresh_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_refresh(); }
static void cb_RemoveSelected_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_removeSelected(); }
static void cb_SelectHoles_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_selectHoles(); }
static void cb_SelectIdenticalShapes_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_selectIdenticalShapes(); }
static void cb_SelectIdenticalComplete_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_selectIdenticalComplete(); }
static void cb_SelectIdenticalMirror_stub(Fl_Widget*, void* v) { ((statusWindow_c*)v)->cb_selectIdenticalMirror(); }

class StatusProgress : public LFl_Double_Window {

  private:

    LFl_Progress * p;

  public:

    StatusProgress(void) : LFl_Double_Window(false) {

      label("Progress");

      (new LFl_Box("Calculating Status information.\n"
                  "This might take some time...", 0, 0, 1, 1))->pitch(3);

      p = new LFl_Progress(0, 1, 1, 1);
      p->minimum(0);
      p->maximum(1);
      p->selection_color((Fl_Color)4);
      p->pitch(3);

      LFl_Button * btn = new LFl_Button("Cancel", 0, 2, 1, 1);
      btn->pitch(3);
      btn->callback(cb_ProgressClose_stub, this);

      end();

      set_modal();
    }

    void setProgress(float value) {
      p->value(value);
    }
};

statusWindow_c::statusWindow_c(int x, int y, int w, int h)
  : layouter_c(x, y, w, h), puz(0), cbUser(0), closeCb(0), changedCb(0)
{
  box(FL_THIN_UP_BOX);
  setMinimumSize(200, 210);
  clip_children(1);
  end();
}

void statusWindow_c::setCallbacks(Fl_Callback * onClose, Fl_Callback * onChanged, void * user)
{
  closeCb = onClose;
  changedCb = onChanged;
  cbUser = user;
}

void statusWindow_c::getMinSize(int *width, int *height) const {
  /* The table inside can be very tall. Report the docked-pane size so the
   * parent can keep this panel visible at the bottom instead of overflowing. */
  *width = getMinWidth() > 0 ? (int)getMinWidth() : 200;
  *height = getMinHeight() > 0 ? (int)getMinHeight() : 210;
}

void statusWindow_c::clearChildren(void)
{
  selection.clear();
  identicalMirror.clear();
  identicalShape.clear();
  identicalComplete.clear();
  while (children()) {
    Fl_Widget * c = child(0);
    remove(*c);
    delete c;
  }
}

void statusWindow_c::cb_close(void)
{
  if (closeCb)
    closeCb(this, cbUser);
}

void statusWindow_c::cb_refresh(void)
{
  populate(puz);
}

void statusWindow_c::cb_removeSelected(void) {

  if (!puz)
    return;

  bt_assert(selection.size() <= puz->getNumberOfShapes());

  /* we have to go up from the bottom as otherwise the indixes may shift
   *
   * we hafe to use the selection size as starting point as
   * the user may have pressed cancel during calculation leaving us
   * with an incomplete list
   */
  for (unsigned int s = selection.size(); s > 0; s--)
  {
    if (selection[s-1]->value())
    {
      for (unsigned int i = 0; i < puz->getNumberOfProblems(); i++)
        if (puz->getProblem(i)->usesShape(s-1))
          puz->getProblem(i)->removeAllSolutions();

      puz->removeShape(s-1);
    }
  }

  if (changedCb)
    changedCb(this, cbUser);

  populate(puz);
}

void statusWindow_c::cb_selectHoles(void) {

  if (!puz)
    return;

  bt_assert(selection.size() <= puz->getNumberOfShapes());

  for (unsigned int s = 0; s < selection.size(); s++) {
    const voxel_c * v = puz->getShape(s);
    bool has2DHoles = !v->connected(0, false, 0, false);
    bool has3DHoles = !v->connected(0, false, 0);
    if (has2DHoles || has3DHoles)
      selection[s]->value(1);
  }
}

void statusWindow_c::cb_selectIdenticalShapes(void) {

  for (unsigned int s = 0; s < selection.size(); s++)
    if (s < identicalShape.size() && identicalShape[s])
      selection[s]->value(1);
}

void statusWindow_c::cb_selectIdenticalComplete(void) {

  for (unsigned int s = 0; s < selection.size(); s++)
    if (s < identicalComplete.size() && identicalComplete[s])
      selection[s]->value(1);
}

void statusWindow_c::cb_selectIdenticalMirror(void) {

  for (unsigned int s = 0; s < selection.size(); s++)
    if (s < identicalMirror.size() && identicalMirror[s])
      selection[s]->value(1);
}

void statusWindow_c::populate(puzzle_c * p) {

  puz = p;
  clearChildren();

  if (!p)
    return;

  Fl_Group * prev = Fl_Group::current();
  Fl_Group::current(0);
  StatusProgress * stp = new StatusProgress;
  stp->show();

  begin();

  char tmp[200];

  unsigned int lines = p->getNumberOfShapes();
  unsigned int head = 3;

  layouter_c * fr = new layouter_c(0, 0, 1, 1);
  fr->pitch(7);

  LFl_Scroll *tableScroll = new LFl_Scroll(0, 0, 1, 1);
  tableScroll->type(Fl_Scroll::VERTICAL_ALWAYS);
  tableScroll->weight(1, 1);
  tableScroll->setMinimumSize(40, 64);
  tableScroll->setShrinkMinSize(40, 56);

  unsigned int cols = 27;

  // 2 more columns for notchable and millable
  if (p->getGridType()->getType() == gridType_c::GT_BRICKS)
    cols += 4;

  voxelTablePuzzle_c shapeTab(p);

  for (unsigned int s = 0; s < p->getNumberOfShapes(); s++) {

    LFl_Box * b;

    if (s & 1) {
      b = new LFl_Box("", 0, s+head, cols, 1);
      b->color(fl_rgb_color(150, 150, 150));
      b->box(FL_FLAT_BOX);
    }

    const voxel_c * v = p->getShape(s);

    unsigned int col = 0;

    selection.push_back(new LFl_Check_Button(" ", col, s+head));
    col+=2;

    if (v->getName().length())
      snprintf(tmp, 200, "S%i - %s", s+1, v->getName().c_str());
    else
      snprintf(tmp, 200, "S%i", s+1);

    b = new LFl_Box("", col, s+head);
    b->copy_label(tmp);
    b->color(fltkPieceColor(s));
    b->labelcolor(contrastPieceColor(s));
    b->box(FL_FLAT_BOX);
    col += 2;

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", col, s+head))->copy_label(tmp);
    col += 2;

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE));
    (new LFl_Box("", col, s+head))->copy_label(tmp);
    col += 2;

    snprintf(tmp, 200, "%i", v->countState(voxel_c::VX_VARIABLE) + v->countState(voxel_c::VX_FILLED));
    (new LFl_Box("", col, s+head))->copy_label(tmp);
    col += 2;
    Fl::wait(0);

    unsigned int shapeIdx;
    unsigned char shapeTrans;
    bool shapeKnown = shapeTab.getSpace(v, &shapeIdx, &shapeTrans, voxelTable_c::PAR_MIRROR);
    identicalMirror.push_back(shapeKnown);

    if (shapeKnown)
    {
      snprintf(tmp, 200, "%i", shapeIdx+1);
      b = new LFl_Box("", col, s+head);
      b->copy_label(tmp);
      b->color(fltkPieceColor(shapeIdx));
      b->labelcolor(contrastPieceColor(shapeIdx));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    shapeKnown = shapeTab.getSpace(v, &shapeIdx, &shapeTrans, 0);
    identicalShape.push_back(shapeKnown);

    if (shapeKnown)
    {
      snprintf(tmp, 200, "%i", shapeIdx+1);
      b = new LFl_Box("", col, s+head);
      b->copy_label(tmp);
      b->color(fltkPieceColor(shapeIdx));
      b->labelcolor(contrastPieceColor(shapeIdx));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    shapeKnown = shapeTab.getSpace(v, &shapeIdx, &shapeTrans, voxelTable_c::PAR_COLOUR);
    identicalComplete.push_back(shapeKnown && shapeTrans < p->getGridType()->getSymmetries()->getNumTransformations());

    if (shapeKnown && shapeTrans < p->getGridType()->getSymmetries()->getNumTransformations())
    {
      snprintf(tmp, 200, "%i", shapeIdx+1);
      b = new LFl_Box("", col, s+head);
      b->copy_label(tmp);
      b->color(fltkPieceColor(shapeIdx));
      b->labelcolor(contrastPieceColor(shapeIdx));
      b->box(FL_FLAT_BOX);
    }
    col +=2 ;
    Fl::wait(0);

    if (v->connected(0, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (v->connected(1, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (v->connected(2, true, 0)) {
      new LFl_Box("X", col, s+head);
    } else {
      b = new LFl_Box("", col, s+head);
      b->color(fl_rgb_color(pieceColorRi(s), pieceColorGi(s), pieceColorBi(s)));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (!v->connected(0, false, 0, false)) {
      b = new LFl_Box("X", col, s+head);
      b->color(fltkPieceColor(s));
      b->labelcolor(contrastPieceColor(s));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (!v->connected(0, false, 0)) {
      b = new LFl_Box("X", col, s+head);
      b->color(fltkPieceColor(s));
      b->labelcolor(contrastPieceColor(s));
      b->box(FL_FLAT_BOX);
    }
    col += 2;
    Fl::wait(0);

    if (p->getGridType()->getType() == gridType_c::GT_BRICKS) {
      if (isNotchable(v))
        b = new LFl_Box("X", col, s+head);

      col += 2;

      if (isMillable(v))
        b = new LFl_Box("X", col, s+head);

      col += 2;
    }

    if (!p->getGridType()->getSymmetries()->symmetryKnown(v)) {
      b = new LFl_Box("---", col, s+head);
      b->color(fltkPieceColor(s));
      b->labelcolor(contrastPieceColor(s));
      b->box(FL_FLAT_BOX);
    } else {
      snprintf(tmp, 200, "%i", p->getGridType()->getSymmetries()->calculateSymmetry(v));
      b = new LFl_Box("", col, s+head);
      b->copy_label(tmp);
      b->box(FL_NO_BOX);
    }
    col += 2;

    stp->setProgress(1.0*s/p->getNumberOfShapes());
    Fl::wait(0);
    if (!stp->visible())
      break;

    shapeTab.addSpace(s, voxelTable_c::PAR_MIRROR);
    shapeTab.addSpace(s, voxelTable_c::PAR_MIRROR | voxelTable_c::PAR_COLOUR);
  }

  stp->hide();
  delete stp;

  unsigned int col = 1;

  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Shape", col++, 0))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Units", col, 0, 5))->pitch(2);
  (new LFl_Box("Normal", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Variable", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Total", col++, 1))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Identical", col, 0, 5))->pitch(2);
  (new LFl_Box("Mirror", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Shape", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Complete", col++, 1))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Connectivity", col, 0, 5))->pitch(2);
  (new LFl_Box("Face", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Edge", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("Corner", col++, 1))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  (new LFl_Box("Holes", col, 0, 3))->pitch(2);
  (new LFl_Box("2D", col++, 1))->pitch(2);
  new LFl_Line(col++, 1, 1, lines+head-1, 1);
  (new LFl_Box("3D", col++, 1))->pitch(2);
  new LFl_Line(col++, 0, 1, lines+head, 2);

  if (p->getGridType()->getType() == gridType_c::GT_BRICKS) {

    (new LFl_Box("Tools", col, 0, 3))->pitch(2);
    (new LFl_Box("Notch", col++, 1))->pitch(2);
    new LFl_Line(col++, 1, 1, lines+head-1, 1);
    (new LFl_Box("Mill", col++, 1))->pitch(2);
    new LFl_Line(col++, 0, 1, lines+head, 2);
  }

  (new LFl_Box("Sym", col++, 0, 1))->pitch(2);

  new LFl_Line(0, 2, cols, 1, 2);

  fr->end();
  fr->setMinimumSize(40, 140);
  fr->setShrinkMinSize(40, 64);
  fr->shrinkPrio(0, 0);
  fr->weight(1, 1);

  fr = new layouter_c(0, 1, 1, 1);
  fr->pitch(7);
  fr->weight(1, 0);
  fr->shrinkPrio(255, 255);

  wrapButtonRow_c *actionRow = new wrapButtonRow_c(0, 0, 1, 1, false, 12);
  actionRow->addButton("Refresh", cb_DetailsRefresh_stub, this,
                       " Reload piece information ");
  actionRow->addButton("Close", cb_DetailsClose_stub, this, 0);
  actionRow->addButton("Remove selected", cb_RemoveSelected_stub, this, 0);
  actionRow->finish();
  actionRow->end();

  LFl_Box * btnGap = new LFl_Box(0, 1, 1, 1);
  btnGap->setMinimumSize(0, 6);
  btnGap->setShrinkMinSize(0, 6);
  btnGap->shrinkPrio(255, 255);

  wrapButtonRow_c *selRow = new wrapButtonRow_c(0, 2, 1, 1, true, 6);
  selRow->addButton("Select holes", cb_SelectHoles_stub, this,
                    " Select all shapes that have 2D or 3D holes ");
  selRow->addButton("Select Identical Shapes", cb_SelectIdenticalShapes_stub, this,
                    " Select shapes that match another shape ignoring colour ");
  selRow->addButton("Select Identical Complete", cb_SelectIdenticalComplete_stub, this,
                    " Select shapes that match another shape including colour ");
  selRow->addButton("Select Identical Mirror", cb_SelectIdenticalMirror_stub, this,
                    " Select shapes that match another shape as a mirror image ");
  selRow->finish();
  selRow->end();

  fr->end();

  end();
  if (prev)
    Fl_Group::current(prev);

  resize(x(), y(), w(), h());
}
