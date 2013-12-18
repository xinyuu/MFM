#ifndef BITFIELD_H
#define BITFIELD_H

#include "itype.h"
#include <stdio.h>

/* 
 * Try to make this a multiple of 32. Otherwise
 * there will be some wasted space.
 */
template <int bitLength>
class BitField
{
private:
  u32 m_bits[bitLength >> 5];

  u32 MakeMask(int pos, int len);

  void WriteBit(int idx, bool bit);

  bool ReadBit(int idx);
public:

  BitField();

  BitField(u32* values);

  u32 Read(int startIdx, int length);

  void Write(int startIdx, int length, u32 value);

  /* 
   * Inserts a value at startIdx. Pushes the values
   * already there down length bits.
   */
  void Insert(int startIdx, int length, u32 value);

  void Print(FILE* ostream);
			   
};

#include "bitfield.tcc"

#endif /*BITFIELD_H*/
