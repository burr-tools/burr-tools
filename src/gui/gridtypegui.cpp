#include "gridtypegui.h"
#include "guigridtype.h"

#include "../lib/gridtype.h"

#include "Layouter.h"

gridTypeGui_0_c::gridTypeGui_0_c(int x, int y, int w, int h, gridType_c * gt) {

  LFl_Frame *fr;

  {
    fr = new LFl_Frame(0, 0);

    new LFl_Check_Button("Length X different from Length Y", 0, 0);
    new LFl_Check_Button("Length X different from Length Z", 0, 1);
    new LFl_Check_Button("Length Y different from Length Z", 0, 2);

    fr->end();
  }
  {
    fr = new LFl_Frame(0, 1);

    new LFl_Check_Button("Angle between X and Y Axis is 90", 0, 0);
    new LFl_Check_Button("Angle between X and Z Axis is 90", 0, 1);
    new LFl_Check_Button("Angle between Y and Z Axis is 90", 0, 2);

    fr->end();
  }
  {
    fr = new LFl_Frame(0, 2);

    new LFl_Check_Button("X-Y Axis and X-Z Axis Angles are equal", 0, 0);
    new LFl_Check_Button("X-Y Axis and Y-Z Axis Angles are equal", 0, 1);
    new LFl_Check_Button("X-Z Axis and Y-Z Axis Angles are equal", 0, 2);

    fr->end();
  }

  end();
}

class gridTypeInfos_c {
  public:

    gridType_c * gt;
    guiGridType_c * ggt;

    gridTypeInfos_c(gridType_c * g) : gt(g), ggt(new guiGridType_c(g)) {}
};

gridTypeParameterWindow_c::gridTypeParameterWindow_c(guiGridType_c * ggt) {
  label("Set parameters for grid type");

  ggt->getCofigurationDialog(0, 0, 1, 1);

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



}

gridType_c * gridTypeSelectorWindow_c::getGridType(void) {
  gridType_c * tmp = gti[current]->gt;
  gti[current]->gt = 0;
  return tmp;
}

