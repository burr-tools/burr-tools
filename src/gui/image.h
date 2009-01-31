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
#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "tr.h"

class voxelFrame_c;

/* this class represents an bitmap image
 * that always has rgb and alpha map
 */
class image_c {

  private:

    /* size of the image */
    unsigned width, height;

    /* the image bitmap, for each pixel there are 4 unsigned chars
     * r, g, b and alpha, so the memory size for this array is 4*width*height
     */
    unsigned char * bitmap;

    GLubyte * tile;

    /* this structure is used, when an OpenGl image_c is accumulated, otherwise it's 0
     */
    TRcontext *tr;

  public:

    /* create new image and instantiate with the given colour */
    image_c(unsigned int width, unsigned int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

    /* create new image with given bitmap, this bitmap must be the right format */
    image_c(unsigned int w, unsigned int h, unsigned char *b) : width(w), height(h), bitmap(b), tile(0), tr(0) { }

    /* just create the required memory for an image of the given size */
    image_c(unsigned int w, unsigned int h) : width(w), height(h), bitmap(new unsigned char[w*h*4]), tile(0), tr(0) { }

    ~image_c(void);

    /* openGL image grepping is done in loops as the target resolution might
     * be much bigger than the openGl context. The image is grepped part by part
     * this is done in a loop
     * do {
     *   prepareOpenGlImagePart(..)
     *   draw();
     * while (getOpenGlImagePart);
     */
    void prepareOpenGlImagePart(voxelFrame_c * dr);
    bool getOpenGlImagePart(void);

    /* saves the image into a png file with the given name
     * extension is _not_ appended
     */
    int saveToPNG(const char * fname) const;

    /* blit image i at the given position into this
     * image. Alpha blending is done
     */
    void blit(const image_c * i, int xpos, int ypos);

    /* assigns a colour to all transparent pixels the pixels must be completely transparent */
    void deTransparentize(unsigned char r, unsigned char g, unsigned char b);

    /* makes all pixels with the given colour completely transparent */
    void transparentize(unsigned char r, unsigned char g, unsigned char b);

    /* scales the image down by the given factor
     * taking care of averaging pixels and alpha values
     */
    void scaleDown(unsigned char by);

    /* removes pixels columns from left and right that are completely transparent
     * but leaves a border of the given amount of pixels
     * the resulting image will be a multiple of multiple wide
     */
    void minimizeWidth(unsigned int border, unsigned int multiple = 1);

    /* return the size of the image */
    unsigned int w(void) { return width; }
    unsigned int h(void) { return height; }
};

#endif
