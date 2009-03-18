/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#ifndef __INTDIV_H__
#define __INTDIV_H__

/**
 * Integer division with round towards minus infinity,
 *
 * taken from python
 * The type must be a signed integer type
 */

template <class T>
inline T intdiv_inf(T x, T y)
{
  return x/y - ((x%y && (x%y^y)<0) ? 1 : 0);
}

#if 0

/* the original Python function which also calculates the remainder */

static enum divmod_result
i_divmod(register long x, register long y,
         long *p_xdivy, long *p_xmody)
{
	long xdivy, xmody;

	if (y == 0) {
		PyErr_SetString(PyExc_ZeroDivisionError,
				"integer division or modulo by zero");
		return DIVMOD_ERROR;
	}
	/* (-sys.maxint-1)/-1 is the only overflow case. */
	if (y == -1 && UNARY_NEG_WOULD_OVERFLOW(x))
		return DIVMOD_OVERFLOW;
	xdivy = x / y;
	xmody = x - xdivy * y;
	/* If the signs of x and y differ, and the remainder is non-0,
	 * C89 doesn't define whether xdivy is now the floor or the
	 * ceiling of the infinitely precise quotient.  We want the floor,
	 * and we have it iff the remainder's sign matches y's.
	 */
	if (xmody && ((y ^ xmody) < 0) /* i.e. and signs differ */) {
		xmody += y;
		--xdivy;
		assert(xmody && ((y ^ xmody) >= 0));
	}
	*p_xdivy = xdivy;
	*p_xmody = xmody;
	return DIVMOD_OK;
}

#endif

#endif

