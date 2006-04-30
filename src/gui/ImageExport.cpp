/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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
#include "ImageExport.h"
#include "voxeldrawer.h"
#include "Image.h"

#include "../lib/puzzle.h"
#include "../lib/disassembly.h"

#include <FL/Fl.h>

/* 2 classes for layoutable widgets */
class LView3dGroup : public View3dGroup, public layoutable_c {

  public:

    LView3dGroup(int x, int y, int w, int h, const guiGridType_c * ggt) : View3dGroup(0, 0, 50, 50, ggt), layoutable_c(x, y, w, h) {}

    virtual void getMinSize(int * w, int *h) const {
      *w = 400;
      *h = 400;
    }
};

class LBlockListGroup : public BlockListGroup, public layoutable_c {
  public:
    LBlockListGroup(int x, int y, int w, int h, BlockList * l) : BlockListGroup(0, 0, 50, 50, l), layoutable_c(x, y, w, h) {
      pitch(1);
    }

    virtual void getMinSize(int *w, int *h) const {
      *w = 100;
      *h = 60;
    }
};

/* image info contains the information for one image to be exported
 * this information is the setup required to produce this image, also
 * the size
 */
class ImageInfo {

  public:

    typedef enum {
      SHOW_SINGLE,
      SHOW_ASSEMBLY
    } functions;

  private:

    // the parameters to set up the image
    functions setupFunction;

    puzzle_c * puzzle;

    // parameters for single
    unsigned int shape;
    bool showColors;

    // parameters for assembly
    unsigned int problem;
    unsigned int solution;
    bool dim;

    DisasmToMoves * positions;

    // the image data
    Image * i;  // image generated with the drawer that is with a fixed hight and the required width
    Image * i2; // the final image
    unsigned int i2aa;

    /* the openGL context to draw to */
    voxelDrawer_c * vv;

  public:

    /* create image info to create a single shape image */
    ImageInfo(puzzle_c * p, bool color,
        unsigned int s, voxelDrawer_c * v) : setupFunction(SHOW_SINGLE), puzzle(p),
                                          shape(s), showColors(color),
                                          i(new Image(600, 200)), i2(0), vv(v) { }

    /* image info for an assembly, if you don't give pos, you will get the standard assembly with
     * no piece shifted
     */
    ImageInfo(puzzle_c * p, bool color, unsigned int prob,
        unsigned int sol, voxelDrawer_c * v,
        DisasmToMoves * pos = 0, bool d = false) : setupFunction(SHOW_ASSEMBLY), puzzle(p),
                                                   showColors(color), problem(prob),
                                                   solution(sol), dim(d), positions(pos),
                                                   i(new Image(600, 200)),
                                                   i2(0), vv(v) { }

    ~ImageInfo() {
      if (i) delete i;
      if (i2) delete i2;
    }

    /* set up the voxelDrawer_c so that is shows the information for this image */
    void setupContent(void);

    /* preparation to get a tile for the preview image */
    void preparePreviewImage(void);

    /* get a tile of the preview image */
    bool getPreviewImage(void);

    /* get the width / height ratio of the preview image */
    double ratio(void) { return ((double)(i->w()))/i->h(); }

    /* start a new image */
    void generateImage(unsigned int w, unsigned int h, unsigned char aa);

    /* returns true, if the image has been started */
    bool imageStarted(void) { return i2; }

    /* prepare for a new tile of the image */
    void prepareImage(void);

    /* get a new tile for the image */
    Image * getImage(void);
};

void ImageInfo::setupContent(void) {

  switch (setupFunction) {
    case SHOW_SINGLE:
      vv->showSingleShape(puzzle, shape);
      vv->showColors(puzzle, showColors);

      break;
    case SHOW_ASSEMBLY:
      vv->showAssembly(puzzle, problem, solution);
      vv->showColors(puzzle, showColors);

      if (positions) {
        vv->updatePositions(positions);
        if (dim)
          vv->dimStaticPieces(positions);
      }
  }
}

/* preparation to get a tile for the preview image */
void ImageInfo::preparePreviewImage(void) {

  setupContent();
  i->prepareOpenGlImagePart(vv);
  glClearColor(1, 1, 1, 0);
}

/* get a tile of the preview image */
bool ImageInfo::getPreviewImage(void) {

  if (!i->getOpenGlImagePart()) {

    i->transparentize(255, 255, 255);
    i->minimizeWidth(10);

    return false;
  } else
    return true;
}

/* start a new image */
void ImageInfo::generateImage(unsigned int w, unsigned int h, unsigned char aa) {
  if (i2)
    delete i2;
  i2 = new Image ((h*3)*aa, h*aa);
  i2aa = aa;
}

/* prepare for a new tile of the image */
void ImageInfo::prepareImage(void) {

  setupContent();
  i2->prepareOpenGlImagePart(vv);
  glClearColor(1, 1, 1, 0);
}

/* get a new tile for the image */
Image * ImageInfo::getImage(void) {

  if (!i2->getOpenGlImagePart()) {

    i2->transparentize(255, 255, 255);
    i2->minimizeWidth(i->h()/60, i2aa);
    i2->scaleDown(i2aa);

    return i2;
  } else
    return 0;
}





static void cb_ImageExportAbort_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Abort(); }
void ImageExportWindow::cb_Abort(void) {
  hide();
}

bool ImageExportWindow::PreDraw(void) {

  static char statText[50];

  switch(state) {
    case 0:

      snprintf(statText, 50, "create Preview Image %i / %i", im, images.size());
      status->label(statText);

      images[im]->preparePreviewImage();

      return true;

    case 1:

      if (!images[im]->imageStarted()) {

        snprintf(statText, 50, "create Image %i / %i", im, images.size());
        status->label(statText);

        unsigned int pageHeight = atoi(SizePixelY->value());
        unsigned int w = (unsigned int)((pageHeight / linesPerPage) * images[im]->ratio() + 0.9);

        // calculate antialiasing factor
        int aa = 1;
        if (AA2->value()) aa = 2;
        if (AA3->value()) aa = 3;
        if (AA4->value()) aa = 4;
        if (AA5->value()) aa = 5;

        images[im]->generateImage(w, pageHeight / linesPerPage, aa);
      }

      images[im]->prepareImage();

      return true;
  }

  return false;
}

void ImageExportWindow::nextImage(bool finish) {

  static char statText[20];

  if (i) {

    snprintf(statText, 20, "save page %i", curPage);
    status->label(statText);

    char name[1000];

    if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/')
      snprintf(name, 1000, "%s/%s%03i.png", Pname->value(), Fname->value(), curPage);
    else
      snprintf(name, 1000, "%s%s%03i.png", Pname->value(), Fname->value(), curPage);

    i->saveToPNG(name);
    delete i;
    i = 0;
  }

  if (!finish) {

    unsigned int pageHeight = atoi(SizePixelY->value());
    unsigned int pageWidth = atoi(SizePixelX->value());

    if (BgWhite->value()) {
      i = new Image(pageWidth, pageHeight, 255, 255, 255, 255);
    } else {
      i = new Image(pageWidth, pageHeight, 0, 0, 0, 0);
    }
  }
}

void ImageExportWindow::PostDraw(void) {

  switch(state) {
    case 0:

      if (!images[im]->getPreviewImage()) {

        // finished preview image, next
        im++;

        // all preview images generated?
        if (im >= images.size()) {

          // now find out in how many lines the images need to be put onto the pages to
          // get them all onto the available space
          linesPerPage = 1;

          unsigned int pageWidth = atoi(SizePixelX->value());

          // if we have less images than pages, lower pages
          unsigned int pages = atoi(NumPages->value());
          if (pages > images.size()) pages = images.size();

          while (true) {

            curWidth = 0;
            curLine = 0;
            curPage = 0;

            unsigned int imgHeight = atoi(SizePixelY->value()) / linesPerPage;

            // check, if everything fits with the current number of lines
            for (unsigned int im = 0; im < images.size(); im++) {
              // calculate width of the image when it has the current line hight
              unsigned int w = (unsigned int)(imgHeight * images[im]->ratio() + 0.9);

              if (curWidth + w < pageWidth) {
                // image fits onto the line
                curWidth += w;
              } else {
                // image on the next line
                curWidth = w;
                curLine++;
                if (curLine >= linesPerPage) {
                  curLine = 0;
                  curPage++;
                }
              }
            }

            // check if we fit
            if ((curPage <= pages) || ((curPage == pages+1) && (curLine == 0) && (curWidth == 0)))
              break;

            linesPerPage++;
          }

          // ok, now lets setup the variable for output
          curWidth = 0;
          curLine = 0;
          curPage = 0;

          nextImage(false);

          im = 0;
          state = 1;
        }
      }

      break;

    case 1:

      Image * i2 = images[im]->getImage();

      if (i2) {

        unsigned int pageWidth = atoi(SizePixelX->value());
        unsigned int imgHeight = atoi(SizePixelY->value()) / linesPerPage;

        unsigned int w = i2->w();

        if (curWidth + w < pageWidth) {
          // image fits onto the line
          i->blit(i2, curWidth, curLine * imgHeight);
          curWidth += w;
        } else {
          // image on the next line
          curWidth = w;
          curLine++;
          if (curLine >= linesPerPage) {
            curLine = 0;
            nextImage(false);
            curPage++;
          }
          i->blit(i2, 0, curLine * imgHeight);
        }

        im++;

        if (im >= images.size()) {

          nextImage(true);

          // finished,
          state = 3;

          // remove callbacks
          view3D->getView()->setCallback();

          status->label("Done");

          working = false;
        }
      }

      break;
  }
}


static void cb_ImageExportExport_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Export(); }
void ImageExportWindow::cb_Export(void) {

  /* this vector contains all the information of all images that need to appear in the output */
  images.clear();

  if (ExpShape->value()) {

    images.push_back(new ImageInfo(puzzle, ColConst->value(), ShapeSelect->getSelection(), view3D->getView()));

  } else if (ExpAssembly->value()) {

    images.push_back(new ImageInfo(puzzle, ColConst->value(), ProblemSelect->getSelection(), 0, view3D->getView()));

  } else if (ExpSolution->value()) {

    // renerate an image for each step (for the moment only for the last solution)
    unsigned int s = puzzle->probSolutionNumber(ProblemSelect->getSelection()) - 1;
    separation_c * t = puzzle->probGetDisassembly(ProblemSelect->getSelection(), s);
    unsigned int prob = ProblemSelect->getSelection();
    if (!t) return;

    for (unsigned int step = 0; step < t->sumMoves(); step++) {
      DisasmToMoves * dtm = new DisasmToMoves(t, 20);
      dtm->setStep(step);
      images.push_back(new ImageInfo(puzzle, ColConst->value(), prob, s, view3D->getView(), dtm, DimStatic->value()));
    }

  } else if (ExpProblem->value()) {
    // generate an image for each piece in the problem
    unsigned int prob = ProblemSelect->getSelection();

    images.push_back(new ImageInfo(puzzle, ColConst->value(), puzzle->probGetResult(prob), view3D->getView()));

    for (unsigned int p = 0; p < puzzle->probShapeNumber(prob); p++)
      images.push_back(new ImageInfo(puzzle, ColConst->value(), puzzle->probGetShape(prob, p), view3D->getView()));

  } else

    return;

  im = 0;
  working = true;
  BtnStart->deactivate();
  BtnAbbort->deactivate();
  state = 0;

  view3D->getView()->setCallback(this);
}

static void cb_ImageExport3DUpdate_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Update3DView(); }
void ImageExportWindow::cb_Update3DView(void) {

  if (ExpShape->value()) {
    view3D->showSingleShape(puzzle, ShapeSelect->getSelection());
    view3D->showColors(puzzle, ColConst->value() == 1);
  } else if (ExpAssembly->value()) {
    view3D->showAssembly(puzzle, ProblemSelect->getSelection(), 0);
    view3D->showColors(puzzle, ColConst->value() == 1);
  } else if (ExpSolution->value()) {
    view3D->showAssembly(puzzle, ProblemSelect->getSelection(), 0);
    view3D->showColors(puzzle, ColConst->value() == 1);
  } else if (ExpProblem->value()) {
    view3D->showSingleShape(puzzle, puzzle->probGetResult(ProblemSelect->getSelection()));
    view3D->showColors(puzzle, ColConst->value() == 1);
  }
}

static void cb_ImageExportSzUpdate_stub(Fl_Widget *o, void *v) { ((ImageExportWindow*)(v))->cb_SzUpdate(); }
void ImageExportWindow::cb_SzUpdate(void) {

  if (SzA4Land->value()) {
    SzX->value("297");
    SzY->value("210");
  } else if (SzA4Port->value()) {
    SzX->value("210");
    SzY->value("297");
  } else if (SzLetterLand->value()) {
    SzX->value("279");
    SzY->value("216");
  } else if (SzLetterPort->value()) {
    SzX->value("216");
    SzY->value("279");
  }

  if (SzManual->value()) {
    SzX->activate();
    SzY->activate();
  } else {
    SzX->deactivate();
    SzY->deactivate();
  }

  if (SzX->value() && SzX->value()[0] && SzDPI->value() && SzDPI->value()[0]) {
    char tmp[20];
    snprintf(tmp, 20, "%i", int(atoi(SzDPI->value()) * atoi(SzX->value()) * 0.03937 + 0.5));
    SizePixelX->value(tmp);
  }

  if (SzY->value() && SzY->value()[0] && SzDPI->value() && SzDPI->value()[0]) {
    char tmp[20];
    snprintf(tmp, 20, "%i", int(atoi(SzDPI->value()) * atoi(SzY->value()) * 0.03937 + 0.5));
    SizePixelY->value(tmp);
  }
}

ImageExportWindow::ImageExportWindow(puzzle_c * p, const guiGridType_c * ggt) : puzzle(p), working(false), state(0), i(0) {

  label("Export Images");

  LFl_Frame *fr;

  {
    fr = new LFl_Frame(0, 0, 1, 2);

    BgWhite = new LFl_Radio_Button("White Background", 0, 0);
    BgTransp = new LFl_Radio_Button("Transparent Background", 0, 1);
    BgWhite->value(1);
    (new LFl_Box(0, 2))->weight(0, 1);
    fr->end();
  }

  {
    // the group that defines the supersampling
    fr = new LFl_Frame(0, 2);

    AA1 = new LFl_Radio_Button("No Antialiasing", 0, 0);
    AA2 = new LFl_Radio_Button("2x2 Supersampling", 0, 1);
    AA3 = new LFl_Radio_Button("3x3 Supersampling", 0, 2);
    AA3->value(1);
    AA4 = new LFl_Radio_Button("4x4 Supersampling", 0, 3);
    AA5 = new LFl_Radio_Button("5x5 Supersampling", 0, 4);

    {
      layouter_c * l = new layouter_c(0, 5, 1, 2);

      ColPiece = new LFl_Radio_Button("Use piece colors", 0, 5);
      ColConst = new LFl_Radio_Button("Use color constraint colors", 0, 6);

      ColPiece->value(1);

      ColPiece->callback(cb_ImageExport3DUpdate_stub, this);
      ColConst->callback(cb_ImageExport3DUpdate_stub, this);

      l->end();
    }

    DimStatic = new LFl_Check_Button("Dim static pieces", 0, 7);
    (new LFl_Box(0, 8))->weight(0, 1);
    fr->end();
  }

  {
    // user defined size input
    fr = new LFl_Frame(1, 1, 1, 2);

    int y = 0;

    SzA4Port = new LFl_Radio_Button("A4 Portrait", 0, y++, 5, 1);
    SzA4Port->callback(cb_ImageExportSzUpdate_stub, this);

    SzA4Land = new LFl_Radio_Button("A4 Landscape", 0, y++, 5, 1);
    SzA4Land->callback(cb_ImageExportSzUpdate_stub, this);

    SzLetterPort = new LFl_Radio_Button("Letter Portrait", 0, y++, 5, 1);
    SzLetterPort->callback(cb_ImageExportSzUpdate_stub, this);

    SzLetterLand = new LFl_Radio_Button("Letter Landscape", 0, y++, 5, 1);
    SzLetterLand->callback(cb_ImageExportSzUpdate_stub, this);

    SzManual = new LFl_Radio_Button("manual", 0, y++, 5, 1);
    SzManual->value(1);
    SzManual->callback(cb_ImageExportSzUpdate_stub, this);

    (new LFl_Box("Size X", 0, y))->stretchRight();
    SzX = new LFl_Input(2, y);
    SzX->callback(cb_ImageExportSzUpdate_stub, this);
    (new LFl_Box("mm", 4, y))->stretchLeft();
    (new LFl_Box(3, y))->setMinimumSize(5, 0);
    (new LFl_Box(1, y++))->setMinimumSize(5, 0);

    (new LFl_Box("Size Y", 0, y))->stretchRight();
    SzY = new LFl_Input(2, y);
    SzY->callback(cb_ImageExportSzUpdate_stub, this);
    (new LFl_Box("mm", 4, y++))->stretchLeft();

    (new LFl_Box("DPI", 0, y))->stretchRight();
    SzDPI = new LFl_Input(2, y++);
    SzDPI->value("300");
    SzDPI->callback(cb_ImageExportSzUpdate_stub, this);

    (new LFl_Box("Pixel X", 0, y))->stretchRight();
    SizePixelX = new LFl_Int_Input(2, y++);
    SizePixelX->value("300");
    SizePixelX->setMinimumSize(50, 0);

    (new LFl_Box("Pixel Y", 0, y))->stretchRight();
    SizePixelY = new LFl_Int_Input(2, y++);
    SizePixelY->value("300");

    (new LFl_Box(0, y))->weight(0, 1);

    fr->end();
  }

  {
    fr = new LFl_Frame(0, 3, 2, 1);

    (new LFl_Box("File name", 0, 0))->stretchLeft();
    (new LFl_Box("Path", 0, 1))->stretchLeft();
    (new LFl_Box("Number of files", 0, 2, 3, 1))->stretchLeft();
    (new LFl_Box("Number of images", 0, 3, 3, 1))->stretchLeft();

    (new LFl_Box(1, 0))->setMinimumSize(5, 0);
    (new LFl_Box(3, 0))->setMinimumSize(5, 0);

    Fname = new LFl_Input(2, 0, 3, 1);
    Fname->value("test");
    Fname->weight(1, 0);
    Pname = new LFl_Input(2, 1, 3, 1);
    NumPages = new LFl_Int_Input(4, 2);
    new LFl_Int_Input(4, 3);

    fr->end();
  }

  {
    // create the radio buttons that select what of the current puzzle file to
    // export and enable only those of the possibilites that are available in
    // the current puzzle

    fr = new LFl_Frame(0, 4, 2, 1);

    ExpShape = new LFl_Radio_Button("Export Shape", 0, 0);
    ExpProblem = new LFl_Radio_Button("Export Problem", 0, 1);
    ExpAssembly = new LFl_Radio_Button("Export Assembly", 0, 2);
    ExpSolution = new LFl_Radio_Button("Export Solution", 0, 3);

    bool assemblies = false;
    bool solutions = false;

    for (unsigned int i = 0; i < puzzle->problemNumber(); i++) {
      for (unsigned int j = 0; j < puzzle->probSolutionNumber(i); j++) {
        if (puzzle->probGetAssembly(i, j)) assemblies = true;
        if (puzzle->probGetDisassembly(i, j)) solutions = true;
        if (assemblies || solutions) break;
      }
      if (assemblies || solutions) break;
    }

    if (puzzle->shapeNumber() == 0)   ExpShape->deactivate();     else ExpShape->setonly();
    if (puzzle->problemNumber() == 0) ExpProblem->deactivate();   else ExpProblem->setonly();
    if (!assemblies)                  ExpAssembly->deactivate();  else ExpAssembly->setonly();
    if (!solutions)                   ExpSolution->deactivate();  else ExpSolution->setonly();

    (new LFl_Box(0, 4))->weight(0, 1);

    ExpShape->callback(cb_ImageExport3DUpdate_stub, this);
    ExpProblem->callback(cb_ImageExport3DUpdate_stub, this);
    ExpAssembly->callback(cb_ImageExport3DUpdate_stub, this);
    ExpSolution->callback(cb_ImageExport3DUpdate_stub, this);

    fr->end();
  }

  {
    layouter_c * l = new layouter_c(0, 5, 2, 1);

    ShapeSelect = new PieceSelector(0, 0, 20, 20, puzzle);
    ProblemSelect = new ProblemSelector(0, 0, 20, 20, puzzle);

    ShapeSelect->setSelection(0);
    ProblemSelect->setSelection(0);

    Fl_Group * gr = new LBlockListGroup(0, 0, 1, 1, ShapeSelect);
    gr->callback(cb_ImageExport3DUpdate_stub, this);

    gr = new LBlockListGroup(1, 0, 1, 1, ProblemSelect);
    gr->callback(cb_ImageExport3DUpdate_stub, this);

    l->end();
  }

  {
    layouter_c * l = new layouter_c(0, 6, 3, 1);

    status = new LFl_Box();
    status->weight(1, 0);
    status->label("Test");
    status->pitch(7);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    BtnStart = new LFl_Button("Export Image(s)", 1, 0);
    BtnStart->pitch(7);
    BtnStart->callback(cb_ImageExportExport_stub, this);
    BtnStart->box(FL_THIN_UP_BOX);

    BtnAbbort = new LFl_Button("Abort", 2, 0);
    BtnAbbort->pitch(7);
    BtnAbbort->callback(cb_ImageExportAbort_stub, this);
    BtnAbbort->box(FL_THIN_UP_BOX);

    l->end();
  }

  view3D = new LView3dGroup(2, 0, 1, 6, ggt);
  cb_Update3DView();

  set_modal();
}

void ImageExportWindow::update(void) {
  if (working) {
    BtnStart->deactivate();
    BtnAbbort->deactivate();
    view3D->deactivate();
    view3D->redraw();
  } else {
    BtnStart->activate();
    BtnAbbort->activate();
    view3D->activate();

    if (state == 3) {
      cb_Update3DView();
      view3D->getView()->invalidate();
      view3D->redraw();
      state = 4;
    }
  }
}

