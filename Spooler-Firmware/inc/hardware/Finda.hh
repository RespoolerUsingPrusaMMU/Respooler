//--------------------------------------------------------------------
/**
 * @file Finda.hh
 * @brief Declares the FINDA filament-presence input interface.
 *
 * Copyright (C) 2026 Andre Pruitt
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *
 * @author Andre Pruitt (andre@pruittfamily.com)
 * @date 2026-09-03
 *
 * @details
 * The rewinder reuses the MMU FINDA sensor, but the mechanical arrangement is
 * intentionally inverted from the original MMU use.  The steel ball is close
 * to FINDA when no filament is present and is pushed away when filament is
 * present.  This class hides that electrical/mechanical polarity and exposes
 * only the semantic filamentPresent() result to the application.
 *
 * FINDA is debounced because vibration from the spooler and filament motion can
 * otherwise create short false transitions that would stop a valid winding.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_FINDA_HH
#define PRUSA_SPOOLER_FINDA_HH

#include <stdint.h>

namespace spooler
{
namespace hardware
{

class Finda
{
public:
  Finda();

//--------------------------------------------------------------------
  /**
   * @brief Samples FINDA and updates its debounced state.
   * @param aNowMs Current system time in milliseconds.
   */
//--------------------------------------------------------------------
  void update(uint16_t aNowMs);

//--------------------------------------------------------------------
  /**
   * @brief Returns the debounced filament state.
   * @return true when filament is present in the sensor path.
   */
//--------------------------------------------------------------------
  bool filamentPresent() const;

private:
//--------------------------------------------------------------------
  /**
   * @brief Reads the physical FINDA input and applies configured polarity.
   * @return Semantic, non-debounced filament-present state.
   */
//--------------------------------------------------------------------
  bool rawFilamentPresent() const;

  bool mRaw;                    ///< Latest non-debounced semantic input value.
  bool mStableFilamentPresent;  ///< Debounced state exposed to the application.
  uint16_t mRawSinceMs;         ///< Time at which mRaw last changed.
};

} // namespace hardware
} // namespace spooler

#endif // PRUSA_SPOOLER_FINDA_HH
