/* -*- C++ -*- */
#include "MDist.h"
#include "Element_Empty.h"
#include "Logger.h"
#include "AtomSerializer.h"
#include "Util.h"

namespace MFM
{
  template <class EC>
  Tile<EC>::Tile(const u32 tileSide, S * sites)
    : TILE_SIDE(tileSide)
    , OWNED_SIDE(TILE_SIDE - 2 * EVENT_WINDOW_RADIUS)  // This OWNED_SIDE computation is duplicated in Grid.h!
    , m_sites(sites)
    , m_cdata(*this)
    , m_lockAttempts(0)
    , m_lockAttemptsSucceeded(0)
    , m_window(*this)
    , m_state(OFF)
    , m_enabled(true)
    , m_backgroundRadiation(false)
    , m_requestedState(OFF)
    , m_warpFactor(3)
  {
    // TILE_SIDE can't be too small, and we must apparently have sites..
    MFM_API_ASSERT_ARG(TILE_SIDE >= 3*EVENT_WINDOW_RADIUS && m_sites != 0);

    // Require even TILE_SIDE.  (The 'GetSquareDistanceFromCenter'
    // computation would be cheaper if TILE_SIDE was guaranteed odd,
    // but that violates a MFM tradition that is now cast in stone.)
    MFM_API_ASSERT_ARG(2 * TILE_SIDE / 2 == TILE_SIDE);

    Init();
  }

  template <class EC>
  void Tile<EC>::Init()
  {
    m_elementTable.Reinit();

    Element_Empty<EC>::THE_INSTANCE.AllocateEmptyType();
    RegisterElement(Element_Empty<EC>::THE_INSTANCE);

    ClearAtoms();
    ClearTileParameters();

    m_dirIterator.Shuffle(m_random);

  }

  template <class EC>
  typename EC::ATOM_CONFIG::ATOM_TYPE Tile<EC>::GetEmptyAtom() const
  {
    return Element_Empty<EC>::THE_INSTANCE.GetDefaultAtom();
  }

  template <class EC>
  void Tile<EC>::SingleXRay(const SPoint & at, u32 bitOdds)
  {
    Random & random = GetRandom();
    GetWritableAtom(at)->XRay(random, bitOdds);
  }

  template <class EC>
  void Tile<EC>::XRay(u32 siteOdds, u32 bitOdds)
  {
    Random & random = GetRandom();
    for(iterator_type i = begin(); i != end(); ++i) {
      if (random.OneIn(siteOdds))
        i->GetAtom().XRay(random, bitOdds);
    }
  }

  template <class EC>
  void Tile<EC>::ClearAtoms()
  {
    for(iterator_type i = begin(); i != end(); ++i) {
      i->Clear();
    }
    NeedAtomRecount();
  }

  template <class EC>
  void Tile<EC>::Connect(AbstractChannel& channel, LonglivedLock & lock, Dir toCache)
  {
    CacheProcessor<EC> & cxn = GetCacheProcessor(toCache);

    MFM_API_ASSERT_STATE(!cxn.IsConnected());

    cxn.ClaimCacheProcessor(*this, channel, lock, toCache);
  }

  template <class EC>
  CacheProcessor<EC> & Tile<EC>::GetCacheProcessor(Dir toCache)
  {
    return const_cast<CacheProcessor<EC>&>(static_cast<const Tile<EC>*>(this)->GetCacheProcessor(toCache));
  }

  template <class EC>
  const CacheProcessor<EC> & Tile<EC>::GetCacheProcessor(Dir toCache) const
  {
    MFM_API_ASSERT_ARG(Dirs::IsLegalDir(toCache));
    return m_cacheProcessors[toCache];
  }

  template <class EC>
  Random& Tile<EC>::GetRandom()
  {
    return m_random;
  }

  template <class EC>
  s32 Tile<EC>::CountData::GetAtomCount(u32 type)
  {
    s32 idx = m_tile.m_elementTable.GetIndex(type);
    if (idx < 0)
      return -1;

    RecountIfNeeded();

    return m_atomCount[idx];
  }

  template <class EC>
  void Tile<EC>::CountData::RecountAtoms()
  {
    for(u32 i = 0; i < ELEMENT_TABLE_SIZE; i++) m_atomCount[i] = 0;

    m_illegalAtomCount = 0;

    for(const_iterator_type i = m_tile.begin(); i != m_tile.end(); ++i) {
      if (m_tile.IsInCache(i.At())) continue;

      u32 atype = i->GetAtom().GetType();
      s32 idx = m_tile.m_elementTable.GetIndex(atype);

      if (idx < 0) ++m_illegalAtomCount;
      else ++m_atomCount[idx];
    }
  }

  template <class EC>
  u32 Tile<EC>::GetUncachedWriteAge32(const SPoint site) const
  {
    const u32 A_BILLION = 1000*1000*1000;
    u64 age = GetUncachedWriteAge(site);
    if (age > A_BILLION) return A_BILLION;
    return (u32) age;
  }

  template <class EC>
  u64 Tile<EC>::GetUncachedWriteAge(const SPoint at) const
  {
    MFM_API_ASSERT_ARG(IsInUncachedTile(at));

    return
      GetEventsExecuted() -
      GetUncachedSite(at).GetLastChangedEventNumber();
}

  template <class EC>
  u64 Tile<EC>::GetUncachedEventAge(const SPoint at) const
  {
    MFM_API_ASSERT_ARG(IsInUncachedTile(at));

    return
      (u64) ((s64) GetEventsExecuted() -
             GetUncachedSite(at).GetLastEventEventNumber());
  }


  template <class EC>
  u32 Tile<EC>::GetUncachedEventAge32(const SPoint site) const
  {
    const u32 A_BILLION = 1000*1000*1000;
    u64 age = GetUncachedEventAge(site);
    if (age > A_BILLION) return A_BILLION;
    return (u32) age;
  }

  template <class EC>
  u64 Tile<EC>::GetUncachedSiteEvents(const SPoint at) const
  {
    MFM_API_ASSERT_ARG(IsInUncachedTile(at));
    return GetUncachedSite(at).GetEventCount();
  }

#if 0
  template <class CC>
  void Tile<CC>::InternalPutAtom(const T & atom, s32 x, s32 y)
  {
    if (((u32) x) >= TILE_WIDTH || ((u32) y) >= TILE_WIDTH)
    {
      FAIL(ARRAY_INDEX_OUT_OF_BOUNDS);
    }
    m_atoms[x][y] = atom;
  }
#endif

  template <class EC>
  template <u32 REACH>
  Dir Tile<EC>::RegionAt(const SPoint& sp) const
  {
    UPoint pt = MakeUnsigned(sp);

    if(pt.GetX() < REACH) {

      if(pt.GetY() < REACH) return Dirs::NORTHWEST;
      if(pt.GetY() >= TILE_SIDE - REACH) return Dirs::SOUTHWEST;
      return Dirs::WEST;

    } else if(pt.GetX() >= TILE_SIDE - REACH) {

      if(pt.GetY() < REACH) return Dirs::NORTHEAST;
      if(pt.GetY() >= TILE_SIDE - REACH) return Dirs::SOUTHEAST;
      return Dirs::EAST;

    }

    // X in neither east nor west reach

    if(pt.GetY() < REACH) return Dirs::NORTH;
    if(pt.GetY() >= TILE_SIDE - REACH) return Dirs::SOUTH;
    return (Dir)-1;
  }

  template <class EC>
  Dir Tile<EC>::CacheAt(const SPoint& pt) const
  {
    return RegionAt<EVENT_WINDOW_RADIUS>(pt);
  }

  template <class EC>
  Dir Tile<EC>::SharedAt(const SPoint& pt) const
  {
    return RegionAt<EVENT_WINDOW_RADIUS * 2>(pt);
  }

  template <class EC>
  Dir Tile<EC>::VisibleAt(const SPoint& pt) const
  {
    return RegionAt<EVENT_WINDOW_RADIUS * 3>(pt);
  }

  template <class EC>
  bool Tile<EC>::ApplyCacheUpdate(bool isDifferent, const T& atom, const SPoint& site)
  {
    MFM_API_ASSERT_ARG(!IsInHidden(site));  // That would make no sense

    bool consistent;
    const T& oldAtom = *GetAtom(site);
    if (atom != oldAtom)
    {
      PlaceAtom(atom, site);
      consistent = isDifferent;
    }
    else
    {
      consistent = !isDifferent;
    }

    return consistent;
  }

  template <class EC>
  void Tile<EC>::PlaceAtom(const T& atom, const SPoint& pt)
  {
    if (!IsLiveSite(pt))
    {
      if (atom.GetType() != Element_Empty<EC>::THE_INSTANCE.GetType())
      {
        LOG.Debug("Not placing type %04x at (%2d,%2d) of %s",
                  atom.GetType(), pt.GetX(), pt.GetY(), this->GetLabel());
      }
      return;
    }

    Site<AC> & site = GetSite(pt);
    T newAtom = atom;
    unwind_protect(
    {
      site.GetAtom().SetEmpty();
      LOG.Warning("Failure during PlaceAtom, erased (%2d,%2d) of %s",
                  pt.GetX(), pt.GetY(), this->GetLabel());
    },
    {
      if(m_backgroundRadiation &&
         m_random.OneIn(BACKGROUND_RADIATION_SITE_ODDS))
      {
        // Write fault!
        newAtom.XRay(m_random, BACKGROUND_RADIATION_BIT_ODDS);
      }

      const T& oldAtom = site.GetAtom();
      bool owned = IsOwnedSite(pt);

      if (oldAtom != newAtom) {
        NeedAtomRecount();
        if (owned)
          site.SetLastChangedEventNumber(GetEventsExecuted());

        site.PutAtom(newAtom);
      }
    });
  }

  template <class EC>
  bool Tile<EC>::IsConnected(Dir dir) const
  {
    const CacheProcessor<EC> & cxn = GetCacheProcessor(dir);
    return cxn.IsConnected();
  }

  template <class EC>
  bool Tile<EC>::IsReachableViaCacheProtocol(const SPoint & location) const
  {
    if (!IsInShared(location))
    {
      return false;
    }

    Dir dir = SharedAt(location);
    if (IsConnected(dir))
    {
      return true;
    }

    if (Dirs::IsCorner(dir))
    {
      if (IsConnected(Dirs::CCWDir(dir)))
      {
        return true;
      }
      if (IsConnected(Dirs::CWDir(dir)))
      {
        return true;
      }
    }
    return false;
  }

  template <class EC>
  bool Tile<EC>::IsCacheSitePossibleEventCenter(const SPoint & location) const
  {
    MFM_API_ASSERT_ARG(IsInCache(location));
    return IsConnected(CacheAt(location));
  }

  template <class EC>
  bool Tile<EC>::IsLiveSite(const SPoint & location) const
  {
    if (!IsInTile(location))
    {
      return false;
    }
    if (IsOwnedSite(location))
    {
      return true;
    }

    // In-tile + not-owned => in-cache

    return IsCacheSitePossibleEventCenter(location);
  }

  template <class EC>
  bool Tile<EC>::IsInUncachedTile(const SPoint& pt) const
  {
    return ((u32) pt.GetX()) < OWNED_SIDE && ((u32) pt.GetY() < OWNED_SIDE);
  }

  template <class EC>
  bool Tile<EC>::HasAnyConnections(Dir regionDir) const
  {
    switch(regionDir)
    {
    case Dirs::NORTH:
    case Dirs::EAST:
    case Dirs::SOUTH:
    case Dirs::WEST:
      return IsConnected(regionDir);

    case Dirs::NORTHWEST:
    case Dirs::NORTHEAST:
    case Dirs::SOUTHEAST:
    case Dirs::SOUTHWEST:
      if (IsConnected(regionDir)) return true;
      if (IsConnected(Dirs::CCWDir(regionDir))) return true;
      if (IsConnected(Dirs::CWDir(regionDir))) return true;
      return false;

    default:
      FAIL(ILLEGAL_ARGUMENT);
    }
  }

  template <class EC>
  TileRegion Tile<EC>::RegionFromIndex(const u32 index)
  {
    enum { R = EVENT_WINDOW_RADIUS };

    if(index > TILE_SIDE)
    {
      FAIL(ARRAY_INDEX_OUT_OF_BOUNDS); /* Index out of Tile bounds */
    }

    const u32 hiddenWidth = TILE_SIDE - R * 6;

    if(index < R * REGION_HIDDEN)
    {
      return (TileRegion)(index / R);
    }
    else if(index >= R * REGION_HIDDEN + hiddenWidth)
    {
      return (TileRegion)((index - (R * REGION_HIDDEN) - hiddenWidth) / R);
    }
    else
    {
      return REGION_HIDDEN;
    }
  }

  template <class EC>
  TileRegion Tile<EC>::RegionIn(const SPoint& pt)
  {
    return MIN(RegionFromIndex((u32)pt.GetX()),
               RegionFromIndex((u32)pt.GetY()));
  }

  template <class EC>
  bool Tile<EC>::Advance()
  {
    if (!ConsiderStateChange())
    {
      return false;
    }

    bool didWork = false;
    State curState = GetCurrentState();
    if (!m_enabled)
    {
      curState = PASSIVE;
    }

    switch (curState)
    {
    case OFF:
      break;
    case ACTIVE:
      didWork |= AdvanceComputation();
      MFM_LOG_DBG6(("Tile %s: AdvanceComputation->%d",
                    this->GetLabel(),
                    didWork));
      // FALL THROUGH
    case PASSIVE:
      didWork |= AdvanceCommunication();
      break;
    default:
      FAIL(ILLEGAL_STATE);
    }
    return didWork;
  }

  template <class EC>
  bool Tile<EC>::AllCacheProcessorsIdle()
  {
    for (u32 i = 0; i < Dirs::DIR_COUNT; ++i)  // Can doing this in order create bias??
    {
      if (!m_cacheProcessors[i].IsIdle())
      {
        return false;
      }
    }
    MFM_LOG_DBG6(("Tile %s All CPs idle",
                  this->GetLabel()));
    return true;
  }

  template <class EC>
  void Tile<EC>::SetBackgroundRadiation(bool on)
  {
    m_backgroundRadiation = on;
  }

  template <class EC>
  void Tile<EC>::SetRequestedState(State state)
  {
    Mutex::ScopeLock lock(m_stateAccess);
    LOG.Debug1("Requesting state %s for Tile %s (current: %s)",
              GetStateName(state),
              this->GetLabel(),
              GetStateName(m_state));
    m_requestedState = state;
  }

  template <class EC>
  bool Tile<EC>::ConsiderStateChange()
  {
    Mutex::ScopeLock lock(m_stateAccess);

    if (m_state == m_requestedState)
    {
      return true;
    }

    if (!m_enabled)
    {
      return false;
    }

    switch (m_requestedState)
    {
    case ACTIVE:
      if (m_state == OFF || m_state == PASSIVE)
      {
        m_state = m_requestedState;
        return true;
      }

      if (AllCacheProcessorsIdle())
      {
        m_state = m_requestedState;
        return true;
      }
      return false;

    case PASSIVE:
      m_state = m_requestedState;
      return true;

    case OFF:
      FAIL(INCOMPLETE_CODE);
    }
    return true;
  }

  template <class EC>
  bool Tile<EC>::AdvanceComputation()
  {

    //NON_ACTIVE,
    if (!IsActive())
    {
      return false;
    }

    //INITIATE_EVENT,
    SPoint pt = GetRandomOwnedCoord();
    return m_window.TryEventAt(pt);
  }

  template <class EC>
  bool Tile<EC>::AdvanceCommunication()
  {
    bool didWork = false;
    for (m_dirIterator.ShuffleOrReset(m_random); m_dirIterator.HasNext(); )
    {
      u32 i = m_dirIterator.Next();
      CacheProcessor<EC> & cp = m_cacheProcessors[i];
      didWork |= cp.Advance();
    }
    return didWork;
  }

  template <class EC>
  void Tile<EC>::ReportTileStatus(Logger::Level level)
  {
    LOG.Log(level," ===TILE %s STATUS REPORT: cur %s req %s===",
            m_label.GetZString(),
            GetStateName(GetCurrentState()),
            GetStateName(m_requestedState));

    LOG.Log(level,"  ==Tile %s Global==", m_label.GetZString());
    LOG.Log(level,"   Address: %p", (void*) this);
    LOG.Log(level,"   Error stack top: %p", (void*) m_errorEnvironmentStackTop);

    LOG.Log(level,"  ==Tile %s Atomic==", m_label.GetZString());
    LOG.Log(level,"   Recount needed: %s", m_cdata.m_needRecount?"true":"false");

    LOG.Log(level,"  ==Tile %s Events==", m_label.GetZString());
    LOG.Log(level,"   Events: %dM (total)", (u32) (GetEventsExecuted() / 1000000));

    for (u32 d = Dirs::NORTH; d <= Dirs::NORTHWEST; ++d)
    {
      CacheProcessor<EC> & cp = GetCacheProcessor(d);
      cp.ReportCacheProcessorStatus(level);
    }
  }
} /* namespace MFM */
