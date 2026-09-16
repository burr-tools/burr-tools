#ifndef BTTEST_STL_READER_H
#define BTTEST_STL_READER_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/*
 * A minimal STL reader, for checking the exporters by parsing back what
 * they emit.
 *
 * The mesh/STL design doc is specific about why this exists rather than a
 * byte comparison against a fixture, and equally specific about the trap in
 * writing it: it must be written from the FORMAT, not from the writer, or
 * the two will agree on a shared misunderstanding and the test will confirm
 * nothing.
 *
 * So, from the format:
 *
 *   Binary STL is an 80-byte header of arbitrary content, then a
 *   little-endian uint32 triangle count, then that many 50-byte records.
 *   Each record is twelve little-endian 32-bit floats -- a normal followed
 *   by three vertices, x then y then z -- and a 16-bit attribute count,
 *   conventionally zero.
 *
 *   ASCII STL is "solid <name>", then for each facet:
 *     facet normal <nx> <ny> <nz>
 *       outer loop
 *         vertex <x> <y> <z>   (three times)
 *       endloop
 *     endfacet
 *   and finally "endsolid".
 *
 * The reader below is deliberately tolerant about whitespace and strict
 * about structure: a malformed file should raise rather than silently
 * yield fewer triangles, since "fewer triangles than expected" is exactly
 * the failure a lenient reader would hide.
 */

namespace bttest {

struct StlTriangle {
  float normal[3];
  float vertex[3][3];
};

struct StlMesh {
  std::string name;              //< the solid name, ASCII only
  std::vector<StlTriangle> triangles;
};

/** true when the first bytes look like an ASCII STL rather than a binary one */
inline bool looksLikeAsciiStl(const std::string & raw) {
  /* The conventional sniff. It is not airtight -- a binary file could
     begin with these bytes -- so the cases that care pass the encoding
     explicitly rather than relying on it. */
  return raw.size() >= 5 && raw.compare(0, 5, "solid") == 0;
}

inline StlMesh readAsciiStl(const std::string & raw) {
  StlMesh mesh;
  std::istringstream in(raw);

  std::string token;
  if (!(in >> token) || token != "solid")
    throw std::runtime_error("ascii stl: expected 'solid'");

  /* the rest of the line is the name; it may be empty */
  std::getline(in, mesh.name);
  while (!mesh.name.empty() && (mesh.name[0] == ' ' || mesh.name[0] == '\t'))
    mesh.name.erase(0, 1);

  while (in >> token) {
    if (token == "endsolid") break;

    if (token != "facet")
      throw std::runtime_error("ascii stl: expected 'facet', got '" + token + "'");

    StlTriangle t{};

    if (!(in >> token) || token != "normal")
      throw std::runtime_error("ascii stl: expected 'normal'");
    for (int i = 0; i < 3; i++)
      if (!(in >> t.normal[i])) throw std::runtime_error("ascii stl: short normal");

    if (!(in >> token) || token != "outer") throw std::runtime_error("ascii stl: expected 'outer'");
    if (!(in >> token) || token != "loop")  throw std::runtime_error("ascii stl: expected 'loop'");

    for (int v = 0; v < 3; v++) {
      if (!(in >> token) || token != "vertex")
        throw std::runtime_error("ascii stl: expected 'vertex'");
      for (int i = 0; i < 3; i++)
        if (!(in >> t.vertex[v][i])) throw std::runtime_error("ascii stl: short vertex");
    }

    if (!(in >> token) || token != "endloop")  throw std::runtime_error("ascii stl: expected 'endloop'");
    if (!(in >> token) || token != "endfacet") throw std::runtime_error("ascii stl: expected 'endfacet'");

    mesh.triangles.push_back(t);
  }

  return mesh;
}

inline StlMesh readBinaryStl(const std::string & raw) {
  if (raw.size() < 84)
    throw std::runtime_error("binary stl: shorter than the 84-byte header");

  StlMesh mesh;

  /* the 80-byte header carries the title the writer put there; it is
     NUL-padded, so stop at the first NUL */
  size_t n = 0;
  while (n < 80 && raw[n] != '\0') n++;
  mesh.name.assign(raw.data(), n);

  uint32_t count = 0;
  std::memcpy(&count, raw.data() + 80, 4);

  const size_t expected = 84 + size_t(count) * 50;
  if (raw.size() < expected)
    throw std::runtime_error("binary stl: file is shorter than its triangle count claims");

  mesh.triangles.reserve(count);

  for (uint32_t i = 0; i < count; i++) {
    const char * rec = raw.data() + 84 + size_t(i) * 50;

    StlTriangle t{};
    std::memcpy(t.normal, rec, 12);
    std::memcpy(t.vertex[0], rec + 12, 12);
    std::memcpy(t.vertex[1], rec + 24, 12);
    std::memcpy(t.vertex[2], rec + 36, 12);

    mesh.triangles.push_back(t);
  }

  return mesh;
}

/** the whole file as bytes */
inline std::string slurp(const std::string & path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("could not open " + path);
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

/** signed volume of a triangle soup, by the divergence theorem */
inline double stlVolume(const StlMesh & m) {
  double total = 0;

  for (const StlTriangle & t : m.triangles) {
    const float * a = t.vertex[0];
    const float * b = t.vertex[1];
    const float * c = t.vertex[2];

    total += double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1])
           - double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0])
           + double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0]);
  }

  return total / 6.0;
}

} // namespace bttest

#endif
