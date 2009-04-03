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
#ifndef __BITFIELD_H__
#define __BITFIELD_H__

#include <inttypes.h>

#include "bt_assert.h"

/** \file bitfield.h
 * contains the template class bitfield_c
 */

template<int bits>

/**
 * this is a fast class to have a bit vector with a constant number of bits
 * the number it dependend on the template parameter
 */
class bitfield_c {

  private:

    uint64_t field[(bits+63)/64]; ///< the bitfield

  public:

    /// constructor, all bits are cleared at the beginning
    bitfield_c() {
      clear();
    }

    /// constructor, the bits are initialized to the value of the string
    bitfield_c(const char * val) {
      clear();
      operator=(val);
    }

    /// copy constructor
    bitfield_c(const bitfield_c<bits>& orig) {
      memcpy(field, orig.field, 8*((bits+63)/64));
    }

    /// get a bit
    bool get(uint16_t pos) const {
      bt_assert(pos < bits);
      return field[pos >> 6] & (1ll << (pos & 63));
    }

    /// set a bit to one
    void set(uint16_t pos) {
      bt_assert(pos < bits);
      field[pos >> 6] |= (1ll << (pos & 63));
    }

    /// set a bit to zero
    void reset(uint16_t pos) {
      bt_assert(pos < bits);
      field[pos >> 6] &= ~(1ll << (pos & 63));
    }

    /// set all bits to zero
    void clear(void) {
      memset(field, 0, 8*((bits+63)/64));
    }

    /// check, if at least one bit is set to 1
    bool notNull(void) {
      for (int i = 0; i < ((bits+63)/64); i++)
        if (field[i] > 0)
          return true;

      return false;
    }

    /**
     *  assign the the string repesentation of a value to
     *  the bits of this bitfield
     *
     *  The string must be a hexadecimal value, no prefix no
     *  suffix, you can use upper and lower case letters.
     *
     *  The string must not be larget than allowed. If the string
     *  is shorter than necessary the remaining values are left untouched,
     *  so it's up to you to clear them before calling this function
     */
    const bitfield_c<bits> & operator=(const char * val) {

      size_t l = strlen(val);

      bt_assert(4*l <= bits);

      int fpos = 0;
      int bitpos = 0;
      field[0] = 0;

      for (int i = l-1; i >= 0; i--) {
        uint64_t cval = 0;

        if ((val[i] >= '0') && (val[i] <= '9'))
          cval = val[i]-'0';
        else if ((val[i] >= 'a') && (val[i] <= 'f'))
          cval = val[i] - 'a' + 10;
        else if ((val[i] >= 'A') && (val[i] <= 'F'))
          cval = val[i] - 'A' + 10;
        else
          bt_assert(0);

        field[fpos] |= cval << bitpos;

        bitpos += 4;
        if (bitpos >= 64) {
          bitpos = 0;
          fpos++;
          field[fpos] = 0;
        }
      }

      return *this;
    }

    /// comparison of 2 bitfields
    bool operator==(const bitfield_c<bits> & op) const {

      for (int i = 0; i < ((bits+63)/64); i++)
        if (op.field[i] != field[i])
          return false;

      return true;
    }

    /// inequality test of 2 bitfields
    bool operator!=(const bitfield_c<bits> & op) const {

      for (int i = 0; i < ((bits+63)/64); i++)
        if (op.field[i] != field[i])
          return true;

      return false;
    }

    /**
     * find the number of set bits in the bitfield
     *
     * The method add up the number of set bits in
     * each 64 bit entry of the bitvector. This value is
     * calculated using a method similar to this:
     * http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
     */
    unsigned int countbits(void) const {

      unsigned int res = 0;

      for (int i = 0; i < ((bits+63)/64); i++) {
        uint64_t s = field[i];

        s -= ((s >> 1) & 0x5555555555555555ll);
        s = (((s >> 2) & 0x3333333333333333ll) + (s & 0x3333333333333333ll));
        s = (((s >> 4) + s) & 0x0f0f0f0f0f0f0f0fll);
        s += (s >> 8);
        s += (s >> 16);
        s += (s >> 32);

        res += (s & 0x3f);
      }

      return res;
    }

    /** put the value of the bitfield into the string str.
     * not more than len characters are written
     */
    void print(char * str, unsigned int len) const {

      int idx = 0;

      for (int i = ((bits+63)/64)-1; i >= 0; i--)
        idx += snprintf(str+idx, len-idx, "%016llx", field[i]);
    }

    /**
     * use printf to display the content of the string
     */
    void print(void) const {

      for (int i = ((bits+63)/64)-1; i >= 0; i--)
        printf("%016llx", field[i]);
    }

    /**
     * bitwise and of 2 bitfields
     */
    const bitfield_c<bits> operator&(const bitfield_c<bits> & right) const {
      bitfield_c<bits> res;

      for (int i = 0; i < ((bits+63)/64); i++)
        res.field[i] = field[i] & right.field[i];

      return res;
    }

    /**
     * bitwise or of 2 bitfields
     */
    const bitfield_c<bits> operator|(const bitfield_c<bits> & right) const {
      bitfield_c<bits> res;

      for (int i = 0; i < ((bits+63)/64); i++)
        res.field[i] = field[i] | right.field[i];

      return res;
    }

};


#endif
