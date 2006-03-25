#include "gridtypegui.h"

#include "guigridtype.h"
#include "Layouter.h"

#include "../lib/gridtype.h"

#include <FL/Fl_Browser_.H>
#include <FL/Fl_Image.H>

static void cb_gridTypeGui0Update(Fl_Widget *o, void *v) { ((gridTypeGui_0_c*)(v))->updateValues(); }
void gridTypeGui_0_c::updateValues(void) {

  unsigned char bb = 0;
  if (arxy->value()) bb |= 1;
  if (arxz->value()) bb |= 2;
  if (aryz->value()) bb |= 4;

  switch (bb) {
    case 0:
      axyeaxz->activate();
      axyeayz->activate();
      axzeayz->activate();
      break;
    case 1:
      axyeaxz->deactivate(); axyeaxz->set();
      axyeayz->deactivate(); axyeayz->set();
      axzeayz->activate();
      break;
    case 2:
      axyeaxz->deactivate(); axyeaxz->set();
      axyeayz->activate();
      axzeayz->deactivate(); axzeayz->set();
      break;
    case 3:
      axyeaxz->deactivate(); axyeaxz->clear();
      axyeayz->deactivate(); axyeayz->set();
      axzeayz->deactivate(); axzeayz->clear();
      break;
    case 4:
      axyeaxz->activate();
      axyeayz->deactivate(); axyeayz->set();
      axzeayz->deactivate(); axzeayz->set();
      break;
    case 5:
      axyeaxz->deactivate(); axyeaxz->set();
      axyeayz->deactivate(); axyeayz->clear();
      axzeayz->deactivate(); axzeayz->clear();
      break;
    case 6:
      axyeaxz->deactivate(); axyeaxz->clear();
      axyeayz->deactivate(); axyeayz->clear();
      axzeayz->deactivate(); axzeayz->set();
      break;
    case 7:
      axyeaxz->deactivate(); axyeaxz->clear();
      axyeayz->deactivate(); axyeayz->clear();
      axzeayz->deactivate(); axzeayz->clear();
      break;
  }

  gt->setBrickXneY(lxy->value());
  gt->setBrickXneZ(lxz->value());
  gt->setBrickYneZ(lyz->value());
  gt->setBrickAngleOrthoXY(arxy->value());
  gt->setBrickAngleOrthoXZ(arxz->value());
  gt->setBrickAngleOrthoYZ(aryz->value());
  gt->setBrickAngleXYneXZ(axyeaxz->value());
  gt->setBrickAngleXYneYZ(axyeayz->value());
  gt->setBrickAngleXZneYZ(axzeayz->value());
}

gridTypeGui_0_c::gridTypeGui_0_c(int x, int y, int w, int h, gridType_c * g) : gt(g) {

  LFl_Frame *fr;

  {
    fr = new LFl_Frame(0, 0);

    lxy = new LFl_Check_Button("Length X different from Length Y", 0, 0);
    lxz = new LFl_Check_Button("Length X different from Length Z", 0, 1);
    lyz = new LFl_Check_Button("Length Y different from Length Z", 0, 2);

    if (gt->getBrickXneY()) lxy->set();
    if (gt->getBrickXneZ()) lxz->set();
    if (gt->getBrickYneZ()) lyz->set();

    fr->end();
  }
  {
    fr = new LFl_Frame(0, 1);

    arxy = new LFl_Check_Button("Angle between X and Y Axis is 90", 0, 0);
    arxz = new LFl_Check_Button("Angle between X and Z Axis is 90", 0, 1);
    aryz = new LFl_Check_Button("Angle between Y and Z Axis is 90", 0, 2);

    if (gt->getBrickAngleOrthoXY()) arxy->set();
    if (gt->getBrickAngleOrthoXZ()) arxz->set();
    if (gt->getBrickAngleOrthoYZ()) aryz->set();

    fr->end();
  }
  {
    fr = new LFl_Frame(0, 2);

    axyeaxz = new LFl_Check_Button("X-Y Axis and X-Z Axis Angles are different", 0, 0);
    axyeayz = new LFl_Check_Button("X-Y Axis and Y-Z Axis Angles are different", 0, 1);
    axzeayz = new LFl_Check_Button("X-Z Axis and Y-Z Axis Angles are different", 0, 2);

    if (gt->getBrickAngleXYneXZ()) axyeaxz->set();
    if (gt->getBrickAngleXYneYZ()) axyeayz->set();
    if (gt->getBrickAngleXZneYZ()) axzeayz->set();

    fr->end();
  }


  lxy->callback(cb_gridTypeGui0Update, this);
  lxz->callback(cb_gridTypeGui0Update, this);
  lyz->callback(cb_gridTypeGui0Update, this);
  arxy->callback(cb_gridTypeGui0Update, this);
  arxz->callback(cb_gridTypeGui0Update, this);
  aryz->callback(cb_gridTypeGui0Update, this);
  axyeaxz->callback(cb_gridTypeGui0Update, this);
  axyeayz->callback(cb_gridTypeGui0Update, this);
  axzeayz->callback(cb_gridTypeGui0Update, this);

  end();

  updateValues();
}

class gridTypeInfos_c {
  public:

    gridType_c * gt;
    guiGridType_c * ggt;

    gridTypeInfos_c(gridType_c * g) : gt(g), ggt(new guiGridType_c(g)) {}
};

gridTypeParameterWindow_c::gridTypeParameterWindow_c(guiGridType_c * ggt) {
  label("Set parameters for grid type");

  ggt->getConfigurationDialog(0, 0, 1, 1);

  (new LFl_Button("Close", 0, 1))->pitch(7);
}

gridTypeSelectorWindow_c::gridTypeSelectorWindow_c(void) {

  /* for each grid type available we need to greate an instance
   * here and put it into a gridtypeinfo class and into the
   * vector. This vector will be later on the one
   * with all required information
   */
  gti.push_back(new gridTypeInfos_c(new gridType_c));

  /* from here on the code should not need changes when new grid types are added */

  label("Select space grid");

  LFl_Frame *fr;

  /* first the selector for all grid types */
  {
    fr = new LFl_Frame(0, 0);

    for (unsigned int i = 0; i < gti.size(); i++) {
      LFl_Radio_Button * b = new LFl_Radio_Button(gti[i]->ggt->getName(), 0, i);
      if (i == 0)
        b->set();
    }

    fr->end();
  }

  /* now all the parameter boxes, only the first one is visible, the others are hidden for now */
  {
    fr = new LFl_Frame(1, 0);

    for (unsigned int i = 0; i < gti.size(); i++) {
      gridTypeGui_c * g = gti[i]->ggt->getConfigurationDialog(0, 0, 1, 1);

      if (i != 0)
        g->hide();
    }

    fr->end();
  }

  /* finally the 3D view that contains exactly one voxel */


  /* now the buttons */
  {
    layouter_c * l = new layouter_c(0, 1, 3, 1);

    (new LFl_Button("Ok", 0, 0))->pitch(7);

    l->end();
  }

  current = 0;
}

gridTypeSelectorWindow_c::~gridTypeSelectorWindow_c(void) {
  for (unsigned int i = 0; i < gti.size(); i++) {
    if (gti[i]->gt)
      delete gti[i]->gt;
    delete gti[i]->ggt;
    delete gti[i];
  }
}


gridType_c * gridTypeSelectorWindow_c::getGridType(void) {
  gridType_c * tmp = gti[current]->gt;
  gti[current]->gt = 0;
  return tmp;
}

