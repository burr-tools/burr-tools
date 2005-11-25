/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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
#include "ImageExport.h"

ImageExportWindow::ImageExportWindow(void) {
  LFl_Frame *fr;

  fr = new LFl_Frame(0, 0, 1, 2);
  new LFl_Round_Button("White Background", 0, 0);
  new LFl_Round_Button("Transparent Background", 0, 1);
  (new LFl_Box(0, 2))->weight(0, 1);
  fr->end();

  fr = new LFl_Frame(0, 2);
  new LFl_Round_Button("No Antialiasing", 0, 0);
  new LFl_Round_Button("2x2 Supersampling", 0, 1);
  new LFl_Round_Button("3x3 Supersampling", 0, 2);
  new LFl_Round_Button("4x4 Supersampling", 0, 3);
  new LFl_Round_Button("5x5 Supersampling", 0, 4);
  (new LFl_Box(0, 5))->weight(0, 1);
  fr->end();

  // user defined size input
  fr = new LFl_Frame(1, 1, 1, 2);
  (new LFl_Box("Size X", 0, 0))->stretchRight();
  (new LFl_Box("Size Y", 0, 1))->stretchRight();
  (new LFl_Box("DPI", 0, 2))->stretchRight();
  (new LFl_Box("Pixel X", 0, 3))->stretchRight();
  (new LFl_Box("Pixel Y", 0, 4))->stretchRight();

  (new LFl_Box(1, 0))->setMinimumSize(2, 0);

  new LFl_Input(2, 0);
  new LFl_Input(2, 1);
  new LFl_Input(2, 2);
  new LFl_Input(2, 3);
  new LFl_Input(2, 4);

  (new LFl_Box(3, 0))->setMinimumSize(2, 0);

  new LFl_Box("mm", 4, 0);
  new LFl_Box("mm", 4, 1);

  (new LFl_Box(0, 5))->weight(0, 1);

  fr->end();

  layouter_c * l = new layouter_c(0, 3, 2, 1);
  (new LFl_Box())->weight(1, 0);
  (new LFl_Button("Export Image(s)", 1, 0))->pitch(7);
  (new LFl_Button("Abbort", 2, 0))->pitch(7);
  l->end();

}

