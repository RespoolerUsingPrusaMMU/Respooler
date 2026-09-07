//--------------------------------------------------------------------
/**
 * @file Leds.hh
 * @brief Declares rewinder status indication using the ten MMU LEDs.
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
 * The five red/green LED pairs on the MMU board are driven through the same
 * 16-bit shift register used for motor direction/enable signals.  The Prusa
 * shift-register HAL accepts a logical LED mask; this class generates that mask
 * from the rewinder state.
 *
 * The initial display convention is:
 *   - Homing: moving/blinking green indication.
 *   - Ready: first green LED on.
 *   - Adjust outer limit: moving green indicator.
 *   - Winding: green bar graph representing speed level.
 *   - Paused: blinking green speed bar.
 *   - Out of filament / Error: all red LEDs blink.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_LEDS_HH
#define PRUSA_SPOOLER_LEDS_HH

#include <stdint.h>

#include "app/RewinderState.hh"

namespace spooler
{
namespace hardware
{

class Leds
{
public:
  Leds();

//--------------------------------------------------------------------
  /**
   * @brief Updates the LED display for the current application state.
   * @param aNowMs Current system time in milliseconds.
   * @param aState Current high-level rewinder state.
   * @param aSpeedLevel Current zero-based winding speed level.
   */
//--------------------------------------------------------------------
  void update(uint16_t aNowMs,
              RewinderState aState,
              uint8_t aSpeedLevel);

private:
//--------------------------------------------------------------------
  /** @brief Writes a logical LED bit mask only when the value has changed. */
//--------------------------------------------------------------------
  void write(uint16_t aLogicalBits);

//--------------------------------------------------------------------
  /** @brief Returns the logical shift-register bit for a green slot LED. */
//--------------------------------------------------------------------
  static uint16_t greenBit(uint8_t aSlot);

//--------------------------------------------------------------------
  /** @brief Returns the logical shift-register bit for a red slot LED. */
//--------------------------------------------------------------------
  static uint16_t redBit(uint8_t aSlot);

  uint16_t mLastBits; ///< Last value sent to the shift register.
};

} // namespace hardware
} // namespace spooler

#endif // PRUSA_SPOOLER_LEDS_HH
