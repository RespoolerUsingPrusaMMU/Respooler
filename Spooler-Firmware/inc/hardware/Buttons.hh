//--------------------------------------------------------------------
/**
 * @file Buttons.hh
 * @brief Declares the Prusa MMU three-button analog input interface.
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
 * The original MMU controller does not dedicate one digital pin to each
 * button.  The three switches feed a resistor network connected to a single
 * ADC channel.  Each button therefore produces a different ADC range.
 *
 * This class converts the ADC value into a logical button, debounces that
 * result, and provides both level-oriented and edge-oriented queries.  The
 * application never needs to know the resistor-network ADC thresholds.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_BUTTONS_HH
#define PRUSA_SPOOLER_BUTTONS_HH

#include <stdint.h>

namespace spooler
{
namespace hardware
{

//--------------------------------------------------------------------
/** @brief Logical buttons available on the reused MMU controller. */
//--------------------------------------------------------------------
enum class Button : uint8_t
{
  None,   ///< No button is currently pressed.
  Left,   ///< Left push button.
  Middle, ///< Center push button used for start/stop.
  Right   ///< Right push button.
};

class Buttons
{
public:
  Buttons();

//--------------------------------------------------------------------
  /**
   * @brief Samples the shared ADC input and updates debounce state.
   * @param aNowMs Current system time in milliseconds.
   */
//--------------------------------------------------------------------
  void update(uint16_t aNowMs);

//--------------------------------------------------------------------
  /**
   * @brief Tests whether the requested button is currently held.
   * @param aButton Button to test.
   * @return true when aButton is the stable debounced input.
   */
//--------------------------------------------------------------------
  bool pressed(Button aButton) const;

//--------------------------------------------------------------------
  /**
   * @brief Reports a newly debounced press exactly once.
   * @param aButton Button to test.
   * @return true once when aButton transitions into the stable pressed state.
   */
//--------------------------------------------------------------------
  bool justPressed(Button aButton);

private:
//--------------------------------------------------------------------
  /**
   * @brief Converts one raw ADC count into a logical button value.
   * @param aAdc Raw ADC sample from the button resistor network.
   * @return Decoded button or Button::None when no threshold matches.
   */
//--------------------------------------------------------------------
  Button decode(uint16_t aAdc) const;

  Button mRaw;           ///< Most recent undecounced decoded button value.
  Button mStable;        ///< Button value that has satisfied debounce time.
  Button mReported;      ///< Stable press already returned by justPressed().
  uint16_t mRawSinceMs;  ///< Time at which the current raw value first appeared.
};

} // namespace hardware
} // namespace spooler

#endif // PRUSA_SPOOLER_BUTTONS_HH
