/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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
#include <stdlib.h>

#include "BlockList.h"
#include "view3dgroup.h"
#include "blocklistgroup.h"
#include "voxelframe.h"
#include "buttongroup.h"

#include "../lib/puzzle.h"
#include "../lib/gridtype.h"
#include "../lib/bt_assert.h"
#include "../lib/voxel.h"

#include "../halfedge/volume.h"

#include "../flu/Flu_File_Chooser.h"

#include "../tools/fileexists.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#pragma GCC diagnostic pop


// a simple class (structure) containing some information for each input field
class inputField_c
{
  public:

    stlExporter_c::parameterTypes type;
    Fl_Widget * w;
};


static void cb_stlExportAbort_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Abort(); }

void stlExport_c::cb_Abort(void) {
  hide();
}

static void cb_stlExportExport_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Export(); }

void stlExport_c::cb_Export(void) {

  exportSTL(ShapeSelect->getSelection());

}

static void updateParameters(stlExporter_c * stl, const std::vector<inputField_c *> & params)
{
  for (unsigned int i = 0; i < stl->numParameters(); i++)
  {
    switch (params[i]->type)
    {
      case stlExporter_c::PAR_TYP_DOUBLE:
      case stlExporter_c::PAR_TYP_POS_DOUBLE:
        stl->setParameter(i, atof(((LFl_Float_Input*)(params[i]->w))->value()));
        break;
      case stlExporter_c::PAR_TYP_POS_INTEGER:
        stl->setParameter(i, atoi(((LFl_Int_Input*)(params[i]->w))->value()));
        break;
      case stlExporter_c::PAR_TYP_SWITCH:
        stl->setParameter(i, ((LFl_Check_Button*)(params[i]->w))->value());
        break;
      default:
        bt_assert(0);
    }
  }
}

static void cb_stlExport3DUpdate_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Update3DView(1); }
static void cb_stlExport3DUpdate2_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Update3DView(2); }
void stlExport_c::cb_Update3DView(int type)
{
  updateParameters(stl, params);

  if (type == 2)
  {
    holes.clear();
  }

  Polyhedron * p = 0;
  try
  {
    p = stl->getMesh(*puzzle->getShape(ShapeSelect->getSelection()), holes);
  }
  catch (stlException_c e)
  {
    fl_message("%s",e.comment);
    return;
  }
  catch (...)
  {
    fl_message("The generated mesh is faulty in some way, try to tweak the parameter");
    return;
  }

  if (p)
  {
    view3D->getView()->showMesh(p);
    char txt[100];
    snprintf(txt, 99, "Volume: %1.1f cubic-units\n", volume(*p));
    status->copy_label(txt);
  }
  else
  {
    view3D->getView()->showNothing();
    status->label("");
  }

}

static void cb_stlFileChooser_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_FileChooser(); }
void stlExport_c::cb_FileChooser(void)
{
  char curFile[500];
  snprintf(curFile, 500, "%s/%s", Pname->value(), Fname->value());

  const char * f = flu_file_chooser("Choose STL File to write", "*.stl", curFile);

  if (f)
  {
    const char * div = strrchr(f, '/');

    if (div)
    {
      Fname->value(div+1);

      int i = 0;
      while (true)
      {
        curFile[i] = f[i];
        i++;
        if (f[i] == 0) break;
        if (i >= 499) break;
        if (div == f+i) break;
      }

      curFile[i] = 0;

      Pname->value(curFile);
    }

  }


}

static void cb_stlExportViewUpdate_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_Update3DViewParams(); }
void stlExport_c::cb_Update3DViewParams(void)
{
  switch (mode->getSelected())
  {
    case 0: view3D->getView()->setInsideVisible(false); break;
    case 1: view3D->getView()->setInsideVisible(true); break;
    default: break;
  }
}

static void cb_3dClick_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)v)->cb_3dClick(); }
void stlExport_c::cb_3dClick(void)
{
  if (Fl::event_ctrl() || Fl::event_shift())
  {
    unsigned int shape, face;
    unsigned long voxel;

    if (view3D->getView()->pickShape(Fl::event_x(),
        view3D->getView()->h()-Fl::event_y(),
        &shape, &voxel, &face))
    {
      if (shape == 0)
      {
        if (Fl::event_ctrl())
        {
          holes.removeFace(voxel, face);
          cb_Update3DView(1);
        }
        if (Fl::event_shift())
        {
          holes.addFace(voxel, face);
          cb_Update3DView(1);
        }
      }
    }
  }
}

stlExport_c::stlExport_c(puzzle_c * p) : LFl_Double_Window(true), puzzle(p) {

  label("Export STL");

  stl = p->getGridType()->getStlExporter();
  bt_assert(stl);

  LFl_Frame *fr;

  {
    fr = new LFl_Frame(0, 0, 1, 1);

    (new LFl_Box("File name", 0, 0))->stretchLeft();
    (new LFl_Box("Path", 0, 1))->stretchLeft();

    (new LFl_Box(1, 0))->setMinimumSize(5, 0);
    (new LFl_Box(3, 0))->setMinimumSize(5, 0);

    Fname = new LFl_Input(2, 0, 1, 1);
    Fname->value("test");
    Fname->weight(1, 0);
    Fname->setMinimumSize(50, 0);
    Pname = new LFl_Input(2, 1, 1, 1);
    Pname->value(".");
    Pname->weight(1, 0);
    Pname->setMinimumSize(50, 0);

    Fl_Button * Btn_Dir = new LFl_Button("...", 3, 0, 2, 2);
    Btn_Dir->callback(cb_stlFileChooser_stub, this);

    Binary = new LFl_Check_Button("Binary STL", 0, 2, 3, 1);
    if (stl->getBinaryMode())
      Binary->value(1);
    else
      Binary->value(0);

    fr->end();
  }

  {
    fr = new LFl_Frame(0, 1, 1, 1);

    inputField_c * inp;

    for (unsigned int i = 0; i < stl->numParameters(); i++)
    {
      inp = new inputField_c;

      inp->type = stl->getParameterType(i);

      switch (inp->type)
      {
        case stlExporter_c::PAR_TYP_DOUBLE:
          {
            (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
            (new LFl_Box(1, i))->setMinimumSize(5, 0);
            LFl_Float_Input * in = new LFl_Float_Input(2, i, 1, 1);
            char val[10];
            snprintf(val, 10, "%2.2f", stl->getParameter(i));
            in->value(val);
            in->weight(1, 0);

            inp->w = in;
          }
          break;

        case stlExporter_c::PAR_TYP_POS_DOUBLE:
          {
            (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
            (new LFl_Box(1, i))->setMinimumSize(5, 0);
            LFl_Float_Input * in = new LFl_Float_Input(2, i, 1, 1);

            char val[10];
            snprintf(val, 10, "%2.2f", stl->getParameter(i));
            in->value(val);
            in->weight(1, 0);

            inp->w = in;
          }
          break;

        case stlExporter_c::PAR_TYP_POS_INTEGER:
          {
            (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
            (new LFl_Box(1, i))->setMinimumSize(5, 0);
            LFl_Int_Input * in = new LFl_Int_Input(2, i, 1, 1);

            char val[10];
            snprintf(val, 10, "%i", (int)stl->getParameter(i));
            in->value(val);
            in->weight(1, 0);

            inp->w = in;
          }
          break;

        case stlExporter_c::PAR_TYP_SWITCH:
          {
            LFl_Check_Button * in = new LFl_Check_Button(stl->getParameterName(i), 0, i, 3, 1);

            if (stl->getParameter(i))
              in->value(1);
            else
              in->value(0);

            in->weight(1, 0);

            inp->w = in;
          }
          break;

        default:
          bt_assert(0);
      }

      inp->w->callback(cb_stlExport3DUpdate_stub, this);
      inp->w->tooltip(stl->getParameterTooltip(i));

      params.push_back(inp);
    }

    fr->end();
  }

  {
    ShapeSelect = new PieceSelector(0, 0, 20, 20, puzzle);

    ShapeSelect->setSelection(0);

    LBlockListGroup_c * gr = new LBlockListGroup_c(0, 2, 1, 1, ShapeSelect);
    gr->callback(cb_stlExport3DUpdate2_stub, this);
    gr->setMinimumSize(200, 100);
    gr->stretch();
    gr->weight(0, 1);
  }

  {
    fr = new LFl_Frame(0, 3, 1, 1);

    layouter_c * l = new layouter_c(0, 0, 1, 1);
    l->pitch(5);

    BtnStart = new LFl_Button("Export STL", 0, 0);
    BtnStart->callback(cb_stlExportExport_stub, this);

    (new LFl_Box(0, 1))->setMinimumSize(0, 5);

    BtnAbbort = new LFl_Button("Abort", 0, 2);
    BtnAbbort->callback(cb_stlExportAbort_stub, this);

    fr->end();
  }

  {
    layouter_c * l = new layouter_c(0, 4, 2, 1);

    status = new LFl_Box(0, 0, 1, 1);
    status->box(FL_UP_BOX);
    status->weight(1, 0);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    mode = new ButtonGroup_c(1, 0, 1, 1);

    Fl_Button * b;

    b = mode->addButton();
    b->image(pm.get(ViewModeNormal_xpm));
    b->tooltip(" Display STL ebject normally ");

    b = mode->addButton();
    b->image(pm.get(ViewModeInsides_xpm));
    b->tooltip(" Display the insides of the STL object ");

    mode->callback(cb_stlExportViewUpdate_stub, this);

    l->end();
  }

  view3D = new LView3dGroup(1, 0, 1, 4);
  view3D->setMinimumSize(400, 400);
  view3D->weight(1, 0);
  view3D->callback(cb_3dClick_stub, this);
  cb_Update3DView(1);

  set_modal();
}

void stlExport_c::exportSTL(int shape)
{
  char name[1000];

  voxel_c *v = puzzle->getShape(shape);

  updateParameters(stl, params);

  stl->setBinaryMode(Binary->value() != 0);

  if (Pname->value() && Pname->value()[0] && Pname->value()[strlen(Pname->value())-1] != '/') {
      snprintf(name, 1000, "%s/%s", Pname->value(), Fname->value());
  } else {
      snprintf(name, 1000, "%s%s", Pname->value(), Fname->value());
  }

  if (fileExists(name))
  {
    if (fl_choice("File exists overwrite?", "Cancel", "Overwrite", 0) == 0)
    {
      return;
    }
  }

  try {
    stl->write(name, *v, holes);
  }

  catch (stlException_c e) {
    fl_message("%s",e.comment);
  }
  catch (...)
  {
    fl_message("The generated mesh is faulty in some way, try to tweak the parameter");
  }
}

stlExport_c::~stlExport_c(void)
{
  if (stl) delete stl;
  for (size_t i = 0; i < params.size(); i++)
    delete params[i];
}
