#include "Fail.h"
#include "Tile.h"
#include "Random.h"
#include "EventWindow.h"
#include "Base.h"

namespace MFM {

  template <class EC>
  void UlamElement<EC>::Behavior(EventWindow<EC>& window) const
  {
    Tile<EC> & tile = window.GetTile();
    UlamContext<EC> uc;
    uc.SetTile(tile);

    u32 sym = m_info ? m_info->GetSymmetry(uc) : PSYM_DEG000L;
    window.SetSymmetry((PointSymmetry) sym);

    T & me = window.GetCenterAtomSym();
    Uf_6behave(uc, me);
  }

  template <class EC>
  s32 UlamElement<EC>::PositionOfDataMember(UlamContext<EC>& uc, u32 type, const char * dataMemberTypeName)
  {
    Tile<EC> & tile = uc.GetTile();
    ElementTable<EC> & et = tile.GetElementTable();
    const Element<EC> * eltptr = et.Lookup(type);
    if (!eltptr) return -1;
    const UlamElement<EC> * ueltptr = eltptr->AsUlamElement();
    if (!ueltptr) return -2;
    s32 ret = ueltptr->PositionOfDataMemberType(dataMemberTypeName);
    if (ret < 0) return -3;
    return ret;
  }

  template <class EC>
  void UlamElement<EC>::Print(const UlamClassRegistry & ucr, ByteSink & bs, const T & atom, u32 flags) const
  {
    if (!flags) return;

    if (flags & PRINT_SYMBOL) bs.Printf("(%s)", this->GetAtomicSymbol());

    if (flags & PRINT_FULL_NAME) bs.Printf("%s",this->GetName());

    if (flags & PRINT_ATOM_BODY)
    {
      typedef typename EC::ATOM_CONFIG AC;
      T dup = atom; // Get mutable copy (lame AtomSerializer)
      AtomSerializer<AC> as(dup);
      bs.Printf(":%@", &as);
    }

    this->PrintClassMembers<EC>(ucr,bs,atom,flags,0);
  }
}
