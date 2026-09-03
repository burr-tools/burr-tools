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
 */
#ifndef __SOLVER_TYPE_H__
#define __SOLVER_TYPE_H__

#include <cstring>

/**
 * Which solver pipeline to use.
 *
 * BurrTools Classic: complete take-apart + single-thread DLX.  bt_classic_solver.h
 * Andrew Crowell:    incomplete 90° take-apart, same DLX.      crowell_solver.h
 * BurrTools 2:       Classic take-apart + dancing-cells        bt2_solver.h
 *                    assembly with split / work stealing.
 *
 * Comparison of all three: bt_classic_solver.h
 */
enum solverType_e {
  SOLVER_CLASSIC = 0,
  SOLVER_CROWELL = 1
};

inline unsigned int solverTypeCount(void) {
  return 2;
}

inline const char * solverTypeLabel(solverType_e t) {
  switch (t) {
    case SOLVER_CLASSIC: return "BurrTools Classic";
    case SOLVER_CROWELL: return "Andrew Crowell";
  }
  return "BurrTools Classic";
}

inline solverType_e solverTypeFromIndex(int v) {
  if (v < 0 || (unsigned int)v >= solverTypeCount())
    return SOLVER_CLASSIC;
  return (solverType_e)v;
}

/* Lowercase and drop spaces / hyphens / underscores for CLI and aliases. */
inline void solverTypeNormalize(const char * s, char * out, unsigned int outLen) {
  unsigned int j = 0;
  if (!s || outLen == 0) {
    if (outLen)
      out[0] = 0;
    return;
  }
  for (unsigned int i = 0; s[i] && j + 1 < outLen; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == ' ' || c == '-' || c == '_')
      continue;
    if (c >= 'A' && c <= 'Z')
      c = (unsigned char)(c - 'A' + 'a');
    out[j++] = (char)c;
  }
  out[j] = 0;
}

inline bool solverTypeFromName(const char * s, solverType_e * out) {
  if (!s || !out)
    return false;

  char n[64];
  solverTypeNormalize(s, n, sizeof(n));
  if (n[0] == 0)
    return false;

  if (strcmp(n, "classic") == 0 ||
      strcmp(n, "burrtoolsclassic") == 0 ||
      strcmp(n, "btclassic") == 0) {
    *out = SOLVER_CLASSIC;
    return true;
  }
  if (strcmp(n, "crowell") == 0 ||
      strcmp(n, "andrewcrowell") == 0 ||
      strcmp(n, "fortran") == 0) {
    *out = SOLVER_CROWELL;
    return true;
  }
  return false;
}

inline const char * solverTypeCliNames(void) {
  return "\"BurrTools Classic\", crowell, \"Andrew Crowell\"";
}

inline const char * solverTypeTooltip(void) {
  return " Solver engine. BurrTools Classic is the original complete take-apart "
         "search and a single-thread dancing-links assembly. Andrew Crowell uses "
         "Sliding-Cube 90° heuristics (faster on rotation puzzles, can miss "
         "disassemblies). ";
}

#endif
