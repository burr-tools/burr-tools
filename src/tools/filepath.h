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
#ifndef __FILE_PATH_H__
#define __FILE_PATH_H__

#include <string>

/* The directory part of a file path, without a trailing separator, or an
 * empty string when the path names no directory at all.
 *
 * The export dialogs use this to default their output location to wherever
 * the current puzzle lives. An empty result means "the caller must supply a
 * fallback" — deciding what that fallback is (the dialogs use homedir()) is
 * policy and deliberately stays out of here, so this remains a pure
 * function that src/tools can carry and test_burrtools can test.
 */
std::string directoryOfFile(const std::string & path);

#endif
