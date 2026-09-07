//--------------------------------------------------------------------
/**
 * @file Leds.cc
 * @brief Implements rewinder status indication using the ten MMU LEDs.
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
#include "hardware/Leds.hh"
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
 * @brief Construct a new Leds object.
 *
 * Initializes the last known LED state to all bits set (0xFFFF).
 */
//--------------------------------------------------------------------
Leds::Leds()
  : mLastBits(0xFFFFU)
{
}

//--------------------------------------------------------------------
/**
 * @brief Get the bitmask for the green LED corresponding to the given slot.
 *
 * @param aSlot The slot index (0-4).
 * @return The bitmask for the green LED.
 */
//--------------------------------------------------------------------
uint16_t Leds::greenBit(uint8_t aSlot)
{
  const uint16_t tBit = static_cast<uint16_t>(1U << (9U - static_cast<uint8_t>(aSlot * 2U)));
  return tBit;
}

//--------------------------------------------------------------------
/**
 * @brief Get the bitmask for the red LED corresponding to the given slot.
 *
 * @param aSlot The slot index (0-4).
 * @return The bitmask for the red LED.
 */
//--------------------------------------------------------------------
uint16_t Leds::redBit(uint8_t aSlot)
{
  const uint16_t tBit = static_cast<uint16_t>(1U << (8U - static_cast<uint8_t>(aSlot * 2U)));
  return tBit;
}

//--------------------------------------------------------------------
/**
 * @brief Write the given logical LED bitmask to the hardware.
 *
 * Avoids unnecessary shift-register traffic by only updating when the
 * logical bits have changed since the last write.
 *
 * @param aLogicalBits The logical LED bitmask to write.
 */
//--------------------------------------------------------------------
void Leds::write(uint16_t aLogicalBits)
{
  // Avoid unnecessary shift-register traffic.  This is especially useful in
  // the main loop, where update() can be called far faster than LEDs can change.
  if(aLogicalBits != mLastBits)
  {
    hal::shr16::shr16.SetLED(aLogicalBits);
    mLastBits = aLogicalBits;
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the LED states based on the current rewinder state and speed level.
 *
 * @param aNowMs The current time in milliseconds.
 * @param aState The current state of the rewinder.
 * @param aSpeedLevel The current speed level of the rewinder.
 */
//--------------------------------------------------------------------
void Leds::update(uint16_t aNowMs,
                  RewinderState aState,
                  uint8_t aSpeedLevel)
{
  uint16_t tBits = 0U;

  // Blink timing is derived from the wrapping millisecond counter; division
  // converts it into a half-period phase used by warning/error patterns.
  const bool tBlink = (((aNowMs / (config::LED_BLINK_PERIOD_MS / 2U)) & 1U) != 0U);

  // Determine the LED pattern based on the current rewinder state.
  switch(aState)
  {
    // Boot, Homing, and HomeBackoff states: blink a single green LED in a moving pattern.
    case RewinderState::Boot:
    case RewinderState::Homing:
    case RewinderState::HomeBackoff:
    {
      if(tBlink)
      {
        const uint8_t tSlot = static_cast<uint8_t>((aNowMs / 150U) % 5U);
        tBits = greenBit(tSlot);
      }
      break;
    }

    // Ready state: a single stationary green LED.
    case RewinderState::Ready:
    {
      tBits = greenBit(0U);
      break;
    }

    // AdjustOuterLimit state: a moving green indicator for spool-width setup.
    case RewinderState::AdjustOuterLimit:
    {
      // A moving green indicator distinguishes spool-width setup from the
      // stationary Ready indication.  The pattern is intentionally simple so
      // it does not require the LED layer to know the actual shuttle position.
      const uint8_t tSlot = static_cast<uint8_t>((aNowMs / 250U) % 5U);
      tBits = greenBit(tSlot);
      break;
    }

    // Winding state: progressively light green LEDs based on the speed level.
    case RewinderState::Winding:
    {
      for(uint8_t tIndex = 0U; (tIndex <= aSpeedLevel) && (tIndex < 5U); ++tIndex)
      {
        tBits |= greenBit(tIndex);
      }
      break;
    }

    // Paused state: blink the current green speed bar so the operator can
    // distinguish a resumable pause from the stationary Ready indication.
    case RewinderState::Paused:
    {
      if(tBlink)
      {
        for(uint8_t tIndex = 0U; (tIndex <= aSpeedLevel) && (tIndex < 5U); ++tIndex)
        {
          tBits |= greenBit(tIndex);
        }
      }
      break;
    }

    // OutOfFilament and Error states: blink all red LEDs.
    case RewinderState::OutOfFilament:
    case RewinderState::Error:
    {
      if(tBlink)
      {
        tBits = static_cast<uint16_t>(
          redBit(0U) |
          redBit(1U) |
          redBit(2U) |
          redBit(3U) |
          redBit(4U));
      }
      break;
    }
  }

  // Write the determined LED pattern to the hardware.
  write(tBits);
}

} // namespace hardware
} // namespace spooler
//--------------------------------------------------------------------
