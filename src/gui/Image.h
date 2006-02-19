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
#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "tr.h"
#include "VoxelView.h"

/* this class represents an bitmap image */
/* is always has rgb and alpha map
 */
class Image {

  private:

    unsigned width, height;
    unsigned char * bitmap;

  public:

    Image(unsigned int width, unsigned int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    Image(unsigned int width, unsigned int height, unsigned char *bitmap);
    Image(unsigned int width, unsigned int height, ShadowView * dr);

    ~Image(void);

    int saveToPNG(const char * fname) const;

    void blit(const Image * i, int xpos, int ypos);

    /* assigns a color to all transparent pixels */
    void deTransparentize(unsigned char r, unsigned char g, unsigned char b);

    void transparentize(unsigned char r, unsigned char g, unsigned char b);

    /* scales the image down by the given factor */
    void scaleDown(unsigned char by);

    /* removes pixels columns from left and right that are completely transparent
     * but leaves a border of the given amount of pixels
     * the resulting image will be a multiple of multiple wide
     */
    void minimizeWidth(unsigned int border, unsigned int multiple = 1);

    unsigned int w(void) { return width; }
    unsigned int h(void) { return height; }
};

#endif
