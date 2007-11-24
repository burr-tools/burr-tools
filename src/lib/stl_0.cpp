/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "stl_0.h"
#include "math.h"

#include "puzzle.h"
#include "voxel.h"

static int get_mask(const voxel_c *v, int x,int y,int z,int rot, int edgeface)
 {
  int xx,yy,zz;
  int mask;

  mask=0;
  for (zz=edgeface?1:0; zz<2; zz++)
    for (yy=(edgeface==2)?1:0; yy<2; yy++)
      for (xx=0; xx<2; xx++)
	switch(rot)
	  {
	  case  0: mask = mask*2 + (v->isFilled2(x+xx-1,y+yy-1,z+zz-1)?1:0); break;
	  case  1: mask = mask*2 + (v->isFilled2(x-yy,y+xx-1,z+zz-1)?1:0); break;
	  case  2: mask = mask*2 + (v->isFilled2(x+yy-1,y+zz-1,z+xx-1)?1:0); break;
	  case  3: mask = mask*2 + (v->isFilled2(x+xx-1,y+zz-1,z-yy)?1:0); break;
	  case  4: mask = mask*2 + (v->isFilled2(x+zz-1,y+xx-1,z+yy-1)?1:0); break;
	  case  5: mask = mask*2 + (v->isFilled2(x+zz-1,y-yy,z+xx-1)?1:0); break;
	  case  6: mask = mask*2 + (v->isFilled2(x-xx,y-yy,z+zz-1)?1:0); break;
	  case  7: mask = mask*2 + (v->isFilled2(x+yy-1,y-xx,z+zz-1)?1:0); break;
	  case  8: mask = mask*2 + (v->isFilled2(x-yy,y+zz-1,z-xx)?1:0); break;
	  case  9: mask = mask*2 + (v->isFilled2(x-xx,y+zz-1,z+yy-1)?1:0); break;
	  case 10: mask = mask*2 + (v->isFilled2(x+zz-1,y-xx,z-yy)?1:0); break;
	  case 11: mask = mask*2 + (v->isFilled2(x+zz-1,y+yy-1,z-xx)?1:0); break;
	  case 12: mask = mask*2 + (v->isFilled2(x-xx,y+yy-1,z-zz)?1:0); break;
	  case 13: mask = mask*2 + (v->isFilled2(x-yy,y-xx,z-zz)?1:0); break;
	  case 14: mask = mask*2 + (v->isFilled2(x+yy-1,y-zz,z-xx)?1:0); break;
	  case 15: mask = mask*2 + (v->isFilled2(x-xx,y-zz,z-yy)?1:0); break;
	  case 16: mask = mask*2 + (v->isFilled2(x-zz,y-xx,z+yy-1)?1:0); break;
	  case 17: mask = mask*2 + (v->isFilled2(x-zz,y-yy,z-xx)?1:0); break;
	  case 18: mask = mask*2 + (v->isFilled2(x+xx-1,y-yy,z-zz)?1:0); break;
	  case 19: mask = mask*2 + (v->isFilled2(x+yy-1,y+xx-1,z-zz)?1:0); break;
	  case 20: mask = mask*2 + (v->isFilled2(x-yy,y-zz,z+xx-1)?1:0); break;
	  case 21: mask = mask*2 + (v->isFilled2(x+xx-1,y-zz,z+yy-1)?1:0); break;
	  case 22: mask = mask*2 + (v->isFilled2(x-zz,y+xx-1,z-yy)?1:0); break;
	  case 23: mask = mask*2 + (v->isFilled2(x-zz,y+yy-1,z+xx-1)?1:0); break;

	    // special cases for faces
	  case 24: mask = mask*2 + (v->isFilled2(x+xx-1,y+yy-1,z+zz-1)?1:0); break;
	  case 25: mask = mask*2 + (v->isFilled2(x+yy-1,y+zz-1,z+xx-1)?1:0); break;
	  case 26: mask = mask*2 + (v->isFilled2(x+zz-1,y+xx-1,z+yy-1)?1:0); break;
	  case 27: mask = mask*2 + (v->isFilled2(x+1-xx,y+yy-1,z+zz-1)?1:0); break;
	  case 28: mask = mask*2 + (v->isFilled2(x+yy-1,y+zz-1,z+1-xx)?1:0); break;
	  case 29: mask = mask*2 + (v->isFilled2(x+zz-1,y+1-xx,z+yy-1)?1:0); break;
	  }
  return mask;
}

void stlExporter_0_c::rotate_point(float *x, float *y, float *z, int rot)
{
  float xx,yy,zz;
  xx=*x;yy=*y;zz=*z;
  switch(rot)
    {
    case  0: *x=+xx; *y=+yy; *z=+zz; break;
    case  1: *x=-yy; *y=+xx; *z=+zz; break;
    case  2: *x=+yy; *y=+zz; *z=+xx; break;
    case  3: *x=+xx; *y=+zz; *z=-yy; break;
    case  4: *x=+zz; *y=+xx; *z=+yy; break;
    case  5: *x=+zz; *y=-yy; *z=+xx; break;
    case  6: *x=-xx; *y=-yy; *z=+zz; break;
    case  7: *x=+yy; *y=-xx; *z=+zz; break;
    case  8: *x=-yy; *y=+zz; *z=-xx; break;
    case  9: *x=-xx; *y=+zz; *z=+yy; break;
    case 10: *x=+zz; *y=-xx; *z=-yy; break;
    case 11: *x=+zz; *y=+yy; *z=-xx; break;
    case 12: *x=-xx; *y=+yy; *z=-zz; break;
    case 13: *x=-yy; *y=-xx; *z=-zz; break;
    case 14: *x=+yy; *y=-zz; *z=-xx; break;
    case 15: *x=-xx; *y=-zz; *z=-yy; break;
    case 16: *x=-zz; *y=-xx; *z=+yy; break;
    case 17: *x=-zz; *y=-yy; *z=-xx; break;
    case 18: *x=+xx; *y=-yy; *z=-zz; break;
    case 19: *x=+yy; *y=+xx; *z=-zz; break;
    case 20: *x=-yy; *y=-zz; *z=+xx; break;
    case 21: *x=+xx; *y=-zz; *z=+yy; break;
    case 22: *x=-zz; *y=+xx; *z=-yy; break;
    case 23: *x=-zz; *y=+yy; *z=+xx; break;

      // special cases for faces
    case 24: *x=+xx; *y=+yy; *z=+zz; break;
    case 25: *x=+yy; *y=+zz; *z=+xx; break;
    case 26: *x=+zz; *y=+xx; *z=+yy; break;
    case 27: *x=cube_scale-xx; *y=cube_scale-yy; *z=+zz; break;
    case 28: *x=cube_scale-yy; *y=+zz; *z=cube_scale-xx; break;
    case 29: *x=+zz; *y=cube_scale-xx; *z=cube_scale-yy; break;
    }
}

#define Epsilon 1.0e-5

void stlExporter_0_c::make_tri(float x0, float y0, float z0,
	     float x1, float y1, float z1,
	     float x2, float y2, float z2,
	     int rot, int x, int y, int z)

{
  float p0[3],p1[3],p2[3];

  rotate_point(&x0,&y0,&z0,rot);
  rotate_point(&x1,&y1,&z1,rot);
  rotate_point(&x2,&y2,&z2,rot);

  int cx = x;
  int cy = y;
  int cz = z;

  if (x0 < 0 || x1 < 0 || x2 < 0) cx--;
  if (y0 < 0 || y1 < 0 || y2 < 0) cy--;
  if (z0 < 0 || z1 < 0 || z2 < 0) cz--;

  p0[0]=x0+x*cube_scale;
  p0[1]=y0+y*cube_scale;
  p0[2]=z0+z*cube_scale;
  p1[0]=x1+x*cube_scale;
  p1[1]=y1+y*cube_scale;
  p1[2]=z1+z*cube_scale;
  p2[0]=x2+x*cube_scale;
  p2[1]=y2+y*cube_scale;
  p2[2]=z2+z*cube_scale;


  outTriangle(
      p0[0], p0[1], p0[2],
      p1[0], p1[1], p1[2],
      p2[0], p2[1], p2[2],
      (cx+0.5)*cube_scale,
      (cy+0.5)*cube_scale,
      (cz+0.5)*cube_scale
      );
}

void stlExporter_0_c::make_quad(float x0, float y0, float z0,
	       float x1, float y1, float z1,
	       float x2, float y2, float z2,
	       float x3, float y3, float z3,
	       int rot, int x, int y, int z)
{
  make_tri(x0,y0,z0,x1,y1,z1,x2,y2,z2,rot,x,y,z);
  make_tri(x0,y0,z0,x2,y2,z2,x3,y3,z3,rot,x,y,z);
}

void stlExporter_0_c::make_corners(const voxel_c *v, const int x, const int y, const int z)
{
  int match,mask;
  int rot;

#ifdef CHECK_CORNERS
  printf("checking corners (%d,%d,%d)\n",x,y,z);
#endif
  for (rot=0,match=0; rot<24 && !match; rot++)
    {
      match=1;
      switch (mask=get_mask(v,x,y,z,rot,0))
	{
	case 0 : break;
	case 1 : // tested
	  make_tri((bevel+shrink),(bevel+shrink),shrink,
		   (bevel+shrink),shrink,(bevel+shrink),
		   shrink,(bevel+shrink),(bevel+shrink),
		   rot, x, y, z);
	  break;
	case 3 : // tested
	  make_quad(-(bevel+shrink),shrink,(bevel+shrink),
		    -(bevel+shrink),(bevel+shrink),shrink,
		    bevel+shrink,(bevel+shrink),shrink,
		    bevel+shrink,shrink,(bevel+shrink),
		   rot, x, y, z);
	  break;
	case 6: // tested
	  make_tri(shrink,-(bevel+shrink),(bevel+shrink),
		   (bevel+shrink),-shrink,(bevel+shrink),
		   (bevel+shrink),-(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_tri(-shrink,(bevel+shrink),(bevel+shrink),
		   -(bevel+shrink),shrink,(bevel+shrink),
		   -(bevel+shrink),(bevel+shrink),shrink,
		   rot, x, y, z);
	  break;
	case 7 : // tested
	  make_quad(-(bevel+shrink),shrink,bevel+shrink,
		    -(bevel+shrink),bevel+shrink,shrink,
		    bevel+shrink,bevel+shrink,shrink,
		    shrink,shrink,bevel+shrink,
		   rot, x, y, z);
	  make_quad(bevel+shrink,-(bevel+shrink),shrink,
		    shrink,-(bevel+shrink),bevel+shrink,
		    shrink,shrink,bevel+shrink,
		    bevel+shrink,bevel+shrink,shrink,
		   rot, x, y, z);
	  break;
	case 15 : // tested
	  make_quad(-(bevel+shrink),-(bevel+shrink),shrink,
		    -(bevel+shrink),bevel+shrink,shrink,
		    bevel+shrink,bevel+shrink,shrink,
		    bevel+shrink,-(bevel+shrink),shrink,
		   rot, x, y, z);
	  break;
	case 22: // tested
	  make_tri(shrink,-(bevel+shrink),(bevel+shrink),
		   bevel+shrink,-shrink,(bevel+shrink),
		   bevel+shrink,-(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_tri(-shrink,bevel+shrink,(bevel+shrink),
		   -(bevel+shrink),shrink,(bevel+shrink),
		   -(bevel+shrink),bevel+shrink,shrink,
		   rot, x, y, z);
	  make_tri(shrink,(bevel+shrink),-(bevel+shrink),
		   (bevel+shrink),shrink,-(bevel+shrink),
		   (bevel+shrink),(bevel+shrink),-shrink,
		   rot, x, y, z);
	  break;
	case 23 : // tested
	  make_tri((bevel+shrink),shrink,shrink,
		   shrink,shrink,(bevel+shrink),
		   shrink,(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_quad(-(bevel+shrink),shrink,bevel+shrink,
		    -(bevel+shrink),bevel+shrink,shrink,
		    shrink,bevel+shrink,shrink,
		    shrink,shrink,bevel+shrink,
		   rot, x, y, z);
	  make_quad(bevel+shrink,-(bevel+shrink),shrink,
		    shrink,-(bevel+shrink),bevel+shrink,
		    shrink,shrink,bevel+shrink,
		    bevel+shrink,shrink,shrink,
		   rot, x, y, z);
	  make_quad(shrink,bevel+shrink,-(bevel+shrink),
		    bevel+shrink,shrink,-(bevel+shrink),
		    bevel+shrink,shrink,shrink,
		    shrink,bevel+shrink,shrink,
		   rot, x, y, z);
	  break;
	case 24: // tested
	  make_tri(bevel+shrink,bevel+shrink,-shrink,
		   shrink,bevel+shrink,-(bevel+shrink),
		   bevel+shrink,shrink,-(bevel+shrink),
		   rot, x, y, z);
	  make_tri(-shrink,-(bevel+shrink),bevel+shrink,
		   -(bevel+shrink),-(bevel+shrink),shrink,
		   -(bevel+shrink),-shrink,bevel+shrink,
		   rot, x, y, z);
	  break;
	case 25: // tested
	  make_quad(shrink,(bevel+shrink),-(bevel+shrink),
		    (bevel+shrink),shrink,-(bevel+shrink),
		    (bevel+shrink),shrink,bevel+shrink,
		    shrink,(bevel+shrink),bevel+shrink,
		   rot, x, y, z);
	  make_tri(-shrink,-(bevel+shrink),bevel+shrink,
		   -(bevel+shrink),-(bevel+shrink),shrink,
		   -(bevel+shrink),-shrink,bevel+shrink,
		   rot, x, y, z);
	  break;
	case 27:
	  make_quad(shrink,bevel+shrink,-(bevel+shrink),
		    bevel+shrink,shrink,-(bevel+shrink),
		    bevel+shrink,shrink,bevel+shrink,
		    shrink,bevel+shrink,shrink,
		    rot, x, y, z);
	  make_quad(-shrink,-(bevel+shrink),bevel+shrink,
		    -(bevel+shrink),-(bevel+shrink),shrink,
		    -(bevel+shrink),(bevel+shrink),shrink,
		    -shrink,shrink,bevel+shrink,
		    rot, x, y, z);
	  make_quad(-shrink,shrink,bevel+shrink,
		    -(bevel+shrink),bevel+shrink,shrink,
		    shrink,bevel+shrink,shrink,
		    bevel+shrink,shrink,bevel+shrink,
		    rot, x, y, z);
	  break;
	case 29: // tested
	  make_quad(shrink,bevel+shrink,-(bevel+shrink),
		    bevel+shrink,shrink,-(bevel+shrink),
		    bevel+shrink,shrink,shrink,
		    shrink,bevel+shrink,bevel+shrink,
		    rot, x, y, z);
	  make_quad(-(bevel+shrink),-shrink,bevel+shrink,
		    shrink,-shrink,bevel+shrink,
		    bevel+shrink,-(bevel+shrink),shrink,
		    -(bevel+shrink),-(bevel+shrink),shrink,
		    rot, x, y, z);
	  make_quad(shrink,-shrink,bevel+shrink,
		    shrink,bevel+shrink,bevel+shrink,
		    bevel+shrink,shrink,shrink,
		    bevel+shrink,-(bevel+shrink),shrink,
		    rot, x, y, z);
	  break;
	case 30: // tested
	  make_quad((bevel+shrink),-shrink,bevel+shrink,
		    (bevel+shrink),-(bevel+shrink),shrink,
		    -(bevel+shrink),-(bevel+shrink),shrink,
		    -shrink,-shrink,bevel+shrink,
		   rot, x, y, z);
	  make_quad(-(bevel+shrink),(bevel+shrink),shrink,
		    -shrink,(bevel+shrink),bevel+shrink,
		    -shrink,-shrink,bevel+shrink,
		    -(bevel+shrink),-(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_tri(bevel+shrink,bevel+shrink,-shrink,
		   shrink,bevel+shrink,-(bevel+shrink),
		   bevel+shrink,shrink,-(bevel+shrink),
		   rot, x, y, z);
	  break;
	case 31: // tested
	  make_quad(shrink,bevel+shrink,-(bevel+shrink),
		    bevel+shrink,shrink,-(bevel+shrink),
		    bevel+shrink,shrink,shrink,
		    shrink,bevel+shrink,shrink,
		   rot, x, y, z);
	  make_tri(-(bevel+shrink),-(bevel+shrink),shrink,
		   shrink,bevel+shrink,shrink,
		   bevel+shrink,shrink,shrink,
		       rot, x, y, z);
	  make_tri(-(bevel+shrink),-(bevel+shrink),shrink,
		       -(bevel+shrink),bevel+shrink,shrink,
		       shrink,bevel+shrink,shrink,
		       rot, x, y, z);
	  make_tri(-(bevel+shrink),-(bevel+shrink),shrink,
		       bevel+shrink,shrink,shrink,
		       bevel+shrink,-(bevel+shrink),shrink,
		       rot, x, y, z);

	  break;
	case 60 : // tested
	  make_quad(-(bevel+shrink),-shrink,(bevel+shrink),
		    bevel+shrink,-shrink,(bevel+shrink),
		    bevel+shrink,-(bevel+shrink),shrink,
		    -(bevel+shrink),-(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_quad(-(bevel+shrink),shrink,-(bevel+shrink),
		    bevel+shrink,shrink,-(bevel+shrink),
		    bevel+shrink,(bevel+shrink),-shrink,
		    -(bevel+shrink),(bevel+shrink),-shrink,
		   rot, x, y, z);
	  break;
	case 103: // tested (was 61)
	  make_quad(-(bevel+shrink),shrink,bevel+shrink,
		    -(bevel+shrink),shrink,shrink,
		    shrink,shrink,shrink,
		    shrink,shrink,bevel+shrink,
		    rot, x, y, z);
	  make_quad(-(bevel+shrink),shrink,shrink,
		    -shrink,bevel+shrink,shrink,
		    bevel+shrink,bevel+shrink,shrink,
		    shrink,shrink,shrink,
		   rot, x, y, z);
	  make_quad(-(bevel+shrink),shrink,shrink,
		    -(bevel+shrink),shrink,-(bevel+shrink),
		    -shrink,bevel+shrink,-(bevel+shrink),
		    -shrink,bevel+shrink,shrink,
		   rot, x, y, z);
	  make_quad(shrink,-(bevel+shrink),shrink,
		    shrink,-(bevel+shrink),bevel+shrink,
		    shrink,shrink,bevel+shrink,
		    shrink,shrink,shrink,
		   rot, x, y, z);
	  make_quad(bevel+shrink,-shrink,shrink,
		    shrink,-(bevel+shrink),shrink,
		    shrink,shrink,shrink,
		    bevel+shrink,bevel+shrink,shrink,
		   rot, x, y, z);
	  make_quad(bevel+shrink,-shrink,shrink,
		    bevel+shrink,-shrink,-(bevel+shrink),
		    shrink,-(bevel+shrink),-(bevel+shrink),
		    shrink,-(bevel+shrink),shrink,
		   rot, x, y, z);
	  break;
	case 63: // tested
	  make_quad(-(bevel+shrink),-(bevel+shrink),shrink,
		    -(bevel+shrink),shrink,shrink,
		    bevel+shrink,shrink,shrink,
		    bevel+shrink,-(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_quad(-(bevel+shrink),shrink,-(bevel+shrink),
		    bevel+shrink,shrink,-(bevel+shrink),
		    bevel+shrink,shrink,shrink,
		    -(bevel+shrink),shrink,shrink,
		   rot, x, y, z);
	  break;
	case 105: // tested
	  make_tri(-(bevel+shrink),-(bevel+shrink),shrink,
		   -(bevel+shrink),-shrink,(bevel+shrink),
		   -shrink,-(bevel+shrink),(bevel+shrink),
		   rot, x, y, z);
	  make_tri(bevel+shrink,bevel+shrink,shrink,
		   bevel+shrink,shrink,(bevel+shrink),
		   shrink,bevel+shrink,(bevel+shrink),
		   rot, x, y, z);
	  make_tri(-(bevel+shrink),bevel+shrink,-shrink,
		   -(bevel+shrink),shrink,-(bevel+shrink),
		   -shrink,bevel+shrink,-(bevel+shrink),
		   rot, x, y, z);
	  make_tri(bevel+shrink,-(bevel+shrink),-shrink,
		   bevel+shrink,-shrink,-(bevel+shrink),
		   shrink,-(bevel+shrink),-(bevel+shrink),
		   rot, x, y, z);
	  break;
	case 111: // tested
	  make_quad(-shrink,bevel+shrink,-(bevel+shrink),
		    -shrink,bevel+shrink,shrink,
		    -(bevel+shrink),shrink,shrink,
		    -(bevel+shrink),shrink,-(bevel+shrink),
		   rot, x, y, z);
	  make_quad(shrink,-(bevel+shrink),-(bevel+shrink),
		    shrink,-(bevel+shrink),shrink,
		    bevel+shrink,-shrink,shrink,
		    bevel+shrink,-shrink,-(bevel+shrink),
		   rot, x, y, z);
	  make_quad(shrink,-(bevel+shrink),shrink,
		    -(bevel+shrink),-(bevel+shrink),shrink,
		    -(bevel+shrink),+shrink,shrink,
		    bevel+shrink,-shrink,shrink,
		    rot, x, y, z);

	  make_quad(-shrink,bevel+shrink,shrink,
		    bevel+shrink,bevel+shrink,shrink,
		    bevel+shrink,-shrink,shrink,
		    -(bevel+shrink),shrink,shrink,
		    rot, x, y, z);


	  break;
	case 126: break; // tested;
	case 127: // tested;
	  make_quad(-(bevel+shrink),shrink,-(bevel+shrink),
		    shrink,shrink,-(bevel+shrink),
		    shrink,shrink,shrink,
		    -(bevel+shrink),shrink,shrink,
		   rot, x, y, z);
	  make_quad(-(bevel+shrink),-(bevel+shrink),shrink,
		    -(bevel+shrink),shrink,shrink,
		    shrink,shrink,shrink,
		    shrink,-(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_quad(shrink,-(bevel+shrink),-(bevel+shrink),
		    shrink,-(bevel+shrink),shrink,
		    shrink,shrink,shrink,
		    shrink,shrink,-(bevel+shrink),
		   rot, x, y, z);
	  break;
	case 151: // tested (was 107)
	  make_tri(-(bevel+shrink),-(bevel+shrink),-shrink,
		   -shrink,-(bevel+shrink),-(bevel+shrink),
		   -(bevel+shrink),-shrink,-(bevel+shrink),
		   rot, x, y, z);
	  make_tri((bevel+shrink),shrink,shrink,
		   shrink,shrink,(bevel+shrink),
		   shrink,(bevel+shrink),shrink,
		   rot, x, y, z);
	  make_quad(-(bevel+shrink),shrink,bevel+shrink,
		    -(bevel+shrink),bevel+shrink,shrink,
		    shrink,bevel+shrink,shrink,
		    shrink,shrink,bevel+shrink,
		   rot, x, y, z);
	  make_quad(bevel+shrink,-(bevel+shrink),shrink,
		    shrink,-(bevel+shrink),bevel+shrink,
		    shrink,shrink,bevel+shrink,
		    bevel+shrink,shrink,shrink,
		   rot, x, y, z);
	  make_quad(shrink,bevel+shrink,-(bevel+shrink),
		    bevel+shrink,shrink,-(bevel+shrink),
		    bevel+shrink,shrink,shrink,
		    shrink,bevel+shrink,shrink,
		   rot, x, y, z);
	  break;
	case 255 : break; // tested
	default :
	  match=0;
	}
    }
}

void stlExporter_0_c::make_edges(const voxel_c *v, const int x, const int y, const int z)
{
  int rot;
  for (rot=0; rot<12; rot++)
    {
      switch (get_mask(v,x,y,z,rot,1))
	{
	case 0 : break;
	case 15 : break;
	case 1 :
	  make_quad((bevel+shrink),shrink,bevel+shrink,
		    (bevel+shrink),shrink,cube_scale-(bevel+shrink),
		    shrink,(bevel+shrink),cube_scale-(bevel+shrink),
		    shrink,(bevel+shrink),bevel+shrink,
		   rot, x, y, z);
	  break;
	case 9 :
	  make_quad(-(bevel+shrink),-shrink,bevel+shrink,
		    -(bevel+shrink),-shrink,cube_scale-(bevel+shrink),
		    -shrink,-(bevel+shrink),cube_scale-(bevel+shrink),
		    -shrink,-(bevel+shrink),bevel+shrink,
		   rot, x, y, z);
	  make_quad((bevel+shrink),shrink,bevel+shrink,
		    (bevel+shrink),shrink,cube_scale-(bevel+shrink),
		    shrink,(bevel+shrink),cube_scale-(bevel+shrink),
		    shrink,(bevel+shrink),bevel+shrink,
		   rot, x, y, z);
	  break;
	case 3 :
	  make_quad(-(bevel+shrink),shrink,bevel+shrink,
		    bevel+shrink,shrink,bevel+shrink,
		    bevel+shrink,shrink,cube_scale-(bevel+shrink),
		    -(bevel+shrink),shrink,cube_scale-(bevel+shrink),
		   rot, x, y, z);
	  break;
	case 7 :
	  make_quad(+shrink,shrink,bevel+shrink,
		    +shrink,shrink,cube_scale-(bevel+shrink),
		    -(bevel+shrink),shrink,cube_scale-(bevel+shrink),
		    -(bevel+shrink),shrink,bevel+shrink,
		   rot, x, y, z);
	  make_quad(shrink,+shrink,bevel+shrink,
		    shrink,-(bevel+shrink),bevel+shrink,
		    shrink,-(bevel+shrink),cube_scale-(bevel+shrink),
		    shrink,+shrink,cube_scale-(bevel+shrink),
		   rot, x, y, z);
	  break;
	default : break;
	}
    }
}

void stlExporter_0_c::make_faces(const voxel_c *v, const int x,const int y,const int z)
{
  int rot;
  for (rot=24; rot<30; rot++)
    if (get_mask(v,x,y,z,rot,2)==1)
      {
	make_quad(shrink,shrink+bevel,shrink+bevel,
		  shrink,shrink+bevel,cube_scale-(shrink+bevel),
		  shrink,cube_scale-(shrink+bevel),cube_scale-(shrink+bevel),
		  shrink,cube_scale-(shrink+bevel),shrink+bevel,
		  rot,x,y,z);
      }
}

void stlExporter_0_c::write(const char * fname, voxel_c * v) {

  if (v->countState(voxel_c::VX_VARIABLE)) throw new stlException_c("Shapes with variable voxels cannot be exported");
  if (cube_scale < Epsilon) throw new stlException_c("Cube size too small");
  if (shrink < 0) throw new stlException_c("Offset cannot be negative");
  if (bevel < 0) throw new stlException_c("Bevel cannot be negative");
  if (cube_scale < (2*bevel + 2*shrink)) throw new stlException_c("Cube size too small for given bevel and offset");

  int cost = (int)ceilf(v->countState(voxel_c::VX_FILLED) * cube_scale*cube_scale*cube_scale / 1000.0);

  char name[1000];
  snprintf(name, 1000, "%s_%03i", fname, cost);
  open(name);

  for (unsigned int x = 0; x <= v->getX(); x++)
    for (unsigned int y = 0; y <= v->getY(); y++)
      for (unsigned int z = 0; z <= v->getZ(); z++) {
        make_faces(v,x,y,z);
        make_edges(v,x,y,z);
        make_corners(v,x,y,z);
      }

  close();
}


const char * stlExporter_0_c::getParameterName(unsigned int idx) const {

  switch (idx) {

    case 0: return "Cube Size";
    case 1: return "Bevel";
    case 2: return "Offset";
    default: return 0;
  }
}

double stlExporter_0_c::getParameter(unsigned int idx) const {

  switch (idx) {
    case 0: return cube_scale;
    case 1: return bevel;
    case 2: return shrink;
    default: return 0;
  }
}

void stlExporter_0_c::setParameter(unsigned int idx, double value) {

  switch (idx) {
    case 0: cube_scale = value; return;
    case 1: bevel = value; return;
    case 2: shrink = value; return;
    default: return;
  }
}

