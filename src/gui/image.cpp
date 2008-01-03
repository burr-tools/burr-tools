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
#include "image.h"
#include "voxelframe.h"

#include "../lib/bt_assert.h"

#include <png.h>
#include <stdio.h>

#include <FL/gl.h>

#define TILE_BORDER 0

image_c::image_c(unsigned int w, unsigned int h, unsigned char r, unsigned char g, unsigned char b, unsigned char a) : width(w), height(h), bitmap(new unsigned char[w*h*4*sizeof(GLubyte)]), tile(0), tr(0) {

  /* initialize image bitmap */
  for (unsigned int x = 0; x < w*h; x++) {
    bitmap[4*x+0] = r;
    bitmap[4*x+1] = g;
    bitmap[4*x+2] = b;
    bitmap[4*x+3] = a;
  }
}

void image_c::prepareOpenGlImagePart(voxelFrame_c * dr) {

  if (!tr) {

    /* we start a new image grepping, so initialise tile render context */

    tr = trNew();

    int tw = dr->w() & 0xFFFFFFF0;
    int th = dr->h() & 0xFFFFFFF0;

    tile = new GLubyte[tw*th*4];

    trTileSize(tr, tw, th, 0);
    trTileBuffer(tr, GL_RGBA, GL_UNSIGNED_BYTE, tile);
    trImageSize(tr, width, height);
    trRowOrder(tr, TR_TOP_TO_BOTTOM);

    // this call has to be the identical one as in voxelFrame_c::draw()
    trPerspective(tr, 15, 1.0*width/height, dr->getSize()+1, 3*dr->getSize()+1);

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
  }

  /* prepare camera for next tile */
  trBeginTile(tr);
}

bool image_c::getOpenGlImagePart(void) {

  /* grep the next tile */

  bool more = trEndTile(tr);

  int curColumn = trGet(tr, TR_CURRENT_COLUMN);
  int curTileWidth = trGet(tr, TR_CURRENT_TILE_WIDTH);
  int bytesPerImageRow = width*4;
  int bytesPerTileRow = (trGet(tr, TR_TILE_WIDTH)-2*TILE_BORDER) * 4;
  int xOffset = curColumn * bytesPerTileRow + bytesPerImageRow * trGet(tr, TR_CURRENT_ROW) * trGet(tr, TR_TILE_HEIGHT);
  int bytesPerCurrentTileRow = (curTileWidth-2*TILE_BORDER)*4*sizeof(GLubyte);
  int curTileHeight = trGet(tr, TR_CURRENT_TILE_HEIGHT);

  for (int i = 0; i < curTileHeight; i++) {
    memcpy(bitmap + i*bytesPerImageRow + xOffset, /* Dest */
        tile + i*bytesPerTileRow,              /* Src */
        bytesPerCurrentTileRow);               /* Byte count*/
  }

  if (!more) {

    /* we have finished all tiles, so delete the tile render context */
    trDelete(tr);
    tr = 0;
    delete [] tile;
    tile = 0;

    /* flip vertically, as the tile renderer generates an image that is bottom up */
    for (unsigned int y = 0; y < height/2; y++)
      for (unsigned int x = 0; x < width*4; x++) {
        unsigned char tmp = bitmap[y*4*width+x];
        bitmap[y*4*width+x] = bitmap[(height-y-1)*4*width+x];
        bitmap[(height-y-1)*4*width+x] = tmp;
      }
  }

  return more;
}


image_c::~image_c(void) {
  delete [] bitmap;

  /* if we delete the image while we are doing an openGl grep, the context
   * must be deleted, too
   */
  if (tr) trDelete(tr);
  if (tile) delete [] tile;
}

int image_c::saveToPNG(const char * fname) const {

  int sx = width;
  int sy = height;
  unsigned char * buffer = bitmap;

  png_structp png_ptr;
  png_infop info_ptr;
  unsigned char ** png_rows = 0;
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

    /* if we have already instantiated the png_rows, we need to free them */
    if (png_rows) {
      for (y = 0; y < sy; y++)
        delete [] png_rows[y];

      delete [] png_rows;
    }

    fclose(fi);
    png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
    fprintf(stderr, "\nError: Couldn't save the image!\n%s\n\n", fname);
    return 0;
  }

  png_init_io(png_ptr, fi);
  info_ptr->width = sx;
  info_ptr->height = sy;
  info_ptr->bit_depth = 8;
  info_ptr->color_type = PNG_COLOR_TYPE_RGB_ALPHA;
  info_ptr->interlace_type = PNG_INTERLACE_NONE;
  info_ptr->valid = 0;

  png_write_info(png_ptr, info_ptr);

  /* Save the picture: */

  png_rows = new unsigned char*[sy];
  for (y = 0; y < sy; y++)
  {
    png_rows[y] = new unsigned char[4*sx];

    for (x = 0; x < sx; x++)
    {
      png_rows[y][x * 4 + 0] = buffer[(y*sx+x)*4+0];
      png_rows[y][x * 4 + 1] = buffer[(y*sx+x)*4+1];
      png_rows[y][x * 4 + 2] = buffer[(y*sx+x)*4+2];
      png_rows[y][x * 4 + 3] = buffer[(y*sx+x)*4+3];
    }
  }

  png_write_image(png_ptr, png_rows);

  for (y = 0; y < sy; y++)
    delete [] png_rows[y];

  delete [] png_rows;

  png_write_end(png_ptr, NULL);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fi);

  return 1;
}

void image_c::blit(const image_c * i, int xpos, int ypos) {
  for (unsigned int x = 0; x < i->width; x++)
    for (unsigned int y = 0; y < i->height; y++)
      if (((int)x+xpos >= 0) && ((int)x+xpos < (int)width) && ((int)y+ypos >= 0) && ((int)y+ypos < (int)height)) {

        /* get the values of the 2 pixels to blend */
        unsigned char r1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 0];
        unsigned char g1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 1];
        unsigned char b1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 2];
        unsigned char a1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 3];

        unsigned char r2 = i->bitmap[(y*i->width+x) * 4 + 0];
        unsigned char g2 = i->bitmap[(y*i->width+x) * 4 + 1];
        unsigned char b2 = i->bitmap[(y*i->width+x) * 4 + 2];
        unsigned char a2 = i->bitmap[(y*i->width+x) * 4 + 3];

        /* calculate the new colour value: it is a blend of the old and the new one depending
         * on the alpha value of the new pixel
         */
        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 0] = (r1 * (255-a2) + r2 * a2) / 255;
        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 1] = (g1 * (255-a2) + g2 * a2) / 255;
        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 2] = (b1 * (255-a2) + b2 * a2) / 255;

        /* the new alpha value is the inverse product of the 2 old alpha values:
         * (1-a) = (1-a1) * (1-a2)
         * as the values are in range of 0..255 we need to scale them
         */
        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 3] = (unsigned char)((1 - ((1-a1/255.0) * (1-a2/255.0))) * 255 + 0.5);
      }
}

void image_c::deTransparentize(unsigned char r, unsigned char g, unsigned char b) {
  for (unsigned int x = 0; x < width; x++)
    for (unsigned int y = 0; y < height; y++)
      if (bitmap[4*(y*width + x) + 3] == 0) {
        bitmap[4*(y*width + x) + 0] = r;
        bitmap[4*(y*width + x) + 1] = g;
        bitmap[4*(y*width + x) + 2] = b;
        bitmap[4*(y*width + x) + 3] = 255;
      }
}

void image_c::transparentize(unsigned char r, unsigned char g, unsigned char b) {
  for (unsigned int x = 0; x < width; x++)
    for (unsigned int y = 0; y < height; y++)
      if ((bitmap[4*(y*width + x) + 0] == r) &&
          (bitmap[4*(y*width + x) + 1] == g) &&
          (bitmap[4*(y*width + x) + 2] == b))
        bitmap[4*(y*width + x) + 3] = 0;
}

void image_c::scaleDown(unsigned char by) {
  bt_assert(width % by == 0);
  bt_assert(height % by == 0);

  unsigned int nw = width / by;
  unsigned int nh = height / by;

  unsigned char * nb = new unsigned char [nw*nh*4];

  for (unsigned int x = 0; x < nw; x++)
    for (unsigned int y = 0; y < nh; y++) {
      unsigned int a = 0;
      unsigned int r = 0;
      unsigned int g = 0;
      unsigned int b = 0;

      for (unsigned int ax = 0; ax < by; ax++)
        for (unsigned int ay = 0; ay < by; ay++) {
          unsigned int pos = 4*((y*by+ay)*width + x*by+ax);
          if (bitmap[pos + 3]) {
            r += bitmap[pos + 0];
            g += bitmap[pos + 1];
            b += bitmap[pos + 2];
            a += bitmap[pos + 3];
          }
        }

      /* the new colour value is the average of all non transparent pixels
       * or 0 if all pixels were transparent
       */
      if (a) {
        nb[4*(y*nw+x) + 0] = 255 * r / a;
        nb[4*(y*nw+x) + 1] = 255 * g / a;
        nb[4*(y*nw+x) + 2] = 255 * b / a;
      } else {
        nb[4*(y*nw+x) + 0] = 0;
        nb[4*(y*nw+x) + 1] = 0;
        nb[4*(y*nw+x) + 2] = 0;
      }

      /* the new alpha value is the average of all alpha values */
      nb[4*(y*nw+x) + 3] = a / (by*by);
    }

  delete [] bitmap;
  bitmap = nb;

  height = nh;
  width = nw;
}

void image_c::minimizeWidth(unsigned int border, unsigned int multiple) {

  unsigned int xmin = 0;

  // find the smallest used column
  do {

    bool allTran = true;
    // check column for a pixel that is not completely transparent
    for (unsigned int y = 0; y < height; y++)
      if (bitmap[y*width*4+4*xmin+3] != 0) {
        allTran = false;
        break;
      }

    if (!allTran)
      break;

    xmin++;

  } while (xmin < width);

  unsigned int xmax = width-1;

  // find the largest used column
  do {

    bool allTran = true;
    // check column for a pixel that is not completely transparent
    for (unsigned int y = 0; y < height; y++)
      if (bitmap[y*width*4+4*xmax+3] != 0) {
        allTran = false;
        break;
      }

    if (!allTran)
      break;

    xmax--;

  } while (xmax > 0);

  // include the border
  if (xmin > border)
    xmin -= border;
  else
    xmin = 0;

  if (xmax + border + 1 < width)
    xmax += border;
  else
    xmax = width-1;

  if (xmin > xmax) {
    xmin = xmax = 0;
  }

  unsigned int nw = xmax-xmin+1;

  /* make the width a multiple of the given multiplier
   * by adding pixels left and right
   */
  if ((nw % multiple) != 0) {
    unsigned int add = multiple - (nw % multiple);

    if (xmin >= add/2) {
      xmin -= add/2;
      add -= add/2;
    } else {
      add -= xmin;
      xmin = 0;
    }

    xmax += add;

    nw = xmax-xmin+1;
  }

  // create new bitmap
  unsigned char * nb = new unsigned char[nw*height*4];

  // copy image information
  for (unsigned int y = 0; y < height; y++)
    for (unsigned int x = xmin; x <= xmax; x++) {
      nb[(y*nw+x-xmin)*4+0] = bitmap[(y*width+x)*4+0];
      nb[(y*nw+x-xmin)*4+1] = bitmap[(y*width+x)*4+1];
      nb[(y*nw+x-xmin)*4+2] = bitmap[(y*width+x)*4+2];
      nb[(y*nw+x-xmin)*4+3] = bitmap[(y*width+x)*4+3];
    }

  /* delete the old bitmap */
  delete [] bitmap;
  bitmap = nb;
  width = nw;
}
