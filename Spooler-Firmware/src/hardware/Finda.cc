//--------------------------------------------------------------------
/**
 * @file Finda.cc
 * @brief Implements the FINDA filament-presence input interface.
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
 */
//--------------------------------------------------------------------
// Global includes

// Local includes
#include "hardware/Finda.hh"
#include "config/Defaults.hh"
#include "hardware/MmuHal.hh"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace spooler
{
//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace hardware
{

//--------------------------------------------------------------------
/**
 * @brief Construct a new Finda object.
 *
 * Initializes the raw and stable filament-present states to false and sets the
 * raw timestamp to 0.
 */
//--------------------------------------------------------------------
Finda::Finda()
  : mRaw(false),                   ///< The raw filament-present state.
    mStableFilamentPresent(false), ///< The stable filament-present state.
    mRawSinceMs(0U)                //
{
}

//--------------------------------------------------------------------
/**
 * @brief Get the raw filament-present state from the FINDA sensor.
 *
 * @return True if filament is present, false otherwise.
 */
//--------------------------------------------------------------------
bool Finda::rawFilamentPresent() const
{
  // Convert the physical FINDA logic level to the semantic meaning used by the
  // rewinder.  Keeping the polarity here prevents inverted logic elsewhere.
  const bool tHigh = (hal::gpio::ReadPin(FINDA_PIN) == hal::gpio::Level::high);
  bool tFilamentPresent = tHigh;

  // Apply any configuration-based polarity inversion.
  if(config::FINDA_HIGH_MEANS_NO_FILAMENT)
  {
    tFilamentPresent = !tHigh;
  }

  return tFilamentPresent;
}

//--------------------------------------------------------------------
/**
 * @brief Update the filament-present state based on the raw sensor reading.
 *
 * This method applies a debounce mechanism to ensure that the filament-present
 * state only changes after the raw sensor reading has remained stable for the
 * configured debounce period.
 *
 * @param aNowMs The current time in milliseconds.
 */
//--------------------------------------------------------------------
void Finda::update(uint16_t aNowMs)
{
  const bool tSample = rawFilamentPresent();

  // Require the new FINDA value to remain stable for the configured debounce
  // period before allowing it to stop or enable a winding operation.
  if(tSample != mRaw)
  {
    mRaw = tSample;
    mRawSinceMs = aNowMs;
  }
  // End of raw state change handling.
  else
  {
    const uint16_t tElapsed = static_cast<uint16_t>(aNowMs - mRawSinceMs);

    // Check if the raw state has been stable for the debounce period.
    if(tElapsed >= config::FINDA_DEBOUNCE_MS)
    {
      mStableFilamentPresent = mRaw;
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Get the stable filament-present state.
 *
 * @return True if filament is present, false otherwise.
 */
//--------------------------------------------------------------------
bool Finda::filamentPresent() const
{
  const bool tFilamentPresent = mStableFilamentPresent;
  return tFilamentPresent;
}

} // namespace hardware
} // namespace spooler
//--------------------------------------------------------------------
