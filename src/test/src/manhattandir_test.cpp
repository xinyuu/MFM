#include "assert.h"
#include "manhattandir_test.h"

namespace MFM {

void ManhattanDirTest::Test_manhattandirConversion()
{
  SPoint longPt(2, 2);
  SPoint shortPt(1, -1);
  
  u8 longBits  = ManhattanDir<4>::get().FromPoint(longPt, MANHATTAN_TABLE_LONG);
  u8 shortBits = ManhattanDir<4>::get().FromPoint(shortPt, MANHATTAN_TABLE_SHORT);

  SPoint out(0, 0);

  ManhattanDir<4>::get().FillFromBits(out, longBits, MANHATTAN_TABLE_LONG);

  assert(out.GetX() == 2);
  assert(out.GetY() == 2);

  ManhattanDir<4>::get().FillFromBits(out, shortBits, MANHATTAN_TABLE_SHORT);

  assert(out.GetX() == 1);
  assert(out.GetY() == -1);
}
} /* namespace MFM */

