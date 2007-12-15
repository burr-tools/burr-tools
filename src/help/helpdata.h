#ifndef __HELPDATA_H__
#define __HELPDATA_H__

typedef struct {
  const char * const name;
  const char * const data;
  unsigned long size;
} filestruct;

extern filestruct filelist[];

typedef struct {
  const char * const name;
  const unsigned char * data;
  const int w;
  const int h;
} imagestruct;

extern imagestruct imagelist[];

#endif
