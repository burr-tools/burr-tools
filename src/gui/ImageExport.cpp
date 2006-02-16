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
#include "pieceColor.h"
#include "VoxelDrawer.h"
#include "Image.h"

#include <FL/Fl.h>

#include <vector>

class ImageInfo {

  private:

    ShadowView * dr;
    Image * i;  // image generated with the drawer that is with a fixed hight and the required width

  public:

    ImageInfo(ShadowView * d) : dr(d) {
      i = new Image(600, 200, dr);
      i->transparentize(255, 255, 255);
      i->minimizeWidth(10);
    }

    double ratio(void) { return ((double)(i->w()))/i->h(); }

    Image * generateImage(unsigned int w, unsigned int h, unsigned char aa) {
      Image * i = new Image ((h*3)*aa, h*aa, dr);

      i->transparentize(255, 255, 255);
      i->minimizeWidth(aa*h/20, aa);
      i->scaleDown(aa);

      return i;
    }

};

static void cb_ImageExportAbort_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Abort(); }
void ImageExportWindow::cb_Abort(void) {
  hide();
}

static void cb_ImageExportExport_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Export(); }
void ImageExportWindow::cb_Export(void) {

  status->label("Prepare for image");

  /* this vector contains all the information of all images that need to appear in the output */
  std::vector<ImageInfo*> images;

  glDrawBuffer(GL_BACK);
  glReadBuffer(GL_BACK);

  ShadowView dr(view3D->getView());

  // calculate antialiasing factor
  int aa = 1;
  if (AA2->value()) aa = 2;
  if (AA3->value()) aa = 3;
  if (AA4->value()) aa = 4;
  if (AA5->value()) aa = 5;


  if (ExpShape->value()) {
    ShadowView * dr = new ShadowView(view3D->getView());
    dr->showSingleShape(puzzle, ShapeSelect->getSelection(), ((int)ColConst->value()) == 1);
    dr->showColors(puzzle, ColConst->value() == 1);
    images.push_back(new ImageInfo(dr));
  } else if (ExpAssembly->value()) {
    ShadowView * dr = new ShadowView(view3D->getView());
    dr->showAssembly(puzzle, ProblemSelect->getSelection(), 0, ((int)ColConst->value()) == 1);
    dr->showColors(puzzle, ColConst->value() == 1);
    images.push_back(new ImageInfo(dr));
  } else if (ExpSolution->value()) {
    // renerate an image for each step (for the moment only for the last solution)
    unsigned int s = puzzle->probSolutionNumber(ProblemSelect->getSelection()) - 1;
    separation_c * t = puzzle->probGetDisassembly(ProblemSelect->getSelection(), s);
    if (!t) return;

    DisasmToMoves dtm(t, 20);

    for (unsigned int step = 0; step < t->sumMoves(); step++) {
      ShadowView * dr = new ShadowView(view3D->getView());
      dr->showAssembly(puzzle, ProblemSelect->getSelection(), s, false);
      dr->showColors(puzzle, ColConst->value() == 1);
      dtm.setStep(step);
      dr->updatePositions(&dtm);

      images.push_back(new ImageInfo(dr));
    }

  } else if (ExpProblem->value()) {
    // generate an image for each piece in the problem
    ShadowView * dr = new ShadowView(view3D->getView());
    dr->showSingleShape(puzzle, puzzle->probGetResult(ProblemSelect->getSelection()), ((int)ColConst->value()) == 1);
    dr->showColors(puzzle, ColConst->value() == 1);
    images.push_back(new ImageInfo(dr));

    for (unsigned int p = 0; p < puzzle->probShapeNumber(ProblemSelect->getSelection()); p++) {
      ShadowView * dr = new ShadowView(view3D->getView());
      dr->showSingleShape(puzzle, puzzle->probGetShape(ProblemSelect->getSelection(), p), ((int)ColConst->value()) == 1);
      dr->showColors(puzzle, ColConst->value() == 1);
      images.push_back(new ImageInfo(dr));
    }

  } else
    return;

  // if we have less images than pages, lower pages
  unsigned int pages = atoi(NumPages->value());
  if (pages > images.size()) pages = images.size();

  // now find out in how many lines the images need to be put onto the pages to
  // get them all onto the available space

  unsigned int linesPerPage = 1;
  unsigned int pageHeight = atoi(SizePixelY->value());
  unsigned int pageWidth = atoi(SizePixelX->value());

  unsigned int curWidth = 0;
  unsigned int curLine = 0;
  unsigned int curPage = 0;

  unsigned int imgHeight = pageHeight / linesPerPage;

  while (true) {

    curWidth = 0;
    curLine = 0;
    curPage = 0;

    imgHeight = pageHeight / linesPerPage;

    // check, if everything fits with the current number of lines
    for (unsigned int im = 0; im < images.size(); im++) {
      // calculate width of the image when it has the current line hight
      unsigned int w = (unsigned int)(imgHeight * images[im]->ratio() + 0.9);

      if (curWidth + w < pageWidth) {
        // image fits onto the line
        curWidth += w;
      } else if (curWidth == 0) {
        // image is too wide for page, so force it onto the line
        curLine++;
        if (curLine >= linesPerPage) {
          curLine = 0;
          curPage++;
        }
      } else if (w > pageWidth) {
        // image is too wide, and another line had been stated, so finish that line
        // and put image into next line
        curLine++;
        if (curLine >= linesPerPage) {
          curLine = 0;
          curPage++;
        }
        curLine++;
        if (curLine >= linesPerPage) {
          curLine = 0;
          curPage++;
        }
        curWidth = 0;
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

  // ok, now lets output

  curWidth = 0;
  curLine = 0;
  curPage = 0;

  imgHeight = pageHeight / linesPerPage;

  status->label("Create image");

  Image *i;

  if (BgWhite->value()) {
    i = new Image(pageWidth, pageHeight, 255, 255, 255, 255);
  } else {
    i = new Image(pageWidth, pageHeight, 0, 0, 0, 0);
  }

  // check, if everything fits with the current number of lines
  for (unsigned int im = 0; im < images.size(); im++) {
    // calculate width of the image when it has the current line hight
    unsigned int w = (unsigned int)(imgHeight * images[im]->ratio() + 0.9);

    Image *i2 = images[im]->generateImage(w, imgHeight, aa);

    if (curWidth + w < pageWidth) {
      // image fits onto the line
      i->blit(i2, curWidth, curLine * imgHeight);
      curWidth += w;
    } else if (curWidth == 0) {
      i->blit(i2, 0, curLine * imgHeight);
      // image is too wide for page, so force it onto the line
      curLine++;
      if (curLine >= linesPerPage) {
        curLine = 0;
        {
          char name[1000];

          if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/')
            snprintf(name, 1000, "%s/%s%03i.png", Pname->value(), Fname->value(), curPage);
          else
            snprintf(name, 1000, "%s%s%03i.png", Pname->value(), Fname->value(), curPage);


          status->label("Saving");
          i->saveToPNG(name);
          delete i;
          if (BgWhite->value()) {
            i = new Image(pageWidth, pageHeight, 255, 255, 255, 255);
          } else {
            i = new Image(pageWidth, pageHeight, 0, 0, 0, 0);
          }
        }
        curPage++;
      }
    } else if (w > pageWidth) {
      // image is too wide, and another line had been stated, so finish that line
      // and put image into next line
      curLine++;
      if (curLine >= linesPerPage) {
        curLine = 0;
        {
          char name[1000];

          if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/')
            snprintf(name, 1000, "%s/%s%03i.png", Pname->value(), Fname->value(), curPage);
          else
            snprintf(name, 1000, "%s%s%03i.png", Pname->value(), Fname->value(), curPage);


          status->label("Saving");
          i->saveToPNG(name);
          delete i;
          if (BgWhite->value()) {
            i = new Image(pageWidth, pageHeight, 255, 255, 255, 255);
          } else {
            i = new Image(pageWidth, pageHeight, 0, 0, 0, 0);
          }
        }
        curPage++;
      }
      i->blit(i2, 0, curLine * imgHeight);
      curLine++;
      if (curLine >= linesPerPage) {
        curLine = 0;
        {
          char name[1000];

          if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/')
            snprintf(name, 1000, "%s/%s%03i.png", Pname->value(), Fname->value(), curPage);
          else
            snprintf(name, 1000, "%s%s%03i.png", Pname->value(), Fname->value(), curPage);


          status->label("Saving");
          i->saveToPNG(name);
          delete i;
          if (BgWhite->value()) {
            i = new Image(pageWidth, pageHeight, 255, 255, 255, 255);
          } else {
            i = new Image(pageWidth, pageHeight, 0, 0, 0, 0);
          }
        }
        curPage++;
      }
      curWidth = 0;
    } else {
      // image on the next line
      curWidth = w;
      curLine++;
      if (curLine >= linesPerPage) {
        curLine = 0;
        {
          char name[1000];

          if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/')
            snprintf(name, 1000, "%s/%s%03i.png", Pname->value(), Fname->value(), curPage);
          else
            snprintf(name, 1000, "%s%s%03i.png", Pname->value(), Fname->value(), curPage);


          status->label("Saving");
          i->saveToPNG(name);
          delete i;
          if (BgWhite->value()) {
            i = new Image(pageWidth, pageHeight, 255, 255, 255, 255);
          } else {
            i = new Image(pageWidth, pageHeight, 0, 0, 0, 0);
          }
        }
        curPage++;
      }
      i->blit(i2, 0, curLine * imgHeight);
    }

    delete i2;
  }

  {
    char name[1000];

    if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/')
      snprintf(name, 1000, "%s/%s%03i.png", Pname->value(), Fname->value(), curPage);
    else
      snprintf(name, 1000, "%s%s%03i.png", Pname->value(), Fname->value(), curPage);


    status->label("Saving");
    i->saveToPNG(name);
    delete i;
  }

  glDrawBuffer(GL_FRONT);
  glReadBuffer(GL_FRONT);

  status->label(0);
  hide();
}

static void cb_ImageExport3DUpdate_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Update3DView(); }
void ImageExportWindow::cb_Update3DView(void) {

  if (ExpShape->value()) {
    view3D->showSingleShape(puzzle, ShapeSelect->getSelection(), ((int)ColConst->value()) == 1);
    view3D->showColors(puzzle, ColConst->value() == 1);
  } else if (ExpAssembly->value()) {
    view3D->showAssembly(puzzle, ProblemSelect->getSelection(), 0, ((int)ColConst->value()) == 1);
    view3D->showColors(puzzle, ColConst->value() == 1);
  } else if (ExpSolution->value()) {
    view3D->showAssembly(puzzle, ProblemSelect->getSelection(), 0, ((int)ColConst->value()) == 1);
    view3D->showColors(puzzle, ColConst->value() == 1);
  } else if (ExpProblem->value()) {
    view3D->showSingleShape(puzzle, puzzle->probGetResult(ProblemSelect->getSelection()), ((int)ColConst->value()) == 1);
    view3D->showColors(puzzle, ColConst->value() == 1);
  }
}

ImageExportWindow::ImageExportWindow(puzzle_c * p) : puzzle(p) {

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

    new LFl_Check_Button("Dim static pieces", 0, 7);
    (new LFl_Box(0, 8))->weight(0, 1);
    fr->end();
  }

  {
    // user defined size input
    fr = new LFl_Frame(1, 1, 1, 2);

    int y = 0;

    new LFl_Radio_Button("A4 Portrait", 0, y++, 5, 1);
    new LFl_Radio_Button("A4 Landscape", 0, y++, 5, 1);
    new LFl_Radio_Button("Legal Portrait", 0, y++, 5, 1);
    new LFl_Radio_Button("Legal Landscape", 0, y++, 5, 1);
    (new LFl_Radio_Button("manual", 0, y++, 5, 1))->value(1);

    (new LFl_Box("Size X", 0, y))->stretchRight();
    new LFl_Input(2, y);
    (new LFl_Box("mm", 4, y))->stretchLeft();
    (new LFl_Box(3, y))->setMinimumSize(5, 0);
    (new LFl_Box(1, y++))->setMinimumSize(5, 0);

    (new LFl_Box("Size Y", 0, y))->stretchRight();
    new LFl_Input(2, y);
    (new LFl_Box("mm", 4, y++))->stretchLeft();

    (new LFl_Box("DPI", 0, y))->stretchRight();
    new LFl_Input(2, y++);

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

    LFl_Button *b;

    status = new LFl_Box();
    status->weight(1, 0);
    status->label("Test");
    status->pitch(7);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    b = new LFl_Button("Export Image(s)", 1, 0);
    b->pitch(7);
    b->callback(cb_ImageExportExport_stub, this);
    b->box(FL_THIN_UP_BOX);

    b = new LFl_Button("Abort", 2, 0);
    b->pitch(7);
    b->callback(cb_ImageExportAbort_stub, this);
    b->box(FL_THIN_UP_BOX);

    l->end();
  }

  view3D = new LView3dGroup(2, 0, 1, 6);
  cb_Update3DView();
}

