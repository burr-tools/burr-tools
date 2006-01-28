#include "tr.h"
#include "VoxelDrawer.h"


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
    Image(unsigned int width, unsigned int height, VoxelDrawer * dr, TRcontext * tr);

    ~Image(void);

    int saveToPNG(const char * fname) const;

    void blit(const Image * i, int xpos, int ypos);

    /* makes the pixels with the given color transparent */
    void transparentize(unsigned char r, unsigned char g, unsigned char b);

    /* scales the image down by the given factor */
    void scaleDown(unsigned char by);

    /* removes pixels columns from left and right that are only filled with the given color
     * but leaves a border of the given amount of pixels
     */
    void minimizeWidth(unsigned char r, unsigned char g, unsigned char b, unsigned int border);

};
