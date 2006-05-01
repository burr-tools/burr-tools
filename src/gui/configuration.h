/* Burr Solver
 * Copyright (C) 2000-2006 Andreas Röver
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
#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <stdio.h>

/* this module contains a class for configuration file
 * handling loading and saving is handled
 */

class configuration_c {

public:

  configuration_c(void);
  ~configuration_c(void);

  bool useTooltips(void) { return i_use_tooltips; }
  void useTooltips(bool val) { i_use_tooltips = val; }

  bool useLightning(void) { return i_use_lightning; }
  void useLightning(bool val) { i_use_lightning = val; }

  bool useRubberband(void) { return i_use_rubberband; }
  void useRubberband(bool val) { i_use_rubberband = val; }

  bool useBlendedRemoving(void) { return i_use_blendedRemoving; }
  void useBlendedRemoving(bool val) { i_use_blendedRemoving = val; }

  int windowPosX(void) { return i_window_pos_x; }
  int windowPosY(void) { return i_window_pos_y; }
  int windowPosW(void) { return i_window_pos_w; }
  int windowPosH(void) { return i_window_pos_h; }
  void windowPos(int x, int y, int w, int h) {
    i_window_pos_x = x;
    i_window_pos_y = y;
    i_window_pos_w = w;
    i_window_pos_h = h;
  }

  void dialog(void);

private:

  typedef enum {
    CT_BOOL,
    CT_STRING,
    CT_INT,
  } cnf_type;

  void parse(FILE *in);
  void register_entry(char *cnf_name, cnf_type  cnf_typ, void *cnf_var, long maxlen, bool dialog, char * dtext);

  typedef struct config_data {
    config_data *next;
    char     *cnf_name;  // name of entry in configuration file
    cnf_type  cnf_typ;   // data type of the variable
    void     *cnf_var;   // pointer to the variable
    long      maxlen;    // maximum length (for strings)
    bool      dialog;    // shall it be visible in the config dialog
    char *    dialogText;
    void *    widget;    // used in the dialog to save pointer to the widget
  } config_data;

  config_data *first_data;

  bool i_use_tooltips;
  bool i_use_lightning;
  bool i_use_rubberband;
  bool i_use_blendedRemoving;

  int i_window_pos_x;
  int i_window_pos_y;
  int i_window_pos_w;
  int i_window_pos_h;
};

extern configuration_c config;

#endif
