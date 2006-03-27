#ifndef __GRID_TYPE_GUI_H__
#define __GRID_TYPE_GUI_H__

#include "Layouter.h"

class gridType_c;
class guiGridType_c;

/* this is a fltk group that contains all elements to change the
 * parameters that a certain gridtype can have
 *
 * in fact this is only the base class, for each gridtype there will be
 * a inherited class thata contains the elements
 */
class gridTypeGui_c : public layouter_c {
};

/* the required elements for the brick grid type */
class gridTypeGui_0_c : public gridTypeGui_c {

  private:

    gridType_c * gt;

    LFl_Check_Button *lxy, *lxz, *lyz;
    LFl_Check_Button *arxy, *arxz, *aryz;
    LFl_Check_Button *axyeaxz, *axyeayz, *axzeayz;

  public:

    gridTypeGui_0_c(int x, int y, int w, int h, gridType_c * gt);

    void updateValues(void);
};

class gridTypeGui_1_c : public gridTypeGui_c {

  public:

    gridTypeGui_1_c(int x, int y, int w, int h, gridType_c * gt);
};

/* this window allows you to edit the parameters of one
 * grid type
 */
class gridTypeParameterWindow_c : public LFl_Double_Window {

  public:

    /* creates the parameter window for the given gui grid type */
    gridTypeParameterWindow_c(guiGridType_c * ggt);
};

/* this class is used int eh window below to hold all data necessary
 * for one grid type
 */
class gridTypeInfos_c;

/* this window allows you to select the grid type and
 * the parameters for the selected type
 */

class gridTypeSelectorWindow_c : public LFl_Double_Window {

  private:

    std::vector<gridTypeInfos_c*> gti;

    /* currently selected grid type from the vector above */
    unsigned int current;

  public:

    gridTypeSelectorWindow_c(void);
    ~gridTypeSelectorWindow_c(void);

    /* after the window has been close you can get the created grid type with this function
     */
    gridType_c * getGridType(void);


    void select_cb(void);
};

#endif
