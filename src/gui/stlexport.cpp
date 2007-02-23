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
#include "stlexport.h"
#include "math.h"

#include "BlockList.h"
#include "view3dgroup.h"
#include "blocklistgroup.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <FL/Fl.H>
#include <FL/fl_ask.H>

static void cb_stlExportAbort_stub(Fl_Widget* o, void* v) { ((stlExport_c*)(v))->cb_Abort(); }

void stlExport_c::cb_Abort(void) {
  hide();
}

static void cb_stlExportExport_stub(Fl_Widget* o, void* v) { ((stlExport_c*)(v))->cb_Export(); }

void stlExport_c::cb_Export(void) {

  exportSTL(ShapeSelect->getSelection());

}

static void cb_stlExport3DUpdate_stub(Fl_Widget* o, void* v) { ((stlExport_c*)(v))->cb_Update3DView(); }
void stlExport_c::cb_Update3DView(void) {

  view3D->showSingleShape(puzzle, ShapeSelect->getSelection());
}

stlExport_c::stlExport_c(puzzle_c * p, const guiGridType_c * ggt) : LFl_Double_Window(false), puzzle(p) {

  label("Export STL");

  LFl_Frame *fr;

  {
    fr = new LFl_Frame(0, 0, 1, 1);

    (new LFl_Box("File name", 0, 0))->stretchLeft();
    (new LFl_Box("Path", 0, 1))->stretchLeft();

    (new LFl_Box(1, 0))->setMinimumSize(5, 0);
    (new LFl_Box(3, 0))->setMinimumSize(5, 0);

    Fname = new LFl_Input(2, 0, 3, 1);
    Fname->value("test");
    Fname->weight(1, 0);
    Fname->setMinimumSize(50, 0);
    Pname = new LFl_Input(2, 1, 3, 1);
    Pname->value("./");
    Pname->weight(1, 0);
    Pname->setMinimumSize(50, 0);

    fr->end();
  }


  {
    fr = new LFl_Frame(0, 1, 1, 1);

    (new LFl_Box("Cube Size", 0, 0))->stretchLeft();
    (new LFl_Box("Bevel", 0, 1))->stretchLeft();
    (new LFl_Box("Offset", 0, 2))->stretchLeft();

    (new LFl_Box(0, 0))->setMinimumSize(5, 0);
    (new LFl_Box(0, 1))->setMinimumSize(5, 0);
    (new LFl_Box(0, 2))->setMinimumSize(5, 0);

    CubeSize = new LFl_Float_Input(2, 0, 3, 1);
    CubeSize->value("10.0");

    Bevel = new LFl_Float_Input(2, 1, 3, 1);
    Bevel->value("1.0");

    Offset = new LFl_Float_Input(2, 2, 3, 1);
    Offset->value("0.1");

    fr->end();
  }

  {
    ShapeSelect = new PieceSelector(0, 0, 20, 20, puzzle);

    ShapeSelect->setSelection(0);

    LBlockListGroup_c * gr = new LBlockListGroup_c(0, 2, 1, 1, ShapeSelect);
    gr->callback(cb_stlExport3DUpdate_stub, this);
    gr->setMinimumSize(200, 100);
    gr->stretch();
  }

  {
    layouter_c * l = new layouter_c(0, 3, 2, 1);

    status = new LFl_Box();
    status->weight(1, 0);
    status->label("Test");
    status->pitch(7);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    BtnStart = new LFl_Button("Export STL", 1, 0);
    BtnStart->pitch(7);
    BtnStart->callback(cb_stlExportExport_stub, this);

    BtnAbbort = new LFl_Button("Abort", 2, 0);
    BtnAbbort->pitch(7);
    BtnAbbort->callback(cb_stlExportAbort_stub, this);

    l->end();
  }

  view3D = new LView3dGroup(1, 0, 1, 3, ggt);
  view3D->setMinimumSize(400, 400);
  cb_Update3DView();

  set_modal();
}


static FILE *fp;
static float bevel;
static float shrink;
static float cube_scale;

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

static void rotate_point(float *x, float *y, float *z, int rot)
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

static float vlength(const float a[3]) {
  return sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
}

#define Epsilon 1.0e-5

static void make_tri(float x0, float y0, float z0,
	     float x1, float y1, float z1,
	     float x2, float y2, float z2,
	     int rot, int x, int y, int z)

{
  float v1[3],v2[3],v3[3];
  float p0[3],p1[3],p2[3],n[3];
  float len;

  rotate_point(&x0,&y0,&z0,rot);
  rotate_point(&x1,&y1,&z1,rot);
  rotate_point(&x2,&y2,&z2,rot);

  p0[0]=x0+x*cube_scale;
  p0[1]=y0+y*cube_scale;
  p0[2]=z0+z*cube_scale;
  p1[0]=x1+x*cube_scale;
  p1[1]=y1+y*cube_scale;
  p1[2]=z1+z*cube_scale;
  p2[0]=x2+x*cube_scale;
  p2[1]=y2+y*cube_scale;
  p2[2]=z2+z*cube_scale;

  v1[0]=p1[0]-p0[0]; v1[1]=p1[1]-p0[1]; v1[2]=p1[2]-p0[2];
  v2[0]=p2[0]-p0[0]; v2[1]=p2[1]-p0[1]; v2[2]=p2[2]-p0[2];
  v3[0]=p2[0]-p1[0]; v3[1]=p2[1]-p1[1]; v3[2]=p2[2]-p1[2];

  if (vlength(v1)<Epsilon||
      vlength(v2)<Epsilon||
      vlength(v3)<Epsilon)
    return;

  // generate normal
  n[0] = (v2[1] * v1[2]) - (v2[2] * v1[1]);
  n[1] = (v2[2] * v1[0]) - (v2[0] * v1[2]);
  n[2] = (v2[0] * v1[1]) - (v2[1] * v1[0]);


  len = vlength(n);
  if (len>Epsilon) {
    n[0] /= len; n[1] /= len; n[2] /= len;
  }
  else return;

  fprintf(fp,"  facet normal %9.4e %9.4e %9.4e\n",n[0],n[1],n[2]);
  fprintf(fp,"    outer loop\n");
  fprintf(fp,"      vertex %9.4e %9.4e %9.4e\n",p0[0],p0[1],p0[2]);
  fprintf(fp,"      vertex %9.4e %9.4e %9.4e\n",p1[0],p1[1],p1[2]);
  fprintf(fp,"      vertex %9.4e %9.4e %9.4e\n",p2[0],p2[1],p2[2]);
  fprintf(fp,"    endloop\n");
  fprintf(fp,"  endfacet\n");
}

static void make_quad(float x0, float y0, float z0,
	       float x1, float y1, float z1,
	       float x2, float y2, float z2,
	       float x3, float y3, float z3,
	       int rot, int x, int y, int z)
{
  make_tri(x0,y0,z0,x1,y1,z1,x2,y2,z2,rot,x,y,z);
  make_tri(x0,y0,z0,x2,y2,z2,x3,y3,z3,rot,x,y,z);
}

static void make_corners(const voxel_c *v, const int x, const int y, const int z)
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

static void make_edges(const voxel_c *v, const int x, const int y, const int z)
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

static void make_faces(const voxel_c *v, const int x,const int y,const int z)
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

void stlExport_c::exportSTL(int shape)
{
  int x,y,z;
  char name[1000];

  if (puzzle->getGridType()->getType() != gridType_c::GT_BRICKS) {
    fl_message("Sorry STL export only supports cubes right now");
    return;
  }

  voxel_c *v = puzzle->getShape(shape);

  for (unsigned int i = 0; i < v->getXYZ(); i++)
    if (v->getState(i) == voxel_c::VX_VARIABLE) {
      fl_message("I can not export shapes with variable voxels");
      return;
    }

  int xsize = v->getX();
  int ysize = v->getY();
  int zsize = v->getZ();

  bevel = atof(Bevel->value());
  shrink = atof(Offset->value());
  cube_scale = atof(CubeSize->value());
  int cost = (int)ceilf((float)(v->countState(voxel_c::VX_FILLED))*cube_scale*cube_scale*cube_scale / 1000.0);

  if (Pname->value() && Pname->value()[0] &&
      Pname->value()[strlen(Pname->value())-1] != '/') {
    if (v->getName().length())
      snprintf(name, 1000, "%s/%s_%s_%03i.stl", Pname->value(), Fname->value(), v->getName().c_str(),cost);
    else
      snprintf(name, 1000, "%s/%s_S%d_%03i.stl", Pname->value(), Fname->value(), shape+1,cost);
  }
  else {
    if (v->getName().length())
      snprintf(name, 1000, "%s%s_%s_%03i.stl", Pname->value(), Fname->value(), v->getName().c_str(),cost);
    else
      snprintf(name, 1000, "%s%s_S%d_%03i.stl", Pname->value(), Fname->value(), shape+1,cost);
  }
  status->copy_label(name);

  if (cube_scale < Epsilon) {
    fl_message("Cube size too small!");
    return;
  }

  if (shrink < 0) {
    fl_message("Offset cannot be negative!");
    return;
  }

  if (bevel < 0) {
    fl_message("Bevel cannot be negative!");
    return;
  }

  if (cube_scale < (2*bevel + 2*shrink)) {
    fl_message("Cube size too small for given bevel and offset!");
    return;
  }

  fp = (FILE *) fopen(name,"w");

  if (fp==NULL) {
    fl_message("Could not open file");
    return;
  }

  if (v->getName().length())
    fprintf(fp,"solid %s_%s_%03i\n",Fname->value(),v->getName().c_str(),cost);
  else
    fprintf(fp,"solid %s_S%d_%03i\n",Fname->value(),shape+1,cost);
  for (x=0; x<=xsize; x++)
    for (y=0; y<=ysize; y++)
      for (z=0; z<=zsize; z++)
	{
	  make_faces(v,x,y,z);
	  make_edges(v,x,y,z);
	  make_corners(v,x,y,z);
	}
  fprintf(fp,"endsolid\n");
  fclose(fp);
}
