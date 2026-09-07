//--------------------------------------------------------------------
/**
 * @file Tmc2130.hh
 * @brief Declares the rewinder-local TMC2130 motor driver wrapper.
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
 * @date 2026-09-04
 *
 * @details
 * This wrapper intentionally contains only the TMC2130 functionality required
 * by the filament rewinder.  The original Prusa hal/tmc2130.h header also pulls
 * in the complete MMU configuration and physical-unit framework.  The rewinder
 * does not need those higher-level dependencies, so keeping a small local
 * wrapper makes the hardware layer independent of config/axis.h and unit.h.
 *
 * The implementation follows the register programming used by the Prusa MMU
 * firmware for CHOPCONF, current control, StallGuard, PWM configuration, and
 * SPI access.  Low-level GPIO, SPI, and shift-register services continue to be
 * supplied by the proven Prusa HAL.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_TMC2130_HH
#define PRUSA_SPOOLER_TMC2130_HH

#include <stdint.h>

#include "hardware/MmuHal.hh"

namespace spooler
{
namespace hardware
{

//--------------------------------------------------------------------
/** @brief TMC2130 microstep-resolution field values. */
//--------------------------------------------------------------------
enum class MicrostepResolution : uint8_t
{
  MRes256 = 0U,
  MRes128 = 1U,
  MRes64  = 2U,
  MRes32  = 3U,
  MRes16  = 4U,
  MRes8   = 5U,
  MRes4   = 6U,
  MRes2   = 7U,
  MRes1   = 8U
};

//--------------------------------------------------------------------
/** @brief Hardware mapping for one TMC2130 motor channel. */
//--------------------------------------------------------------------
struct Tmc2130Params
{
  volatile hal::spi::SPI_TypeDef *mSpi;
  uint8_t mShiftRegisterIndex;
  bool mDirectionInverted;
  hal::gpio::GPIO_pin mChipSelectPin;
  hal::gpio::GPIO_pin mStepPin;
  hal::gpio::GPIO_pin mStallGuardPin;
  MicrostepResolution mMicrostepResolution;
  int8_t mStallGuardThreshold;
};

//--------------------------------------------------------------------
/** @brief Run and hold current values accepted by the TMC2130 registers. */
//--------------------------------------------------------------------
struct Tmc2130Currents
{
  explicit Tmc2130Currents(uint8_t aRunCurrent,
                           uint8_t aHoldCurrent);

  bool mVSense;
  uint8_t mRunCurrent;
  uint8_t mHoldCurrent;
};

//--------------------------------------------------------------------
/**
 * @brief Minimal TMC2130 driver used by the rewinder.
 */
//--------------------------------------------------------------------
class Tmc2130
{
public:
  Tmc2130();

//--------------------------------------------------------------------
  /** @brief Initializes the driver registers and GPIO pins. */
//--------------------------------------------------------------------
  bool init(const Tmc2130Params &aParams,
            const Tmc2130Currents &aCurrents);

//--------------------------------------------------------------------
  /** @brief Enables or disables motor current through the MMU shift register. */
//--------------------------------------------------------------------
  void setEnabled(const Tmc2130Params &aParams, bool aEnabled);

//--------------------------------------------------------------------
  /** @brief Updates run and hold current registers. */
//--------------------------------------------------------------------
  void setCurrents(const Tmc2130Params &aParams,
                   const Tmc2130Currents &aCurrents);

//--------------------------------------------------------------------
  /** @brief Sets the logical motor direction. */
//--------------------------------------------------------------------
  static void setDirection(const Tmc2130Params &aParams, bool aDirection);

//--------------------------------------------------------------------
  /** @brief Toggles the STEP pin. CHOPCONF is configured for double-edge step. */
//--------------------------------------------------------------------
  static void step(const Tmc2130Params &aParams);

//--------------------------------------------------------------------
  /** @brief Returns true while the active-low StallGuard/DIAG signal is active. */
//--------------------------------------------------------------------
  static bool stallActive(const Tmc2130Params &aParams);

private:
  enum class Register : uint8_t
  {
    Gconf      = 0x00U,
    Gstat      = 0x01U,
    Ioin       = 0x04U,
    IHoldIRun  = 0x10U,
    TPowerDown = 0x11U,
    TPwmThrs   = 0x13U,
    TCoolThrs  = 0x14U,
    ChopConf   = 0x6CU,
    CoolConf   = 0x6DU,
    PwmConf    = 0x70U
  };

  uint32_t readRegister(const Tmc2130Params &aParams, Register aRegister);
  void writeRegister(const Tmc2130Params &aParams,
                     Register aRegister,
                     uint32_t aValue);
  void transfer(const Tmc2130Params &aParams, uint8_t (&aData)[5]);
  void setStallGuardThreshold(const Tmc2130Params &aParams);

  bool mEnabled;
};

} // namespace hardware
} // namespace spooler

#endif // PRUSA_SPOOLER_TMC2130_HH
