//--------------------------------------------------------------------
/**
 * @file Buttons.cc
 * @brief Implements the Prusa MMU three-button analog input interface.
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
#include "hardware/Buttons.hh"
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
 * @brief Construct a new Buttons object.
 *
 * Initializes the raw, stable, and reported button states to None and sets the
 * raw timestamp to 0.
 */
//--------------------------------------------------------------------
Buttons::Buttons()
  : mRaw(Button::None),
    mStable(Button::None),
    mReported(Button::None),
    mRawSinceMs(0U)
{
}

//--------------------------------------------------------------------
/**
 * @brief Decode the ADC value into a button.
 *
 * @param aAdc The ADC value to decode.
 * @return The corresponding button, or None if no button is pressed.
 */
//--------------------------------------------------------------------
Button Buttons::decode(uint16_t aAdc) const
{
  // The resistor network produces non-overlapping ADC windows.  Counts that do
  // not fall inside a window are intentionally interpreted as no button.
  Button tButton = Button::None;

  // Decode the ADC value into a button based on predefined thresholds.
  if(aAdc <= config::RIGHT_BUTTON_MAX)
  {
    tButton = Button::Right;
  }
  // Check for the middle button next.
  else if((aAdc >= config::MIDDLE_BUTTON_MIN) &&
          (aAdc <= config::MIDDLE_BUTTON_MAX))
  {
    tButton = Button::Middle;
  }
  // Check for the left button last.
  else if((aAdc >= config::LEFT_BUTTON_MIN) &&
          (aAdc <= config::LEFT_BUTTON_MAX))
  {
    tButton = Button::Left;
  }

  return tButton;
}

//--------------------------------------------------------------------
/**
 * @brief Update the button states based on the current ADC reading.
 *
 * @param aNowMs The current time in milliseconds.
 */
//--------------------------------------------------------------------
void Buttons::update(uint16_t aNowMs)
{
  const Button tSample = decode(hal::adc::ReadADC(config::BUTTONS_ADC_CHANNEL));

  // A raw change restarts the debounce interval.  The new value is not exposed
  // to the application until it remains unchanged for BUTTON_DEBOUNCE_MS.
  if(tSample != mRaw)
  {
    mRaw = tSample;
    mRawSinceMs = aNowMs;
  }
  // No raw change; continue debounce logic.
  else
  {
    const uint16_t tElapsed = static_cast<uint16_t>(aNowMs - mRawSinceMs);

    // Check if the debounce interval has elapsed.
    if(tElapsed >= config::BUTTON_DEBOUNCE_MS)
    {
      mStable = mRaw;
    }

    //
    if(mStable == Button::None)
    {
      mReported = Button::None;
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Check if a specific button is currently pressed.
 *
 * @param aButton The button to check.
 * @return True if the button is pressed, false otherwise.
 */
//--------------------------------------------------------------------
bool Buttons::pressed(Button aButton) const
{
  const bool tPressed = (mStable == aButton);
  return tPressed;
}

//--------------------------------------------------------------------
/**
 * @brief Check if a specific button was just pressed.
 *
 * @param aButton The button to check.
 * @return True if the button was just pressed, false otherwise.
 */
//--------------------------------------------------------------------
bool Buttons::justPressed(Button aButton)
{
  // mReported suppresses repeated events while a button remains held.  Once
  // the stable state returns to None, the next press can generate a new event.
  bool tPressed = false;

  // Check if the button was just pressed.
  if((mStable == aButton) && (mReported != aButton))
  {
    mReported = aButton;
    tPressed = true;
  }

  return tPressed;
}

} // namespace hardware
} // namespace spooler
//--------------------------------------------------------------------

