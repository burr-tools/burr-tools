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
#include "Image.h"

#include "../lib/bt_assert.h"

#include <png.h>
#include <stdio.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#elif WIN32
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#endif

Image::Image(unsigned int w, unsigned int h, unsigned char r, unsigned char g, unsigned char b, unsigned char a) : width(w), height(h) {
  bitmap = new unsigned char[w*h*4];
  for (unsigned int x = 0; x < w*h; x++) {
    bitmap[4*x+0] = r;
    bitmap[4*x+1] = g;
    bitmap[4*x+2] = b;
    bitmap[4*x+3] = a;
  }
}

Image::Image(unsigned int w, unsigned int h) : width(w), height(h), tr(0) {
  bitmap = new unsigned char[w*h*4];
}

Image::Image(unsigned int w, unsigned int h, unsigned char *b) : width(w), height(h), bitmap(b) { }

void Image::prepareOpenGlImagePart(VoxelView * dr) {

  if (!tr) {
    tr = trNew();

    trTileSize(tr, dr->w(), dr->h(), 0);
    trImageSize(tr, width, height);
    trPerspective(tr, 25, (double)width/height, dr->getSize(), dr->getSize()+100);

    trImageBuffer(tr, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
  }

  glClearColor(1, 1, 1, 0);

  trBeginTile(tr);
}

bool Image::getOpenGlImagePart(void) {

  int result = trEndTile(tr);

  if (!result) {
    // we finished

    // now flip vertically
    for (unsigned int y = 0; y < height/2; y++)
      for (unsigned int x = 0; x < width*4; x++) {
        unsigned char tmp = bitmap[y*4*width+x];
        bitmap[y*4*width+x] = bitmap[(height-y-1)*4*width+x];
        bitmap[(height-y-1)*4*width+x] = tmp;
      }

    trDelete(tr);
    tr = 0;
  }

  return (result != 0);
}


Image::~Image(void) {
  delete [] bitmap;
}

int Image::saveToPNG(const char * fname) const {

  int sx = width;
  int sy = height;
  unsigned char * buffer = bitmap;

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
  info_ptr->color_type = PNG_COLOR_TYPE_RGB_ALPHA;
  info_ptr->interlace_type = PNG_INTERLACE_NONE;
  info_ptr->valid = 0;

  png_write_info(png_ptr, info_ptr);

  /* Save the picture: */

  png_rows = new unsigned char*[sy]; //     (unsigned char **)(malloc(sizeof(char *) * sy));
  for (y = 0; y < sy; y++)
  {
    png_rows[y] = new unsigned char[4*sx];// (unsigned char*)malloc(sizeof(char) * 3 * sx);

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
    delete [] png_rows[y]; //free(png_rows[y]);

  delete [] png_rows; //free(png_rows);

  png_write_end(png_ptr, NULL);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fi);

  return 1;
}

void Image::blit(const Image * i, int xpos, int ypos) {
  for (unsigned int x = 0; x < i->width; x++)
    for (unsigned int y = 0; y < i->height; y++)
      if ((x+xpos >= 0) && (x+xpos < width) && (y+ypos >= 0) && (y+ypos < height)) {
        unsigned char r1,r2, g1, g2, b1, b2, a1, a2;

        r1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 0];
        r2 = i->bitmap[(y*i->width+x) * 4 + 0];

        g1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 1];
        g2 = i->bitmap[(y*i->width+x) * 4 + 1];

        b1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 2];
        b2 = i->bitmap[(y*i->width+x) * 4 + 2];

        a1 = bitmap[((y+ypos)*width + (x+xpos)) * 4 + 3];
        a2 = i->bitmap[(y*i->width+x) * 4 + 3];

        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 0] = (r1 * (255-a2) + r2 * a2) / 255;
        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 1] = (g1 * (255-a2) + g2 * a2) / 255;
        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 2] = (b1 * (255-a2) + b2 * a2) / 255;

        bitmap[((y+ypos)*width + (x+xpos)) * 4 + 3] = (unsigned char)((1 - ((1-a1/255.0) * (1-a2/255.0))) * 255 + 0.5);
      }
}

void Image::deTransparentize(unsigned char r, unsigned char g, unsigned char b) {
  for (unsigned int x = 0; x < width; x++)
    for (unsigned int y = 0; y < height; y++)
      if (bitmap[4*(y*width + x) + 3] == 0) {
        bitmap[4*(y*width + x) + 0] = r;
        bitmap[4*(y*width + x) + 1] = g;
        bitmap[4*(y*width + x) + 2] = b;
        bitmap[4*(y*width + x) + 3] = 255;
      }
}

void Image::transparentize(unsigned char r, unsigned char g, unsigned char b) {
  for (unsigned int x = 0; x < width; x++)
    for (unsigned int y = 0; y < height; y++)
      if ((bitmap[4*(y*width + x) + 0] == r) &&
          (bitmap[4*(y*width + x) + 1] == g) &&
          (bitmap[4*(y*width + x) + 2] == b))
        bitmap[4*(y*width + x) + 3] = 0;
}

void Image::scaleDown(unsigned char by) {
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

      if (a) {
        nb[4*(y*nw+x) + 0] = 255 * r / a;
        nb[4*(y*nw+x) + 1] = 255 * g / a;
        nb[4*(y*nw+x) + 2] = 255 * b / a;
      } else {
        nb[4*(y*nw+x) + 0] = 0;
        nb[4*(y*nw+x) + 1] = 0;
        nb[4*(y*nw+x) + 2] = 0;
      }
      nb[4*(y*nw+x) + 3] = a / (by*by);
    }

  delete [] bitmap;
  bitmap = nb;

  height = nh;
  width = nw;
}

void Image::minimizeWidth(unsigned int border, unsigned int multiple) {

  unsigned int xmin = 0;

  // find the smallest used column
  do {

    bool allTran = true;
    // check column for a pixed that is not completely transparent
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

  // find the smallest used column
  do {

    bool allTran = true;
    // check column for a pixed that is not completely transparent
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

  delete [] bitmap;
  bitmap = nb;
  width = nw;
}
