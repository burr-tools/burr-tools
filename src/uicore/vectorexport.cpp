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
#include "vectorexport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>

namespace btui {

  std::string_view vectorExtension(VectorFormat f) {
    switch (f) {
      case VectorFormat::PS:  return "ps";
      case VectorFormat::EPS: return "eps";
      case VectorFormat::TeX: return "tex";
      case VectorFormat::PDF: return "pdf";
      case VectorFormat::SVG: return "svg";
      case VectorFormat::PGF: return "pgf";
    }
    return "svg";
  }

  namespace {

    // --- projection ----------------------------------------------------------

    struct Projected {
      float x, y;      // page, y down
      float viewZ;     // distance from the eye along the view axis
      bool ok;         // in front of the eye
    };

    struct Projector {
      const VectorInput & in;
      Mat4 mvp;

      Projected project(const float * p) const {
        const Vec3 v{ p[0], p[1], p[2] };
        const Mat4 & m = mvp;
        const float cx = m(0, 0) * v.x + m(0, 1) * v.y + m(0, 2) * v.z + m(0, 3);
        const float cy = m(1, 0) * v.x + m(1, 1) * v.y + m(1, 2) * v.z + m(1, 3);
        const float cw = m(3, 0) * v.x + m(3, 1) * v.y + m(3, 2) * v.z + m(3, 3);
        const Mat4 & w = in.view;
        const float vz = w(2, 0) * v.x + w(2, 1) * v.y + w(2, 2) * v.z + w(2, 3);
        Projected r{ 0, 0, -vz, cw > 1e-6f };
        if (r.ok) {
          r.x = (cx / cw + 1.0f) * 0.5f * in.width;
          r.y = (1.0f - cy / cw) * 0.5f * in.height;
        }
        return r;
      }

      /* the renderer's mesh shader: view-space light, then shade() */
      void shade(const MeshVertex & v, bool lit, float alpha, VectorPrimitive & out) const {
        float f = 0.92f;
        if (lit) {
          const Mat4 & w = in.view;
          const Vec3 n = normalize(Vec3{
            w(0, 0) * v.normal[0] + w(0, 1) * v.normal[1] + w(0, 2) * v.normal[2],
            w(1, 0) * v.normal[0] + w(1, 1) * v.normal[1] + w(1, 2) * v.normal[2],
            w(2, 0) * v.normal[0] + w(2, 1) * v.normal[1] + w(2, 2) * v.normal[2] });
          f = 0.62f + 0.5f * std::max(0.0f, dot(n, normalize(Vec3{ kSceneLight[0], kSceneLight[1], kSceneLight[2] })));
        }
        auto ch = [f](std::uint8_t c) {
          const float x = c / 255.0f;
          return std::clamp(f > 1.0f ? x + (1.0f - x) * (f - 1.0f) : x * f, 0.0f, 1.0f);
        };
        out.r = ch(v.color.r);
        out.g = ch(v.color.g);
        out.b = ch(v.color.b);
        out.a = std::clamp(v.color.a / 255.0f * alpha, 0.0f, 1.0f);
      }

      float dimFactor(const MeshVertex & v) const {
        if (in.dimAxis < 0 || in.dimAxis > 2)
          return 1.0f;
        return std::fabs(v.cell[in.dimAxis] - in.dimLayer) > 0.5f ? in.dimAlpha : 1.0f;
      }

      void triangles(const std::vector<MeshVertex> & tris, bool cull, bool lit, bool dim, VectorPage & page) const {
        for (size_t i = 0; i + 2 < tris.size(); i += 3) {
          const Projected a = project(tris[i].pos), b = project(tris[i + 1].pos), c = project(tris[i + 2].pos);
          if (!a.ok || !b.ok || !c.ok)
            continue;
          // counter-clockwise is the front, as in the renderer; y runs down
          const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
          if (std::fabs(area) < 1e-6f || (cull && area > 0))
            continue;
          VectorPrimitive p;
          p.kind = VectorPrimitive::Kind::Polygon;
          p.xy = { a.x, a.y, b.x, b.y, c.x, c.y };
          shade(tris[i], lit, dim ? dimFactor(tris[i]) : 1.0f, p);
          if (p.a <= 0.002f)
            continue;
          p.depth = (a.viewZ + b.viewZ + c.viewZ) / 3.0f;
          page.prims.push_back(std::move(p));
        }
      }
    };

    // --- number output, independent of the C locale ------------------------

    // A stream in the classic locale rather than std::to_chars: macOS's libc++
    // has floating-point to_chars only from macOS 13.3 on.
    void num(std::string & s, float v) {
      if (std::fabs(v) < 0.0005f)
        v = 0;
      std::ostringstream o;
      o.imbue(std::locale::classic());
      o << std::fixed << std::setprecision(3) << double(v);
      std::string t = o.str();
      // trim trailing zeros: 12.500 -> 12.5, 3.000 -> 3
      while (!t.empty() && t.back() == '0') t.pop_back();
      if (!t.empty() && t.back() == '.') t.pop_back();
      s += t;
    }

    std::string hex(float r, float g, float b) {
      static const char * digits = "0123456789abcdef";
      std::string s = "#";
      for (float c : { r, g, b }) {
        const int v = int(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f));
        s += digits[v >> 4];
        s += digits[v & 15];
      }
      return s;
    }

    /* PostScript has no transparency: blend onto the white page */
    void flatten(const VectorPrimitive & p, float & r, float & g, float & b) {
      r = 1.0f - (1.0f - p.r) * p.a;
      g = 1.0f - (1.0f - p.g) * p.a;
      b = 1.0f - (1.0f - p.b) * p.a;
    }

    // --- writers -------------------------------------------------------------

    std::string svg(const VectorPage & pg) {
      std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<!-- Creator: BurrTools -->\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"";
      num(s, pg.width); s += "pt\" height=\""; num(s, pg.height); s += "pt\" viewBox=\"0 0 ";
      num(s, pg.width); s += ' '; num(s, pg.height); s += "\">\n<title>BurrTools</title>\n";
      for (const auto & p : pg.prims) {
        const std::string col = hex(p.r, p.g, p.b);
        if (p.kind == VectorPrimitive::Kind::Polygon) {
          s += "<polygon points=\"";
          for (size_t i = 0; i < p.xy.size(); i += 2) {
            if (i) s += ' ';
            num(s, p.xy[i]); s += ','; num(s, p.xy[i + 1]);
          }
          s += "\" fill=\"" + col + '"';
          if (p.a < 1.0f) {
            s += " fill-opacity=\""; num(s, p.a); s += '"';
          } else {
            // a hairline of the same colour hides the seams viewers draw
            // between neighbouring triangles
            s += " stroke=\"" + col + "\" stroke-width=\"0.3\" stroke-linejoin=\"round\"";
          }
          s += "/>\n";
        } else {
          s += "<line x1=\""; num(s, p.xy[0]); s += "\" y1=\""; num(s, p.xy[1]);
          s += "\" x2=\""; num(s, p.xy[2]); s += "\" y2=\""; num(s, p.xy[3]);
          s += "\" stroke=\"" + col + "\" stroke-width=\""; num(s, p.width);
          s += "\" stroke-linecap=\"round\"";
          if (p.a < 1.0f) { s += " stroke-opacity=\""; num(s, p.a); s += '"'; }
          if (p.dash > 0) { s += " stroke-dasharray=\""; num(s, p.dash); s += ' '; num(s, p.gap); s += '"'; }
          s += "/>\n";
        }
      }
      s += "</svg>\n";
      return s;
    }

    std::string postscript(const VectorPage & pg, bool eps) {
      std::string s = eps ? "%!PS-Adobe-3.0 EPSF-3.0\n" : "%!PS-Adobe-3.0\n";
      s += "%%Title: BurrTools\n%%Creator: BurrTools\n%%BoundingBox: 0 0 ";
      s += std::to_string(int(std::ceil(pg.width))) + ' ' + std::to_string(int(std::ceil(pg.height))) + '\n';
      if (!eps)
        s += "%%Pages: 1\n";
      s += "%%EndComments\n";
      if (!eps)
        s += "%%Page: 1 1\n";
      s += "1 setlinejoin 1 setlinecap\n";
      for (const auto & p : pg.prims) {
        float r, g, b;
        flatten(p, r, g, b);
        num(s, r); s += ' '; num(s, g); s += ' '; num(s, b); s += " setrgbcolor\n";
        for (size_t i = 0; i < p.xy.size(); i += 2) {
          num(s, p.xy[i]); s += ' '; num(s, pg.height - p.xy[i + 1]);
          s += i ? " lineto\n" : " moveto\n";
        }
        if (p.kind == VectorPrimitive::Kind::Polygon) {
          s += p.a < 1.0f ? "closepath fill\n" : "closepath gsave fill grestore 0.3 setlinewidth [] 0 setdash stroke\n";
        } else {
          num(s, p.width); s += " setlinewidth ";
          if (p.dash > 0) { s += '['; num(s, p.dash); s += ' '; num(s, p.gap); s += "] 0 setdash"; }
          else s += "[] 0 setdash";
          s += " stroke\n";
        }
      }
      if (!eps)
        s += "showpage\n%%Trailer\n";
      s += "%%EOF\n";
      return s;
    }

    std::string pgf(const VectorPage & pg) {
      std::string s = "% Title: BurrTools\n% Creator: BurrTools\n\\begin{pgfpicture}\n";
      s += "\\pgfpathrectangle{\\pgfpoint{0pt}{0pt}}{\\pgfpoint{";
      num(s, pg.width); s += "pt}{"; num(s, pg.height); s += "pt}}\n\\pgfusepath{use as bounding box}\n";
      auto point = [&s, &pg](float x, float y) {
        s += "{\\pgfpoint{"; num(s, x); s += "pt}{"; num(s, pg.height - y); s += "pt}}";
      };
      for (const auto & p : pg.prims) {
        s += "\\definecolor{btc}{rgb}{"; num(s, p.r); s += ','; num(s, p.g); s += ','; num(s, p.b); s += "}\n";
        if (p.kind == VectorPrimitive::Kind::Polygon) {
          s += "\\pgfsetfillcolor{btc}\n\\pgfsetfillopacity{"; num(s, p.a); s += "}\n";
        } else {
          s += "\\pgfsetstrokecolor{btc}\n\\pgfsetstrokeopacity{"; num(s, p.a); s += "}\n\\pgfsetlinewidth{";
          num(s, p.width); s += "pt}\n";
          if (p.dash > 0) { s += "\\pgfsetdash{{"; num(s, p.dash); s += "pt}{"; num(s, p.gap); s += "pt}}{0pt}\n"; }
          else s += "\\pgfsetdash{}{0pt}\n";
        }
        for (size_t i = 0; i < p.xy.size(); i += 2) {
          s += i ? "\\pgfpathlineto" : "\\pgfpathmoveto";
          point(p.xy[i], p.xy[i + 1]);
          s += '\n';
        }
        s += p.kind == VectorPrimitive::Kind::Polygon ? "\\pgfpathclose\n\\pgfusepath{fill}\n" : "\\pgfusepath{stroke}\n";
      }
      s += "\\end{pgfpicture}\n";
      return s;
    }

    std::string tex(const VectorPage & pg, const std::string & baseName) {
      // gl2psPrintTeXHeader / Footer, without primitives: the view has no text
      std::string s = "% Title: BurrTools\n% Creator: BurrTools\n"
                      "\\setlength{\\unitlength}{1pt}\n\\begin{picture}(0,0)\n\\includegraphics{";
      s += baseName.empty() ? "untitled" : baseName;
      s += "}\n\\end{picture}%\n\\begin{picture}(" + std::to_string(int(pg.width)) + ',' +
           std::to_string(int(pg.height)) + ")(0,0)\n\\end{picture}\n";
      return s;
    }

    std::string pdf(const VectorPage & pg) {
      // one graphics state per distinct opacity, for the transparent faces
      std::map<int, int> alphaState;   // opacity in 1/1000 -> state number
      auto stateOf = [&alphaState](float a) {
        const int k = int(std::lround(std::clamp(a, 0.0f, 1.0f) * 1000.0f));
        auto it = alphaState.find(k);
        if (it != alphaState.end())
          return it->second;
        const int n = int(alphaState.size());
        alphaState.emplace(k, n);
        return n;
      };

      std::string c = "1 j 1 J\n";
      for (const auto & p : pg.prims) {
        c += "/A" + std::to_string(stateOf(p.a)) + " gs\n";
        num(c, p.r); c += ' '; num(c, p.g); c += ' '; num(c, p.b);
        c += p.kind == VectorPrimitive::Kind::Polygon ? " rg " : " RG ";
        if (p.kind == VectorPrimitive::Kind::Polygon) {
          num(c, p.r); c += ' '; num(c, p.g); c += ' '; num(c, p.b); c += " RG 0.3 w [] 0 d ";
        }
        c += '\n';
        for (size_t i = 0; i < p.xy.size(); i += 2) {
          num(c, p.xy[i]); c += ' '; num(c, pg.height - p.xy[i + 1]);
          c += i ? " l\n" : " m\n";
        }
        if (p.kind == VectorPrimitive::Kind::Polygon) {
          c += p.a < 1.0f ? "h f\n" : "h B\n";
        } else {
          num(c, p.width); c += " w ";
          if (p.dash > 0) { c += '['; num(c, p.dash); c += ' '; num(c, p.gap); c += "] 0 d"; }
          else c += "[] 0 d";
          c += " S\n";
        }
      }

      std::vector<std::string> objs;
      objs.push_back("<< /Type /Catalog /Pages 2 0 R >>");
      objs.push_back("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
      std::string page = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ";
      num(page, pg.width); page += ' '; num(page, pg.height);
      page += "] /Contents 4 0 R /Resources << /ExtGState << ";
      const int firstState = 5;
      for (const auto & [k, n] : alphaState)
        page += "/A" + std::to_string(n) + ' ' + std::to_string(firstState + n) + " 0 R ";
      page += ">> >> >>";
      objs.push_back(page);
      objs.push_back("<< /Length " + std::to_string(c.size()) + " >>\nstream\n" + c + "endstream");
      std::vector<std::string> states(alphaState.size());
      for (const auto & [k, n] : alphaState) {
        std::string st = "<< /Type /ExtGState /ca ";
        num(st, k / 1000.0f); st += " /CA "; num(st, k / 1000.0f); st += " >>";
        states[size_t(n)] = st;
      }
      for (auto & st : states)
        objs.push_back(st);
      objs.push_back("<< /Title (BurrTools) /Creator (BurrTools) >>");

      std::string s = "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n";
      std::vector<size_t> offsets;
      for (size_t i = 0; i < objs.size(); i++) {
        offsets.push_back(s.size());
        s += std::to_string(i + 1) + " 0 obj\n" + objs[i] + "\nendobj\n";
      }
      const size_t xref = s.size();
      s += "xref\n0 " + std::to_string(objs.size() + 1) + "\n0000000000 65535 f \n";
      for (size_t o : offsets) {
        char buf[24];
        std::snprintf(buf, sizeof buf, "%010zu 00000 n \n", o);
        s += buf;
      }
      s += "trailer\n<< /Size " + std::to_string(objs.size() + 1) + " /Root 1 0 R /Info " +
           std::to_string(objs.size()) + " 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
      return s;
    }
  }

  VectorPage projectScene(const VectorInput & in) {
    VectorPage page;
    page.width = in.width;
    page.height = in.height;
    if (in.width <= 0 || in.height <= 0)
      return page;

    const Projector pr{ in, in.projection * in.view };
    if (in.mesh) {
      pr.triangles(in.mesh->opaque, in.cullBackFaces, in.lighting, true, page);
      pr.triangles(in.mesh->translucent, true, in.lighting, true, page);
    }
    if (in.overlayFaces)
      pr.triangles(*in.overlayFaces, false, false, false, page);

    if (in.lines) {
      for (const auto & l : *in.lines) {
        const float a3[3] = { l.a.x, l.a.y, l.a.z }, b3[3] = { l.b.x, l.b.y, l.b.z };
        const Projected a = pr.project(a3), b = pr.project(b3);
        if (!a.ok || !b.ok)
          continue;
        VectorPrimitive p;
        p.kind = VectorPrimitive::Kind::Line;
        p.xy = { a.x, a.y, b.x, b.y };
        p.r = l.color.r / 255.0f;
        p.g = l.color.g / 255.0f;
        p.b = l.color.b / 255.0f;
        p.a = l.color.a / 255.0f;
        p.width = l.widthDp;
        p.dash = l.dashDp;
        p.gap = l.gapDp;
        // a line on a face sorts in front of it, as the renderer's
        // less-or-equal depth test draws it
        p.depth = (a.viewZ + b.viewZ) * 0.5f - 0.01f;
        page.prims.push_back(std::move(p));
      }
    }

    std::stable_sort(page.prims.begin(), page.prims.end(),
                     [](const VectorPrimitive & x, const VectorPrimitive & y) { return x.depth > y.depth; });
    return page;
  }

  std::string writeVector(const VectorPage & page, VectorFormat f, const std::string & baseName) {
    switch (f) {
      case VectorFormat::PS:  return postscript(page, false);
      case VectorFormat::EPS: return postscript(page, true);
      case VectorFormat::TeX: return tex(page, baseName);
      case VectorFormat::PDF: return pdf(page);
      case VectorFormat::SVG: return svg(page);
      case VectorFormat::PGF: return pgf(page);
    }
    return svg(page);
  }
}
