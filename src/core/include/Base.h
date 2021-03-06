/*                                              -*- mode:C++ -*-
  Base.h Fixed storage site components
  Copyright (C) 2015 The Regents of the University of New Mexico.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
  USA
*/

/**
  \file Base.h Fixed storage site components
  \author David H. Ackley.
  \date (C) 2015 All rights reserved.
  \lgpl
 */
#ifndef BASE_H
#define BASE_H

#include "Sense.h"

namespace MFM {

  /**
     A Base holds the non-mobile state of a Site.  It is
     a template depending only on an AtomConfig (AC).
  */
  template <class AC>
  class Base
  {
    /**
       Present the AtomConfig in use
     */
    typedef AC ATOM_CONFIG;

    // Extract short names for parameter types
    typedef typename ATOM_CONFIG::ATOM_TYPE T;

    T m_state;
    T m_base;
    SiteSensors m_sensory;
    u64 m_lastEventEventNumber;

  public:

    void SetLastEventEventNumber(u64 eventNumber)
    {
      m_lastEventEventNumber = eventNumber;
    }

    const u64 GetLastEventEventNumber() const
    {
      return m_lastEventEventNumber;
    }

    /**
    u32 GetRGBIn() const { return m_rgbIn; }
    void SetRGBIn(u32 newRGB) { m_rgbIn = newRGB; }

    u32 GetRGBOut() const { return m_rgbOut; }
    void SetRGBOut(u32 newRGB) { m_rgbOut = newRGB; }

    u32 GetSenseIn() const { return m_senseIn; }
    void SetSenseIn(u32 newSense) { m_senseIn = newSense; }

    u32 GetMotorOut() const { return m_motorOut; }
    void SetMotorOut(u32 newMotor) { m_motorOut = newMotor; }
    */

    void PutStateAtom(const T & newState) { m_state = newState; }
    T & GetStateAtom() { return m_state; }
    const T & GetStateAtom() const { return m_state; }

    void PutBaseAtom(const T & newBase) { m_base = newBase; }
    T & GetBaseAtom() { return m_base; }
    const T & GetBaseAtom() const { return m_base; }

    SiteSensors & GetSensory() { return m_sensory; }
    const SiteSensors & GetSensory() const { return m_sensory; }
  };

} /* namespace MFM */

#endif /*BASE_H*/
