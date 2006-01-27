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

#include "tr.h"

#include <png.h>
#include <stdio.h>

#include <FL/Fl.h>

#define IMAGESIZE 512

class MyVoxelDrawer : public VoxelDrawer {

  private:

    VoxelView *vv;

  public:

    MyVoxelDrawer(VoxelView * v) : vv(v) {}

    virtual void addRotationTransformation(void) {
      vv->getArcBall()->addTransform();
    }
};

static int savePngImage(char * fname, int sx, int sy, GLubyte * buffer)
{
  png_structp png_ptr;
  png_infop info_ptr;
  unsigned char ** png_rows;
  int x, y;

  FILE *fi = fopen(fname, "wb");
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (png_ptr == NULL)
  {
    fclose(fi);
    png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
    fprintf(stderr, "\nError: Couldn't save the image!\n%s\n\n", fname);
    return 0;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL)
  {
    fclose(fi);
    png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
    fprintf(stderr, "\nError: Couldn't save the image!\n%s\n\n", fname);
    return 0;
  }

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    fclose(fi);
    png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
    fprintf(stderr, "\nError: Couldn't save the image!\n%s\n\n", fname);
    return 0;
  }

  png_init_io(png_ptr, fi);
  info_ptr->width = sx;
  info_ptr->height = sy;
  info_ptr->bit_depth = 8;
  info_ptr->color_type = PNG_COLOR_TYPE_RGB;
  info_ptr->interlace_type = PNG_INTERLACE_NONE;
  info_ptr->valid = 0;

  png_write_info(png_ptr, info_ptr);

  /* Save the picture: */

  png_rows = (unsigned char **)(malloc(sizeof(char *) * sy));
  for (y = 0; y < sy; y++)
  {
    png_rows[y] = (unsigned char*)malloc(sizeof(char) * 3 * sx);

    for (x = 0; x < sx; x++)
    {
      png_rows[y][x * 3 + 0] = buffer[(y*sx+x)*3+0];
      png_rows[y][x * 3 + 1] = buffer[(y*sx+x)*3+1];
      png_rows[y][x * 3 + 2] = buffer[(y*sx+x)*3+2];
    }
  }

  png_write_image(png_ptr, png_rows);

  for (y = 0; y < sy; y++)
    free(png_rows[y]);

  free(png_rows);

  png_write_end(png_ptr, NULL);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fi);

  return 1;
}


// make an antialiased blit of the src image to the dst image, the src image has size sw and sh and a line pitch sp
// the destination image dst has size dw and dh and line pitch dx
// destination position is px and py
// the image is scaled down by the given aa factor so that aa*aa pixels is src are averaged and put into dst
// src must be a multiple of aa and it must fit completely onto dst
// src image must be rgb, dst image rgba
// src is bottom to top and dst is in top to bottom order
// the key color is the color in the src image that is assumed to be transparent color
static void aaBlit(unsigned char * src, int sw, int sh, int sp, int aa, int keyr, int keyg, int keyb, unsigned char * dst, int dw, int dh, int dp, int px, int py) {

  // check size, must be multiple of antialiasing factor
  if (sw % aa != 0) return;
  if (sh % aa != 0) return;

  // picture must fit
  if (px + sw/aa >= dw) return;
  if (py + sh/aa >= dh) return;

  for(int x = 0; x < sw/aa; x++)
    for (int y = 0; y < sh/aa; y++) {
      int r, g, b, a;
      r = g = b = a = 0;
      for (int ax = 0; ax < aa; ax++)
        for (int ay = 0; ay < aa; ay++) {
          int pos = 3*(sp*(sh-y*aa+ay-1)+x*aa+ax);

          if ((src[pos+0] != keyr) || (src[pos+1] != keyg) || (src[pos+2] != keyb)) {
            r += src[pos+0];
            b += src[pos+1];
            g += src[pos+2];
          } else {
            a++;
          }
        }

      r /= aa*aa-a;
      g /= aa*aa-a;
      b /= aa*aa-a;

      int pos = 4*(dp*(py+y)+px+x);

      dst[pos+0] = r;
      dst[pos+1] = g;
      dst[pos+2] = b;
      dst[pos+3] = 255*a/(aa*aa);
    }
}




static void cb_ImageExportAbort_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Abort(); }
void ImageExportWindow::cb_Abort(void) {
  hide();
}

static void cb_ImageExportExport_stub(Fl_Widget* o, void* v) { ((ImageExportWindow*)(v))->cb_Export(); }
void ImageExportWindow::cb_Export(void) {

  glDrawBuffer(GL_BACK);
  glReadBuffer(GL_BACK);

  GLubyte *image = new GLubyte[IMAGESIZE * IMAGESIZE * 3];

  TRcontext *tr = trNew();

  trTileSize(tr, view3D->getView()->w(), view3D->getView()->h(), 0);
  trImageSize(tr, IMAGESIZE, IMAGESIZE);
  trImageBuffer(tr, GL_RGB, GL_UNSIGNED_BYTE, image);

  trPerspective(tr, 5 + view3D->getView()->getSize(), 1.0, 10, 1100);

  MyVoxelDrawer dr(view3D->getView());

  dr.showAssembly(puzzle, 0, 0, false);

  do {
    trBeginTile(tr);

    GLfloat LightAmbient[]= { 0.01f, 0.01f, 0.01f, 1.0f };
    GLfloat LightDiffuse[]= { 1.5f, 1.5f, 1.5f, 1.0f };
    GLfloat LightPosition[]= { 700.0f, 200.0f, -90.0f, 1.0f };

    GLfloat AmbientParams[] = {0.1, 0.1, 0.1, 1};
    GLfloat DiffuseParams[] = {0.7, 0.7, 0.7, 0.1};
    GLfloat SpecularParams[] = {0.4, 0.4, 0.4, 0.5};

    glEnable(GL_COLOR_MATERIAL);
    glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
    glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);
    glEnable(GL_LIGHT1);

    glMaterialf(GL_FRONT, GL_SHININESS, 0.5);
    glMaterialfv(GL_FRONT, GL_AMBIENT, AmbientParams);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, DiffuseParams);
    glMaterialfv(GL_FRONT, GL_SPECULAR, SpecularParams);

    glClearColor(1, 1, 1, 0);

    dr.drawData();

  } while (trEndTile(tr));

  trDelete(tr);

  savePngImage("test.png", IMAGESIZE, IMAGESIZE, image);

  delete [] image;

  hide();
}




ImageExportWindow::ImageExportWindow(puzzle_c * p) : puzzle(p) {
  LFl_Frame *fr;

  fr = new LFl_Frame(0, 0, 1, 2);
  new LFl_Round_Button("White Background", 0, 0);
  new LFl_Round_Button("Transparent Background", 0, 1);
  (new LFl_Box(0, 2))->weight(0, 1);
  fr->end();

  fr = new LFl_Frame(0, 2);
  new LFl_Round_Button("No Antialiasing", 0, 0);
  new LFl_Round_Button("2x2 Supersampling", 0, 1);
  new LFl_Round_Button("3x3 Supersampling", 0, 2);
  new LFl_Round_Button("4x4 Supersampling", 0, 3);
  new LFl_Round_Button("5x5 Supersampling", 0, 4);
  new LFl_Round_Button("Use piece colors", 0, 5);
  new LFl_Round_Button("Use color constraint colors", 0, 6);
  new LFl_Check_Button("Dim static pieces", 0, 7);
  (new LFl_Box(0, 8))->weight(0, 1);
  fr->end();

  // user defined size input
  fr = new LFl_Frame(1, 1, 1, 2);
  new LFl_Round_Button("A4 Portrait", 0, 0, 5, 1);
  new LFl_Round_Button("A4 Landscape", 0, 1, 5, 1);
  new LFl_Round_Button("manual", 0, 2, 5, 1);
  (new LFl_Box("Size X", 0, 3))->stretchRight();
  (new LFl_Box("Size Y", 0, 4))->stretchRight();
  (new LFl_Box("DPI", 0, 5))->stretchRight();
  (new LFl_Box("Pixel X", 0, 6))->stretchRight();
  (new LFl_Box("Pixel Y", 0, 7))->stretchRight();

  (new LFl_Box(1, 3))->setMinimumSize(2, 0);

  new LFl_Input(2, 3);
  new LFl_Input(2, 4);
  new LFl_Input(2, 5);
  new LFl_Input(2, 6);
  new LFl_Input(2, 7);

  (new LFl_Box(3, 3))->setMinimumSize(2, 0);

  new LFl_Box("mm", 4, 3);
  new LFl_Box("mm", 4, 4);

  (new LFl_Box(0, 8))->weight(0, 1);

  fr->end();

  fr = new LFl_Frame(0, 3, 2, 1);

  new LFl_Box("File name", 0, 0);
  new LFl_Box("Path", 0, 1);
  new LFl_Box("Number of files", 0, 2, 2, 1);
  new LFl_Box("Number of images", 0, 3, 2, 1);

  new LFl_Input(1, 0, 2, 1);
  new LFl_Input(1, 1, 2, 1);
  new LFl_Input(2, 2);
  new LFl_Input(2, 3);

  fr->end();

  fr = new LFl_Frame(0, 4, 2, 1);

  new LFl_Round_Button("Export Assembly", 0, 0);
  new LFl_Round_Button("Export Solution", 0, 1);

  fr->end();

  layouter_c * l = new layouter_c(0, 5, 3, 1);

  LFl_Button *b;

  (new LFl_Box())->weight(1, 0);

  b = new LFl_Button("Export Image(s)", 1, 0);
  b->pitch(7);
  b->callback(cb_ImageExportExport_stub, this);

  b = new LFl_Button("Abort", 2, 0);
  b->pitch(7);
  b->callback(cb_ImageExportAbort_stub, this);

  l->end();

  view3D = new LView3dGroup(2, 0, 1, 5);
//  view3D->showSingleShape(p, 0, false);
  view3D->showAssembly(p, 0, 0, false);

}

