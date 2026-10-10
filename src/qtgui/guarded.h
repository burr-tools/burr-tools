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
#ifndef BTQT_GUARDED_H
#define BTQT_GUARDED_H

#include "../lib/bt_assert.h"

#include <exception>
#include <type_traits>

/* App::handleInternalError() for code that cannot include app.h (it
 * includes the controllers): rescue-save, report, then QML quits. */
void reportInternalError(const std::exception & e);

/* Run the body of an entry point QML calls. An internal error raised in it --
 * directly, or in a slot one of its signals reaches, such as the scene
 * rebuild -- must not unwind through the QML engine: it is reported here, and
 * the entry point returns a default value (false, 0) instead.
 */
template <class F>
std::invoke_result_t<F> guarded(F && body) {
  try {
    return body();
  } catch (const assert_exception & e) {
    reportInternalError(e);
    if constexpr (!std::is_void_v<std::invoke_result_t<F>>)
      return {};
  }
}

#endif
