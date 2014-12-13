/* -*- mode:C++ -*- */

#include "Logger.h"
namespace MFM {
  template <class CC, u32 BITS>
  u32 StaticLoader<CC,BITS>::m_counter = 2;

  template <class CC, u32 BITS>
  const UUID *(StaticLoader<CC,BITS>::m_uuids[SLOTS]);

  template <class CC, u32 BITS>
  u32 StaticLoader<CC,BITS>::NextType() {
    u32 type;
    do {
      ++m_counter;
      s32 highidx = 31;
      while (highidx > 1 && ((m_counter & (1 << highidx)) == 0))
        --highidx;

      u32 bitsPerBit = BITS / highidx;
      u32 bits = (1<<bitsPerBit) - 1;

      type = 0;
      while (--highidx >= 0) {
        type <<= bitsPerBit;
        if (m_counter & (1 << highidx)) {
          type |= bits;
        }
      }
      LOG.Debug("trying 0x%04x",type);
    } while (m_uuids[type] != 0);
    LOG.Debug("taking 0x%04x",type);
    return type;
  }

  template <class CC, u32 BITS>
  u32 StaticLoader<CC,BITS>::AllocateEmptyType(const UUID & forUUID) {
    return AllocateTypeInternal(forUUID, (s32) CC::ATOM_TYPE::ATOM_EMPTY_TYPE);
  }

  template <class CC, u32 BITS>
  u32 StaticLoader<CC,BITS>::AllocateType(const UUID & forUUID) {
    return AllocateTypeInternal(forUUID, -1);
  }

  template <class CC, u32 BITS>
  u32 StaticLoader<CC,BITS>::AllocateTypeInternal(const UUID & forUUID, s32 useThisType) {
    u32 used = 0;
    for (u32 i = 0; i < SLOTS; ++i) {
      if (!m_uuids[i]) continue;
      ++used;

      if (forUUID == *m_uuids[i])
        FAIL(DUPLICATE_ENTRY);
    }

    if (used == SLOTS)
      FAIL(OUT_OF_ROOM);

    u32 type;
    if (useThisType >= 0)
    {
      type = (u32) useThisType;
    } else {
      type = NextType();
    }

    if (type >= SLOTS)
      FAIL(ILLEGAL_STATE);

    m_uuids[type] = &forUUID;
    return type;
  }

  template <class CC, u32 BITS>
  s32 StaticLoader<CC,BITS>::TypeFromUUID(const UUID & forUUID) {
    for (u32 i = 0; i < SLOTS; ++i) {
      if (!m_uuids[i]) continue;
      if (forUUID == *m_uuids[i])
        return (s32) i;
    }
    return -1;
  }

  template <class CC, u32 BITS>
  s32 StaticLoader<CC,BITS>::TypeFromCompatibleUUID(const UUID & forUUID) {
    for (u32 i = 0; i < SLOTS; ++i) {
      if (!m_uuids[i]) continue;
      if (m_uuids[i]->Compatible(forUUID))
        return (s32) i;
    }
    return -1;
  }
}
