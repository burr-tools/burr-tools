#ifndef __VECTOR_EXPORT_WINDOW_H__
#define __VECTOR_EXPORT_WINDOW_H__

#include "Layouter.h"

#include "voxelframe.h"

class vectorExportWindow_c : public LFl_Double_Window {

    LFl_Input * inp;
    layouter_c * radGroup;

  public:

    vectorExportWindow_c(void);

    const char * getFileName(void);
    voxelFrame_c::VectorFiletype getVectorType(void);

    /* callback functions */
    void cb_FileChoose(void);

    bool cancelled;

};

#endif
