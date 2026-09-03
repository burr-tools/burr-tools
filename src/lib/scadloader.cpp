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
#include "scadloader.h"

#include "voxel.h"
#include "puzzle.h"
#include "problem.h"
#include "gridtype.h"

#include <cctype>
#include <cstdio>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

namespace {

const unsigned int MAX_DIM = 256;

class ScadParser {
  const std::string &s;
  size_t i;

public:
  explicit ScadParser(const std::string &text) : s(text), i(0) {}

  size_t pos(void) const { return i; }
  void setPos(size_t p) { i = p; }
  bool eof(void) const { return i >= s.size(); }

  void skip(void) {
    while (!eof()) {
      if (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r') {
        i++;
        continue;
      }
      if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
        i += 2;
        while (!eof() && s[i] != '\n')
          i++;
        continue;
      }
      if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
        i += 2;
        while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/'))
          i++;
        if (i + 1 < s.size())
          i += 2;
        continue;
      }
      break;
    }
  }

  char peek(void) {
    skip();
    return eof() ? 0 : s[i];
  }

  bool eat(char c) {
    skip();
    if (!eof() && s[i] == c) {
      i++;
      return true;
    }
    return false;
  }

  bool parseString(std::string &out) {
    skip();
    if (eof() || s[i] != '"')
      return false;
    i++;
    out.clear();
    while (!eof() && s[i] != '"') {
      if (s[i] == '\\' && i + 1 < s.size()) {
        i++;
        out.push_back(s[i]);
        i++;
        continue;
      }
      out.push_back(s[i]);
      i++;
    }
    if (eof())
      return false;
    i++; /* closing quote */

    /* OpenSCAD concatenates adjacent string literals */
    skip();
    while (!eof() && s[i] == '"') {
      std::string more;
      if (!parseString(more))
        return false;
      out += more;
      skip();
    }
    return true;
  }

  /* Skip one OpenSCAD value (string, array, number, identifier, call, ...). */
  bool skipValue(void) {
    skip();
    if (eof())
      return false;

    if (s[i] == '"') {
      std::string dummy;
      return parseString(dummy);
    }

    if (s[i] == '[') {
      i++;
      skip();
      if (!eat(']')) {
        while (true) {
          if (!skipValue())
            return false;
          skip();
          if (eat(']'))
            break;
          if (!eat(','))
            return false;
          skip();
          if (eat(']'))
            break;
        }
      }
      return true;
    }

    if (s[i] == '(') {
      return skipMatched('(', ')');
    }

    if (s[i] == '{') {
      return skipMatched('{', '}');
    }

    if (s[i] == '$' || s[i] == '_' || isalpha((unsigned char)s[i])) {
      if (s[i] == '$')
        i++;
      while (!eof() && (s[i] == '_' || isalnum((unsigned char)s[i])))
        i++;
      skip();
      if (!eof() && s[i] == '(')
        return skipMatched('(', ')');
      return true;
    }

    if (s[i] == '-' || s[i] == '+' || s[i] == '.' || isdigit((unsigned char)s[i])) {
      if (s[i] == '-' || s[i] == '+')
        i++;
      while (!eof() && (isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
        i++;
      return true;
    }

    return false;
  }

  bool skipMatched(char open, char close) {
    skip();
    if (eof() || s[i] != open)
      return false;
    int depth = 0;
    bool inStr = false;
    while (!eof()) {
      char c = s[i++];
      if (inStr) {
        if (c == '\\' && !eof())
          i++;
        else if (c == '"')
          inStr = false;
        continue;
      }
      if (c == '"') {
        inStr = true;
        continue;
      }
      if (c == open)
        depth++;
      else if (c == close) {
        depth--;
        if (depth == 0)
          return true;
      }
    }
    return false;
  }

  /* Skip `$name = value` style named arguments. */
  void skipNamedArgs(void) {
    while (true) {
      size_t saved = i;
      skip();
      if (eof())
        return;
      if (s[i] != '$' && s[i] != '_' && !isalpha((unsigned char)s[i]))
        return;
      if (s[i] == '$')
        i++;
      while (!eof() && (s[i] == '_' || isalnum((unsigned char)s[i])))
        i++;
      skip();
      if (!eat('=')) {
        i = saved;
        return;
      }
      if (!skipValue()) {
        i = saved;
        return;
      }
      skip();
      eat(',');
    }
  }

  bool parseStringList(std::vector<std::string> &layers) {
    if (!eat('['))
      return false;
    layers.clear();
    skip();
    if (eat(']'))
      return true;
    while (true) {
      std::string str;
      if (!parseString(str))
        return false;
      layers.push_back(str);
      skip();
      if (eat(']'))
        return true;
      if (!eat(','))
        return false;
      skip();
      if (eat(']'))
        return true;
    }
  }

  /* A piece is either a string (one Z layer) or a vector of layer strings. */
  bool parsePiece(std::vector<std::string> &layers) {
    skip();
    if (eof())
      return false;
    if (s[i] == '"') {
      std::string str;
      if (!parseString(str))
        return false;
      layers.clear();
      layers.push_back(str);
      return true;
    }
    if (s[i] == '[')
      return parseStringList(layers);
    return false;
  }

  bool parsePlate(std::vector<std::vector<std::string> > &pieces) {
    if (!eat('['))
      return false;
    pieces.clear();
    skip();
    if (eat(']'))
      return true;
    while (true) {
      std::vector<std::string> piece;
      if (!parsePiece(piece))
        return false;
      pieces.push_back(piece);
      skip();
      if (eat(']'))
        return true;
      if (!eat(','))
        return false;
      skip();
      if (eat(']'))
        return true;
    }
  }
};

static bool isIdentStart(char c) {
  return c == '_' || isalpha((unsigned char)c);
}

static bool isIdentChar(char c) {
  return c == '_' || isalnum((unsigned char)c);
}

struct ScadCall {
  std::string name;
  size_t argsPos; /* index of '(' */
};

/* Scan the file, skipping comments and strings, and collect puzzlecad calls
 * that are not disabled with the OpenSCAD '*' prefix. */
static void findCalls(const std::string &s, std::vector<ScadCall> &calls) {
  size_t i = 0;
  char lastSig = 0;
  enum { NORM, LINE, BLOCK, STR } st = NORM;

  while (i < s.size()) {
    char c = s[i];

    if (st == LINE) {
      if (c == '\n')
        st = NORM;
      i++;
      continue;
    }
    if (st == BLOCK) {
      if (c == '*' && i + 1 < s.size() && s[i + 1] == '/') {
        st = NORM;
        i += 2;
      } else {
        i++;
      }
      continue;
    }
    if (st == STR) {
      if (c == '\\' && i + 1 < s.size())
        i += 2;
      else {
        if (c == '"')
          st = NORM;
        i++;
      }
      continue;
    }

    if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      st = LINE;
      i += 2;
      continue;
    }
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
      st = BLOCK;
      i += 2;
      continue;
    }
    if (c == '"') {
      st = STR;
      i++;
      continue;
    }

    if (isIdentStart(c) && (i == 0 || !isIdentChar(s[i - 1]))) {
      size_t start = i;
      i++;
      while (i < s.size() && isIdentChar(s[i]))
        i++;
      std::string word = s.substr(start, i - start);
      if (word == "burr_plate" || word == "burr_piece" || word == "packing_box") {
        if (lastSig != '*') {
          ScadCall call;
          call.name = word;
          call.argsPos = i;
          calls.push_back(call);
        }
      }
      lastSig = 'I';
      continue;
    }

    if (!isspace((unsigned char)c))
      lastSig = c;
    i++;
  }
}

static void decodeLayer(const std::string &layer, std::vector<std::vector<char> > &rows) {
  rows.clear();
  std::vector<char> row;
  for (size_t i = 0; i < layer.size(); ) {
    if (layer[i] == '{') {
      int depth = 1;
      i++;
      while (i < layer.size() && depth > 0) {
        if (layer[i] == '{')
          depth++;
        else if (layer[i] == '}')
          depth--;
        i++;
      }
      continue;
    }
    if (layer[i] == '|') {
      rows.push_back(row);
      row.clear();
      i++;
      continue;
    }
    if (layer[i] == ' ' || layer[i] == '\t' || layer[i] == '\r' || layer[i] == '\n') {
      i++;
      continue;
    }
    row.push_back(layer[i]);
    i++;
  }
  rows.push_back(row);
}

/* Convert puzzlecad layer strings into a cubic voxel shape.
 * Returns false if the piece is empty or too large. */
static bool addShapeFromLayers(puzzle_c *p, const std::vector<std::string> &layers,
                               const std::string &name, unsigned int *idOut) {
  if (layers.empty())
    return false;

  std::vector<std::vector<std::vector<char> > > decoded;
  unsigned int sx = 0, sy = 0;
  const unsigned int sz = (unsigned int)layers.size();

  for (size_t z = 0; z < layers.size(); z++) {
    std::vector<std::vector<char> > rows;
    decodeLayer(layers[z], rows);
    if (rows.size() > sy)
      sy = (unsigned int)rows.size();
    for (size_t y = 0; y < rows.size(); y++)
      if (rows[y].size() > sx)
        sx = (unsigned int)rows[y].size();
    decoded.push_back(rows);
  }

  if (sx == 0 || sy == 0 || sz == 0)
    return false;
  if (sx > MAX_DIM || sy > MAX_DIM || sz > MAX_DIM)
    return false;

  bool anyFilled = false;
  for (unsigned int z = 0; z < sz && !anyFilled; z++)
    for (unsigned int y = 0; y < decoded[z].size() && !anyFilled; y++)
      for (unsigned int x = 0; x < decoded[z][y].size(); x++)
        if (decoded[z][y][x] != '.') {
          anyFilled = true;
          break;
        }

  if (!anyFilled)
    return false;

  unsigned int id = p->addShape(sx, sy, sz);
  voxel_c *sh = p->getShape(id);
  sh->setName(name);

  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < decoded[z].size(); y++)
      for (unsigned int x = 0; x < decoded[z][y].size(); x++)
        if (decoded[z][y][x] != '.')
          sh->setState(x, y, z, voxel_c::VX_FILLED);

  if (idOut)
    *idOut = id;
  return true;
}

} // namespace

puzzle_c * loadOpenScadPuzzle(std::istream * str) {

  if (!str)
    return 0;

  std::string text((std::istreambuf_iterator<char>(*str)),
                   std::istreambuf_iterator<char>());

  std::vector<ScadCall> calls;
  findCalls(text, calls);
  if (calls.empty())
    return 0;

  puzzle_c * p = new puzzle_c(new gridType_c());
  problem_c * pr = p->getProblem(p->addProblem());
  pr->setName("Problem");

  std::vector<std::string> resultLayers;
  unsigned int pieceCount = 0;

  for (size_t c = 0; c < calls.size(); c++) {
    ScadParser parser(text);
    parser.setPos(calls[c].argsPos);
    if (!parser.eat('('))
      continue;
    parser.skipNamedArgs();

    if (calls[c].name == "burr_plate") {
      std::vector<std::vector<std::string> > pieces;
      if (!parser.parsePlate(pieces))
        continue;
      for (size_t n = 0; n < pieces.size(); n++) {
        char nm[32];
        snprintf(nm, sizeof(nm), "Piece %u", pieceCount + 1);
        unsigned int id;
        if (addShapeFromLayers(p, pieces[n], nm, &id)) {
          pr->setShapeMinimum(id, 1);
          pieceCount++;
        }
      }
    } else if (calls[c].name == "burr_piece") {
      std::vector<std::string> layers;
      if (!parser.parsePiece(layers))
        continue;
      char nm[32];
      snprintf(nm, sizeof(nm), "Piece %u", pieceCount + 1);
      unsigned int id;
      if (addShapeFromLayers(p, layers, nm, &id)) {
        pr->setShapeMinimum(id, 1);
        pieceCount++;
      }
    } else if (calls[c].name == "packing_box") {
      std::vector<std::string> layers;
      if (parser.parsePiece(layers) && resultLayers.empty())
        resultLayers = layers;
    }
  }

  if (pieceCount == 0) {
    delete p;
    return 0;
  }

  pr->setName("Puzzle");

  if (!resultLayers.empty()) {
    unsigned int id;
    if (addShapeFromLayers(p, resultLayers, "Result", &id))
      pr->setResultId(id);
  }

  if (!pr->resultValid()) {
    unsigned int cube = 1;
    for (unsigned int s = 0; s < p->getNumberOfShapes(); s++) {
      const voxel_c * v = p->getShape(s);
      if (v->countState(voxel_c::VX_FILLED) == 0 &&
          v->countState(voxel_c::VX_VARIABLE) == 0)
        continue;
      unsigned int dx = v->boundX2() - v->boundX1() + 1;
      unsigned int dy = v->boundY2() - v->boundY1() + 1;
      unsigned int dz = v->boundZ2() - v->boundZ1() + 1;
      if (dx > cube) cube = dx;
      if (dy > cube) cube = dy;
      if (dz > cube) cube = dz;
    }
    if (cube > MAX_DIM)
      cube = MAX_DIM;

    unsigned int id = p->addShape(cube, cube, cube);
    voxel_c * sh = p->getShape(id);
    sh->setName("Result");
    sh->skipRecalcBoundingBox(true);
    for (unsigned int i = 0; i < sh->getXYZ(); i++)
      sh->setState(i, voxel_c::VX_VARIABLE);
    sh->skipRecalcBoundingBox(false);
    pr->setResultId(id);
  }

  return p;
}

static bool shapeToLayers(const voxel_c * v, std::vector<std::string> & layers) {
  layers.clear();
  if (!v)
    return false;
  if (v->countState(voxel_c::VX_FILLED) == 0 &&
      v->countState(voxel_c::VX_VARIABLE) == 0)
    return false;

  unsigned int x0 = v->boundX1(), x1 = v->boundX2();
  unsigned int y0 = v->boundY1(), y1 = v->boundY2();
  unsigned int z0 = v->boundZ1(), z1 = v->boundZ2();

  for (unsigned int z = z0; z <= z1; z++) {
    std::string layer;
    for (unsigned int y = y0; y <= y1; y++) {
      if (y > y0)
        layer += '|';
      for (unsigned int x = x0; x <= x1; x++)
        layer += (v->getState(x, y, z) == voxel_c::VX_EMPTY) ? '.' : 'x';
    }
    layers.push_back(layer);
  }
  return !layers.empty();
}

static std::string baseName(const char * path) {
  if (!path || !path[0])
    return "unknown";
  std::string n(path);
  size_t slash = n.find_last_of("/\\");
  if (slash != std::string::npos)
    n = n.substr(slash + 1);
  return n.empty() ? "unknown" : n;
}

bool saveOpenScadPuzzle(std::ostream & str, const puzzle_c * p,
                        const char * sourceName, unsigned int problemIndex) {
  if (!p)
    return false;

  std::vector<unsigned int> shapeIds;
  int skipResult = -1;

  if (problemIndex < p->getNumberOfProblems()) {
    const problem_c * pr = p->getProblem(problemIndex);
    if (pr->resultValid())
      skipResult = (int)pr->getResultId();
    for (unsigned int i = 0; i < pr->getNumberOfParts(); i++) {
      unsigned int id = pr->getShapeIdOfPart(i);
      if ((int)id == skipResult)
        continue;
      bool seen = false;
      for (size_t k = 0; k < shapeIds.size(); k++)
        if (shapeIds[k] == id)
          seen = true;
      if (!seen)
        shapeIds.push_back(id);
    }
  }

  if (shapeIds.empty()) {
    for (unsigned int s = 0; s < p->getNumberOfShapes(); s++)
      if ((int)s != skipResult)
        shapeIds.push_back(s);
  }

  std::vector<std::vector<std::string> > pieces;
  for (size_t i = 0; i < shapeIds.size(); i++) {
    std::vector<std::string> layers;
    if (shapeToLayers(p->getShape(shapeIds[i]), layers))
      pieces.push_back(layers);
  }

  if (pieces.empty())
    return false;

  str <<
    "include <puzzlecad.scad>\n"
    "\n"
    "// This model was generated by BurrTools export for the file:\n"
    "// " << baseName(sourceName) << "\n"
    "\n"
    "// You can freely edit this file to make changes to the model structure or parameters.\n"
    "\n"
    "require_puzzlecad_version(\"2.3.1\");\n"
    "\n"
    "$plate_width = 240;\n"
    "$plate_sep = 3;\n"
    "$unit_beveled = false;\n"
    "$auto_layout = true;\n"
    "\n"
    "// Use burr_scal = 15.7 to match live cubes\n"
    "$burr_scale = 15.7;\n"
    "$burr_bevel = 1.2;\n"
    "\n"
    "$burr_inset = 0.05;\n"
    "$joint_inset= 0.01;\n"
    "\n"
    "$box_wall_thickness = 4;\n"
    "$thatch_thickness = 4;\n"
    "\n"
    "*packing_box(\n"
    "\n"
    "$box_wall_thickness = 4\n"
    ");\n"
    "\n"
    "burr_plate([\n";

  for (size_t n = 0; n < pieces.size(); n++) {
    str << "    [ ";
    for (size_t z = 0; z < pieces[n].size(); z++) {
      if (z)
        str << "      ";
      str << '"' << pieces[n][z] << '"';
      if (z + 1 < pieces[n].size())
        str << ",\n";
    }
    str << " ],\n";
  }

  str << "]);\n";
  return (bool)str;
}
