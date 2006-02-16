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
