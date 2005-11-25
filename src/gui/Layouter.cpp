#include "Layouter.h"

/* task      what we want to do with the endresult
 * widths    the widths of the different columns of the endgrid
 * heights   the heights of the rows in the calculated grid
 * widgetsW  the minimum widths calculated for each widget
 * widgetsH  the minimum heights for each widget
 * targetW and targetH only used when task = 1, it should be bigger than min size
 *      or strange thing may happen
 */
void layouter_c::calcLayout(int task, std::vector<int> *widths, std::vector<int> *heights,
                            std::vector<int> *widgetsW, std::vector<int> *widgetsH, int targetW, int targetH) const {

  /* right this routine is stolen from the gridbaglayouter of the gcc java awt class set */

  /* task = 0: minsize; task = 1: SIZE */

  /* these 2 contain the maximum column and rownumber */
  unsigned int max_x = 0;
  unsigned int max_y = 0;

  /* as the caller is not required to provide the last two vectors
   * we'll have local instances and set the pointer to them if
   * we need that
   */
  std::vector<int> ww, wh;

  if (!widgetsW) widgetsW = &ww;
  if (!widgetsH) widgetsH = &wh;

  /* resize the vectors to the correct size */
  widgetsW->resize(children());
  widgetsH->resize(children());

  Fl_Widget *const * _widgets = array();

  // first we figure out how many rows/columns
  for (int i = 0; i < children(); i++) {

    unsigned int gX, gY, gW, gH;

    layoutable_c * widget = dynamic_cast<layoutable_c*>(_widgets[i]);

    widget->getGridValues(&gX, &gY, &gW, &gH);

    int w, h;

    widget->getMinSize(&w, &h);

    /* check the min sizes of the current widget */

    if (w < widget->getMinWidth()) w = widget->getMinWidth();
    if (h < widget->getMinHeight()) h = widget->getMinHeight();

    (*widgetsW)[i] = w;
    (*widgetsH)[i] = h;

    max_x = (max_x > gX+gW) ? (max_x) : (gX+gW);
    max_y = (max_y > gY+gH) ? (max_y) : (gY+gH);
  }

  std::vector<int> weightsW(max_x), weightsH(max_y);
  widths->resize(max_x);
  heights->resize(max_y);

  /* initialize the vectors */
  for (unsigned int ii = 0; ii < max_x; ii++) {
    (*widths)[ii] = 0;
    weightsW[ii] = 0;
  }

  for (unsigned int ii = 0; ii < max_y; ii++) {
    (*heights)[ii] = 0;
    weightsH[ii] = 0;
  }

  /* separate the process into two steps. First put in the
   * widgets that have a fixed column and then place the other
   * ones. The wider ones can now have a more optimal
   * stretching of the column
   */
  for (int i = 0; i < children(); i++) {
    unsigned int gX, gY, gW, gH;

    layoutable_c * widget = dynamic_cast<layoutable_c*>(_widgets[i]);

    widget->getGridValues(&gX, &gY, &gW, &gH);

    if (gW > 1) continue;

    int width = (*widgetsW)[i];

    width += 2 * widget->getPitch();

    if (width > (*widths)[gX])
      (*widths)[gX] = width;

    if (widget->getWeightX() > weightsW[gX])
      weightsW[gX] = widget->getWeightX();
  }

  for (int i = 0; i < children(); i++) {
    unsigned int gX, gY, gW, gH;

    layoutable_c * widget = dynamic_cast<layoutable_c*>(_widgets[i]);

    widget->getGridValues(&gX, &gY, &gW, &gH);

    if (gW < 2) continue;

    /* find the column with the biggest weight */
    unsigned int bestW = gX;
    for (unsigned int w = gX + 1; w < gX+gW; w++)
      if (weightsW[w] >= weightsW[bestW])
        bestW = w;

    int width = (*widgetsW)[i];

    width += 2 * widget->getPitch();

    for (unsigned int w = gX; w < gX+gW; w++)
      if (w != bestW)
        width -= (*widths)[w];

    if (width < 0) width = 0;

    if (width > (*widths)[bestW])
      (*widths)[bestW] = width;

    if (widget->getWeightX() > weightsW[bestW])
      weightsW[bestW] = widget->getWeightX();
  }

  /* calculate rows, see columns */
  for (int i = 0; i < children(); i++) {
    unsigned int gX, gY, gW, gH;

    layoutable_c * widget = dynamic_cast<layoutable_c*>(_widgets[i]);

    widget->getGridValues(&gX, &gY, &gW, &gH);

    if (gH > 1) continue;

    int height = (*widgetsH)[i];

    height += 2 * widget->getPitch();

    if (height > (*heights)[gY])
      (*heights)[gY] = height;

    if (widget->getWeightY() > weightsH[gY])
      weightsH[gY] = widget->getWeightY();
  }

  for (int i = 0; i < children(); i++) {
    unsigned int gX, gY, gW, gH;

    layoutable_c * widget = dynamic_cast<layoutable_c*>(_widgets[i]);

    widget->getGridValues(&gX, &gY, &gW, &gH);

    if (gH < 2) continue;

    /* find the column with the biggest weight */
    unsigned int bestH = gY;
    for (unsigned int h = gY + 1; h < gY+gH; h++)
      if (weightsH[h] >= weightsH[bestH])
        bestH = h;

    int height = (*widgetsH)[i];

    height += 2 * widget->getPitch();

    for (unsigned int h = gY; h < gY+gH; h++)
      if (h != bestH)
        height -= (*heights)[h];

    if (height < 0) height = 0;

    if (height > (*heights)[bestH])
      (*heights)[bestH] = height;

    if (widget->getWeightY() > weightsH[bestH])
      weightsH[bestH] = widget->getWeightY();
  }

  if (task == 1) {
    /* stretch to final size if we do an actual layout we need to do this
     * here because the weight vectors are not exported
     */

    /* sum up the weight values */
    int sumWeightX = 0, sumWeightY = 0, sumW = 0, sumH = 0;

    for (unsigned int x = 0; x < max_x; x++) {
      sumWeightX += weightsW[x];
      sumW += (*widths)[x];
    }
    for (unsigned int y = 0; y < max_y; y++) {
      sumWeightY += weightsH[y];
      sumH += (*heights)[y];
    }

    /* now add the missing difference to the rows and columns */
    int dW = targetW - sumW;
    int dH = targetH - sumH;

    if (dW > 0)
      for (unsigned int x = 0; x < max_x; x++) {

        int add = (sumWeightX) ? (dW * weightsW[x] / sumWeightX) : (dW / (max_x-x));

        (*widths)[x] += add;
        dW -= add;
        sumWeightX -= weightsW[x];

        if (dW <= 0)
          break;
      }

    if (dH > 0)
      for (unsigned int y = 0; y < max_y; y++) {

        int add = (sumWeightY) ? (dH * weightsH[y] / sumWeightY) : (dH / (max_y-y));

        (*heights)[y] += add;
        dH -= add;
        sumWeightY -= weightsH[y];

        if (dH <= 0)
          break;
      }
  }
}

void layouter_c::resize(int xt, int yt, int w, int h) {

  /* first step calculate the widths of the rows and columns */
  std::vector<int> widths;
  std::vector<int> heights;

  std::vector<int> widgetW;
  std::vector<int> widgetH;

  calcLayout(1, &widths, &heights, &widgetW, &widgetH, w, h);

  /* check, if we need to make our widget bigger to accommodate all subwidgets */
  int wi = 0, hi = 0;
  for (unsigned int i = 0; i < widths.size(); i++)
    wi += widths[i];
  for (unsigned int i = 0; i < heights.size(); i++)
    hi += heights[i];

//  if (wi > w) w = wi;
//  if (hi > h) h = hi;

  Fl_Group::resize(xt, yt, w, h);

  Fl_Widget *const * _widgets = array();

  /* and now layout widgets according to the plan */
  for (int i = 0; i < children(); i++) {

    int xp, yp, w, h;
    xp = yp = h = w = 0;

    unsigned int gx, gy, gw, gh;

    layoutable_c * widget = dynamic_cast<layoutable_c*>(_widgets[i]);

    widget->getGridValues(&gx, &gy, &gw, &gh);

    for (unsigned int j = 0; j < gx; j++) xp += widths[j];
    for (unsigned int j = gx; j < gx+gw; j++) w += widths[j];

    for (unsigned int j = 0; j < gy; j++) yp += heights[j];
    for (unsigned int j = gy; j < gy+gh; j++) h += heights[j];

    /* pitch handling */
    int p = widget->getPitch();
    xp += p;
    yp += p;
    w -= 2*p;
    h -= 2*p;

    unsigned int minHW = (widgetW[i] < widgetH[i]) ? (widgetW[i]) : (widgetH[i]);

    /* stretch handling */
    switch (widget->getStretchX()) {
    case STRETCH_MINUS:
      w = widgetW[i];
      break;
    case STRETCH_PLUS:
      xp += w - widgetW[i];
      w = widgetW[i];
      break;
    case STRETCH_MIDDLE:
      xp += (w - widgetW[i]) / 2;
      w = widgetW[i];
      break;
    case STRETCH_SQUARE:
      xp += (w - minHW) / 2;
      w = minHW;
      break;
    case STRETCH:
      break;
    }

    switch (widget->getStretchY()) {
    case STRETCH_MINUS:
      h = widgetH[i];
      break;
    case STRETCH_PLUS:
      yp += h - widgetH[i];
      h = widgetH[i];
      break;
    case STRETCH_MIDDLE:
      yp += (h - widgetH[i]) / 2;
      h = widgetH[i];
      break;
    case STRETCH_SQUARE:
      yp += (h - minHW) / 2;
      h = minHW;
      break;
    case STRETCH:
      break;
    }

    /* finally place the widget */
    _widgets[i]->resize(xp+x(), yp+y(), w, h);
  }
}

void layouter_c::getMinSize(int *width, int *height) const {

  std::vector<int> widths;
  std::vector<int> heights;

  *width = 0;
  *height = 0;

  if (children()) {

    calcLayout(0, &widths, &heights, 0, 0);

    /* accumulate the rows and columns */
    for (unsigned int i = 0; i < widths.size(); i++) *width += widths[i];
    for (unsigned int i = 0; i < heights.size(); i++) *height += heights[i];
  }
}

