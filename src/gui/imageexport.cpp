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
#include "imageexport.h"

#include "image.h"
#include "view3dgroup.h"
#include "Layouter.h"
#include "blocklistgroup.h"

#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/disassembly.h"
#include "../lib/disasmtomoves.h"
#include "../lib/solution.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#pragma GCC diagnostic pop

#include <stdlib.h>
#include <stdio.h>

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
    voxelFrame_c::colorMode showColors;

    // parameters for assembly
    unsigned int problem;
    unsigned int solution;
    bool dim;

    disasmToMoves_c * positions;

    // the image data
    image_c * i;  // image generated with the drawer that is with a fixed hight and the required width
    image_c * i2; // the final image
    unsigned int i2aa;

    /* the openGL context to draw to */
    voxelFrame_c * vv;

  public:

    /* create image info to create a single shape image */
    ImageInfo(puzzle_c * p, voxelFrame_c::colorMode color,
        unsigned int s, voxelFrame_c * v) : setupFunction(SHOW_SINGLE), puzzle(p),
                                          shape(s), showColors(color),
                                          i(new image_c(600, 200)), i2(0), vv(v) { }

    /* image info for an assembly, if you don't give pos, you will get the standard assembly with
     * no piece shifted
     */
    ImageInfo(puzzle_c * p, voxelFrame_c::colorMode color, unsigned int prob,
        unsigned int sol, voxelFrame_c * v,
        disasmToMoves_c * pos = 0, bool d = false) : setupFunction(SHOW_ASSEMBLY), puzzle(p),
                                                   showColors(color), problem(prob),
                                                   solution(sol), dim(d), positions(pos),
                                                   i(new image_c(600, 200)),
                                                   i2(0), vv(v) { }

    ~ImageInfo() {
      if (i) delete i;
      if (i2) delete i2;
    }

    /* set up the voxelFrame_c so that is shows the information for this image */
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
    image_c * getImage(void);
};

void ImageInfo::setupContent(void) {

  switch (setupFunction) {
    case SHOW_SINGLE:
      vv->showSingleShape(puzzle, shape);
      vv->showColors(puzzle, showColors);

      break;
    case SHOW_ASSEMBLY:
      vv->showAssembly(puzzle->getProblem(problem), solution);
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
    i->minimizeWidth(0);

    return false;
  } else
    return true;
}

/* start a new image */
void ImageInfo::generateImage(unsigned int /*w*/, unsigned int h, unsigned char aa) {
  if (i2)
    delete i2;
  i2 = new image_c ((h*3)*aa, h*aa);
  i2aa = aa;
}

/* prepare for a new tile of the image */
void ImageInfo::prepareImage(void) {

  setupContent();
  i2->prepareOpenGlImagePart(vv);
  glClearColor(1, 1, 1, 0);
}

/* get a new tile for the image */
image_c * ImageInfo::getImage(void) {

  if (!i2->getOpenGlImagePart()) {

    i2->transparentize(255, 255, 255);
    i2->minimizeWidth(0, i2aa);
    i2->scaleDown(i2aa);

    return i2;
  } else
    return 0;
}





static void cb_ImageExportAbort_stub(Fl_Widget* /*o*/, void* v) { ((imageExport_c*)(v))->cb_Abort(); }
void imageExport_c::cb_Abort(void) {
  hide();
}

bool imageExport_c::PreDraw(void) {

  static char statText[50];

  switch(state) {
    case 0:

      snprintf(statText, 50, "create Preview image %u / %zu", im, images.size());
      status->label(statText);

      images[im]->preparePreviewImage();

      return true;

    case 1:

      if (!images[im]->imageStarted()) {

        snprintf(statText, 50, "create image %u / %zu", im, images.size());
        status->label(statText);

        unsigned int w = (unsigned int)(imgHeight * images[im]->ratio() + 0.9);

        // calculate anti-aliasing factor
        int aa = 1;
        if (AA2->value()) aa = 2;
        if (AA3->value()) aa = 3;
        if (AA4->value()) aa = 4;
        if (AA5->value()) aa = 5;

        images[im]->generateImage(w, imgHeight, aa);
      }

      images[im]->prepareImage();

      return true;
  }

  return false;
}

void imageExport_c::nextImage(bool finish) {

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
      i = new image_c(pageWidth, pageHeight, 255, 255, 255, 255);
    } else {
      i = new image_c(pageWidth, pageHeight, 0, 0, 0, 0);
    }
  }
}

void imageExport_c::PostDraw(void) {

  switch(state) {
    case 0:

      if (!images[im]->getPreviewImage()) {

        // finished preview image, next
        im++;

        // all preview images generated?
        if (im >= images.size()) {

          // now find out in how many lines the images need to be put onto the pages to
          // get them all onto the available space

          unsigned int pageWidth = atoi(SizePixelX->value());
          imgHeight = atoi(SizePixelY->value());

          // if we have less images than pages, lower pages
          unsigned int pages = atoi(NumPages->value());
          if (pages == 0) pages = 1;
          if (pages > images.size()) pages = images.size();

          while (true) {

            curWidth = 0;
            curLine = 0;
            curPage = 0;

            unsigned int linesPerPage = atoi(SizePixelY->value()) / imgHeight;

            // check, if everything fits with the current number of lines
            for (unsigned int im = 0; im < images.size(); im++) {
              // calculate width of the image when it has the current line hight
              unsigned int w = (unsigned int)(imgHeight * images[im]->ratio() + 0.9);

              if (curWidth + w < pageWidth || (curWidth == 0)) {
                // image fits onto the line
                curWidth += w + pageWidth/60;

              } else {
                // image on the next line
                curWidth = w + pageWidth/60;
                curLine++;
                if (curLine >= linesPerPage) {
                  curLine = 0;
                  curPage++;
                }
              }
            }

            // check if we fit
            if ((curPage < pages) || ((curPage == pages) && (curLine == 0) && (curWidth == 0)))
              break;

            imgHeight--;
          }

          // OK, now lets setup the variable for output
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

      image_c * i2 = images[im]->getImage();

      if (i2) {

        unsigned int pageWidth = atoi(SizePixelX->value());
        unsigned int linesPerPage = atoi(SizePixelY->value()) / imgHeight;

        unsigned int w = i2->w();
        if (curWidth + w < pageWidth || (curWidth == 0)) {
          // image fits onto the line
          i->blit(i2, curWidth, curLine * imgHeight);
          curWidth += w + pageWidth/60;
        } else {
          // image on the next line
          curWidth = w + pageWidth/60;
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


static void cb_ImageExportExport_stub(Fl_Widget* /*o*/, void* v) { ((imageExport_c*)(v))->cb_Export(); }
void imageExport_c::cb_Export(void) {

  /* this vector contains all the information of all images that need to appear in the output */
  images.clear();

  if (ExpShape->value()) {

    images.push_back(new ImageInfo(puzzle, getColorMode(),
        ShapeSelect->getSelection(), view3D->getView()));

  } else if (ExpAssembly->value()) {

    images.push_back(new ImageInfo(puzzle, getColorMode(),
        ProblemSelect->getSelection(), 0, view3D->getView()));

  } else if (ExpSolutionDisassm->value()) {

    unsigned int prob = ProblemSelect->getSelection();
    problem_c * pr = puzzle->getProblem(prob);

    // generate an image for each step (for the moment only for the last solution)
    unsigned int s = pr->getNumberOfSavedSolutions() - 1;
    separation_c * t = pr->getSavedSolution(s)->getDisassembly();
    if (!t) return;

    for (unsigned int step = 0; step < t->sumMoves(); step++) {
      disasmToMoves_c * dtm = new disasmToMoves_c(t, 20, pr->getNumberOfPieces());
      dtm->setStep(step, false, true);
      images.push_back(new ImageInfo(puzzle, getColorMode(),
           prob, s, view3D->getView(), dtm, DimStatic->value()));
    }

  } else if (ExpSolution->value()) {

    unsigned int prob = ProblemSelect->getSelection();
    problem_c * pr = puzzle->getProblem(prob);

    // generate an image for each step (for the moment only for the last solution)
    unsigned int s = pr->getNumberOfSavedSolutions() - 1;
    separation_c * t = pr->getSavedSolution(s)->getDisassembly();
    if (!t) return;

    for (unsigned int step = t->sumMoves() - 1; step > 0; step--) {
      disasmToMoves_c * dtm = new disasmToMoves_c(t, 20, pr->getNumberOfPieces());
      dtm->setStep(step, false, true);
      images.push_back(new ImageInfo(puzzle, getColorMode(),
           prob, s, view3D->getView(), dtm, DimStatic->value()));
    }

    disasmToMoves_c * dtm = new disasmToMoves_c(t, 20, pr->getNumberOfPieces());
    dtm->setStep(0, false, true);
    images.push_back(new ImageInfo(puzzle, getColorMode(),
          prob, s, view3D->getView(), dtm, false));

  } else if (ExpProblem->value()) {
    // generate an image for each piece in the problem
    unsigned int prob = ProblemSelect->getSelection();
    problem_c * pr = puzzle->getProblem(prob);

    if (pr->resultValid())
      images.push_back(new ImageInfo(puzzle, getColorMode(),
            pr->getResultId(), view3D->getView()));

    for (unsigned int p = 0; p < pr->getNumberOfParts(); p++)
      images.push_back(new ImageInfo(puzzle, getColorMode(),
            pr->getShapeIdOfPart(p), view3D->getView()));

  } else

    return;

  im = 0;
  working = true;
  BtnStart->deactivate();
  BtnAbbort->deactivate();
  state = 0;

  view3D->getView()->setCallback(this);
}

static void cb_ImageExport3DUpdate_stub(Fl_Widget* /*o*/, void* v) { ((imageExport_c*)(v))->cb_Update3DView(); }
void imageExport_c::cb_Update3DView(void) {

  bool assemblies = false;
  bool solutions = false;

  unsigned int prob = ProblemSelect->getSelection();
  problem_c * pr = puzzle->getProblem(prob);

  if (prob < puzzle->getNumberOfProblems())
    if (pr->getNumberOfSavedSolutions() > 0) {
      if (pr->getSavedSolution(0)->getAssembly()) assemblies = true;
      if (pr->getSavedSolution(0)->getDisassembly()) solutions = true;
    }

  if (!solutions) {
    ExpSolutionDisassm->deactivate();
    if (ExpSolutionDisassm->value())
        ExpAssembly->setonly();
  } else
    ExpSolutionDisassm->activate();
  if (!solutions) {
    ExpSolution->deactivate();
    if (ExpSolution->value())
      ExpAssembly->setonly();
  } else
    ExpSolution->activate();
  if (!assemblies) {
    ExpAssembly->deactivate();
    if (ExpAssembly->value())
      ExpProblem->setonly();
  } else
    ExpAssembly->activate();
  if (puzzle->getNumberOfProblems() == 0) {
    ExpProblem->deactivate();
    if (ExpProblem->value())
      ExpShape->setonly();
  } else
    ExpProblem->activate();
  if (puzzle->getNumberOfShapes() == 0)
    ExpShape->deactivate();
  else
    ExpShape->activate();

  if (ExpShape->value()) {
    view3D->getView()->showSingleShape(puzzle, ShapeSelect->getSelection());
  } else if (ExpAssembly->value()) {
    view3D->getView()->showAssembly(puzzle->getProblem(ProblemSelect->getSelection()), 0);
  } else if (ExpSolution->value()) {
    view3D->getView()->showAssembly(puzzle->getProblem(ProblemSelect->getSelection()), 0);
  } else if (ExpSolutionDisassm->value()) {
    view3D->getView()->showAssembly(puzzle->getProblem(ProblemSelect->getSelection()), 0);
  } else if (ExpProblem->value() && pr->resultValid()) {
    view3D->getView()->showSingleShape(puzzle, pr->getResultId());
  }
  view3D->getView()->showColors(puzzle, getColorMode());
}

static void cb_ImageExportSzUpdate_stub(Fl_Widget * /*o*/, void *v) { ((imageExport_c*)(v))->cb_SzUpdate(); }
void imageExport_c::cb_SzUpdate(void) {

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

imageExport_c::imageExport_c(puzzle_c * p) : LFl_Double_Window(false), puzzle(p), working(false), state(0), i(0) {

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
    // the group that defines the super sampling
    fr = new LFl_Frame(0, 2);

    AA1 = new LFl_Radio_Button("No Antialiasing", 0, 0);
    AA2 = new LFl_Radio_Button("2x2 Supersampling", 0, 1);
    AA3 = new LFl_Radio_Button("3x3 Supersampling", 0, 2);
    AA3->value(1);
    AA4 = new LFl_Radio_Button("4x4 Supersampling", 0, 3);
    AA5 = new LFl_Radio_Button("5x5 Supersampling", 0, 4);

    {
      layouter_c * l = new layouter_c(0, 5, 1, 2);

      ColPiece = new LFl_Radio_Button("Use piece colours", 0, 5);
      ColConst = new LFl_Radio_Button("Use colour constraint colours", 0, 6);

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
    // export and enable only those of the possibilities that are available in
    // the current puzzle

    fr = new LFl_Frame(0, 4, 2, 1);

    ExpShape = new LFl_Radio_Button("Export Shape", 0, 0);
    ExpProblem = new LFl_Radio_Button("Export Problem", 0, 1);
    ExpAssembly = new LFl_Radio_Button("Export Assembly", 0, 2);
    ExpSolution = new LFl_Radio_Button("Export Solution (Assembly)", 0, 3);
    ExpSolutionDisassm = new LFl_Radio_Button("Export Solution (Disassembly)", 0, 4);
    ExpSolution->setonly();

    (new LFl_Box(0, 5))->weight(0, 1);

    ExpShape->callback(cb_ImageExport3DUpdate_stub, this);
    ExpProblem->callback(cb_ImageExport3DUpdate_stub, this);
    ExpAssembly->callback(cb_ImageExport3DUpdate_stub, this);
    ExpSolution->callback(cb_ImageExport3DUpdate_stub, this);
    ExpSolutionDisassm->callback(cb_ImageExport3DUpdate_stub, this);

    fr->end();
  }

  {
    layouter_c * l = new layouter_c(0, 5, 2, 1);

    ShapeSelect = new PieceSelector(0, 0, 20, 20, puzzle);
    ProblemSelect = new ProblemSelector(0, 0, 20, 20, puzzle);

    ShapeSelect->setSelection(0);
    ProblemSelect->setSelection(0);

    LBlockListGroup_c * gr = new LBlockListGroup_c(0, 0, 1, 1, ShapeSelect);
    gr->callback(cb_ImageExport3DUpdate_stub, this);
    gr->setMinimumSize(200, 100);

    gr = new LBlockListGroup_c(1, 0, 1, 1, ProblemSelect);
    gr->callback(cb_ImageExport3DUpdate_stub, this);
    gr->setMinimumSize(200, 100);

    l->end();
  }

  {
    layouter_c * l = new layouter_c(0, 6, 3, 1);

    status = new LFl_Box();
    status->weight(1, 0);
    status->label("Test");
    status->pitch(7);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    BtnStart = new LFl_Button("Export image(s)", 1, 0);
    BtnStart->pitch(7);
    BtnStart->callback(cb_ImageExportExport_stub, this);

    BtnAbbort = new LFl_Button("Abort", 2, 0);
    BtnAbbort->pitch(7);
    BtnAbbort->callback(cb_ImageExportAbort_stub, this);

    l->end();
  }

  view3D = new LView3dGroup(2, 0, 1, 6);
  view3D->setMinimumSize(400, 400);
  cb_Update3DView();

  set_modal();
}

void imageExport_c::update(void) {
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

