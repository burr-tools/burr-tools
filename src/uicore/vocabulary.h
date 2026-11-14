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
#ifndef BTUI_VOCABULARY_H
#define BTUI_VOCABULARY_H

#include "camera.h"
#include "layoutmodel.h"
#include "scenemesh.h"

#include <array>
#include <optional>
#include <string_view>

/* The settings' and modes' fixed vocabularies, one table each: the values
 * and the words the settings file and QML name them by. A word is read and
 * written through its table only, so the file, the controllers and the QML
 * cannot drift apart, and a word the table does not know (a hand-edited
 * file, a newer version's) falls back to the table's default.
 */
namespace btui {

  template <class E> struct Term {
    E value{};
    std::string_view word;
  };

  template <class E, std::size_t N> struct Vocabulary {
    std::array<Term<E>, N> terms;
    E fallback;                                    ///< what an unknown word means

    constexpr std::optional<E> find(std::string_view w) const {
      for (const Term<E> & t : terms)
        if (t.word == w)
          return t.value;
      return std::nullopt;
    }
    constexpr E parse(std::string_view w) const { return find(w).value_or(fallback); }
    constexpr std::string_view word(E v) const {
      for (const Term<E> & t : terms)
        if (t.value == v)
          return t.word;
      return word(fallback);
    }
  };

  template <class E, class... T> constexpr auto vocabulary(E fallback, T... terms) {
    return Vocabulary<E, sizeof...(T)>{ { terms... }, fallback };
  }

  // --- Settings (C12) ----------------------------------------------------------

  /* the colour scheme; Light by default, as legacy looks (the spec's
   * default is System), an unknown word System */
  enum class ThemeChoice { System, Light, Dark };
  inline constexpr auto kThemes = vocabulary(ThemeChoice::System,
    Term<ThemeChoice>{ ThemeChoice::System, "system" },
    Term<ThemeChoice>{ ThemeChoice::Light, "light" },
    Term<ThemeChoice>{ ThemeChoice::Dark, "dark" });
  inline constexpr ThemeChoice kDefaultTheme = ThemeChoice::Light;

  inline constexpr auto kDensities = vocabulary(Density::Standard,
    Term<Density>{ Density::Standard, "standard" },
    Term<Density>{ Density::Minimal, "minimal" });

  inline constexpr auto kRotationMethods = vocabulary(Camera::RotationMethod::Drag,
    Term<Camera::RotationMethod>{ Camera::RotationMethod::Drag, "drag" },
    Term<Camera::RotationMethod>{ Camera::RotationMethod::Arcball, "arcball" });

  inline constexpr auto kVoxelStyles = vocabulary(VoxelStyle::Flat,
    Term<VoxelStyle>{ VoxelStyle::Flat, "flat" },
    Term<VoxelStyle>{ VoxelStyle::Legacy, "legacy" },
    Term<VoxelStyle>{ VoxelStyle::Stl, "stl" });

  /* the 3D view's multisampling; 4x by default */
  enum class Antialiasing { Off, X2, X4, X8 };
  inline constexpr auto kAntialiasing = vocabulary(Antialiasing::X4,
    Term<Antialiasing>{ Antialiasing::Off, "off" },
    Term<Antialiasing>{ Antialiasing::X2, "2x" },
    Term<Antialiasing>{ Antialiasing::X4, "4x" },
    Term<Antialiasing>{ Antialiasing::X8, "8x" });

  /* the samples per pixel: 1, 2, 4 or 8 */
  constexpr int samples(Antialiasing a) {
    switch (a) {
      case Antialiasing::Off: return 1;
      case Antialiasing::X2: return 2;
      case Antialiasing::X4: return 4;
      case Antialiasing::X8: return 8;
    }
    return 4;
  }

  // --- the 3D view's modes and Display menu (C06) ------------------------------

  enum class NavMode { Orbit, Pan };
  inline constexpr auto kNavModes = vocabulary(NavMode::Orbit,
    Term<NavMode>{ NavMode::Orbit, "orbit" },
    Term<NavMode>{ NavMode::Pan, "pan" });

  inline constexpr auto kProjections = vocabulary(Camera::Projection::Perspective,
    Term<Camera::Projection>{ Camera::Projection::Perspective, "perspective" },
    Term<Camera::Projection>{ Camera::Projection::Orthographic, "orthographic" });

  inline constexpr auto kColourViews = vocabulary(ColorMode::Piece,
    Term<ColorMode>{ ColorMode::Piece, "piece" },
    Term<ColorMode>{ ColorMode::Voxel, "voxel" });

  // --- Export ▸ Image (C17) ----------------------------------------------------

  /* what the image shows, in the dialog's order */
  enum class ImageMode { Shape, Problem, Assembly, Solution, Disassembly };
  inline constexpr auto kImageModes = vocabulary(ImageMode::Solution,
    Term<ImageMode>{ ImageMode::Shape, "shape" },
    Term<ImageMode>{ ImageMode::Problem, "problem" },
    Term<ImageMode>{ ImageMode::Assembly, "assembly" },
    Term<ImageMode>{ ImageMode::Solution, "solution" },
    Term<ImageMode>{ ImageMode::Disassembly, "disassembly" });

  enum class Paper { Manual, A4Portrait, A4Landscape, LetterPortrait, LetterLandscape };
  inline constexpr auto kPapers = vocabulary(Paper::Manual,
    Term<Paper>{ Paper::Manual, "manual" },
    Term<Paper>{ Paper::A4Portrait, "a4p" },
    Term<Paper>{ Paper::A4Landscape, "a4l" },
    Term<Paper>{ Paper::LetterPortrait, "letterp" },
    Term<Paper>{ Paper::LetterLandscape, "letterl" });
}

#endif
