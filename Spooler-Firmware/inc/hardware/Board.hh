//--------------------------------------------------------------------
/**
 * @file Board.hh
 * @brief Declares the hardware abstraction for the reused Prusa MMU board.
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
 * Board owns the three TMC2130 driver objects and maps the original MMU motor
 * channels into rewinder terminology:
 *
 *   Original pulley motor   -> shuttle / traverse motor
 *   Original selector motor -> take-up spool motor
 *   Original idler motor    -> supply-spool brake motor
 *
 * Board also owns the button, FINDA, LED, and motion-scheduler abstractions.
 * Application code therefore deals with the rewinder mechanism and does not
 * need to know the original Prusa MMU pin or motor names.
 *
 * A 1 ms Timer0 interrupt maintains a wrapping 16-bit millisecond clock.  All
 * elapsed-time tests use unsigned subtraction so they remain correct across the
 * natural 16-bit rollover as long as individual intervals are less than 65536
 * milliseconds.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_BOARD_HH
#define PRUSA_SPOOLER_BOARD_HH

#include <stdint.h>

#include "hardware/Tmc2130.hh"
#include "hardware/Buttons.hh"
#include "hardware/Finda.hh"
#include "hardware/Leds.hh"
#include "motion/MotionController.hh"
#include "motion/StepperAxis.hh"

namespace spooler
{
namespace hardware
{

class Board
{
public:
  Board();

//--------------------------------------------------------------------
  /**
   * @brief Initializes MMU board peripherals and all three TMC2130 drivers.
   * @return true when every motor driver initializes successfully.
   */
//--------------------------------------------------------------------
  bool init();

//--------------------------------------------------------------------
  /** @brief Samples and debounces user/sensor inputs. */
//--------------------------------------------------------------------
  void updateInputs();

//--------------------------------------------------------------------
  /** @brief Returns the 16-bit millisecond system clock. */
//--------------------------------------------------------------------
  uint16_t millis() const;

//--------------------------------------------------------------------
  /** @brief Returns the shared-button input interface. */
//--------------------------------------------------------------------
  Buttons &buttons();

//--------------------------------------------------------------------
  /** @brief Returns the FINDA filament sensor interface. */
//--------------------------------------------------------------------
  Finda &finda();

//--------------------------------------------------------------------
  /** @brief Returns the status LED interface. */
//--------------------------------------------------------------------
  Leds &leds();

//--------------------------------------------------------------------
  /** @brief Returns the shuttle/traverse motor axis. */
//--------------------------------------------------------------------
  motion::StepperAxis &shuttle();

//--------------------------------------------------------------------
  /** @brief Returns the take-up spool motor axis. */
//--------------------------------------------------------------------
  motion::StepperAxis &takeup();

//--------------------------------------------------------------------
  /** @brief Returns the supply-spool brake motor axis. */
//--------------------------------------------------------------------
  motion::StepperAxis &brake();

//--------------------------------------------------------------------
  /** @brief Returns the fixed-frequency motion scheduler. */
//--------------------------------------------------------------------
  motion::MotionController &motion();

private:
//--------------------------------------------------------------------
  /** @brief Configures Timer0 to provide the 1 ms application time base. */
//--------------------------------------------------------------------
  void initMillisecondTimer();

  Tmc2130 mShuttleDriver; ///< Original pulley TMC2130.
  Tmc2130 mTakeupDriver;  ///< Original selector TMC2130.
  Tmc2130 mBrakeDriver;   ///< Original idler TMC2130.

  Tmc2130Params mShuttleParams; ///< Shuttle pin/SPI parameters.
  Tmc2130Params mTakeupParams;  ///< Take-up pin/SPI parameters.
  Tmc2130Params mBrakeParams;   ///< Brake pin/SPI parameters.

  motion::StepperAxis mShuttle; ///< Filament traverse/shuttle axis.
  motion::StepperAxis mTakeup;  ///< Receiving spool rotation axis.
  motion::StepperAxis mBrake;   ///< Supply-spool braking axis.
  motion::MotionController mMotion; ///< 20 kHz step scheduler.

  Buttons mButtons; ///< Three front-panel buttons read through ADC5.
  Finda mFinda;     ///< Filament-present detector.
  Leds mLeds;       ///< Five red/green status LED pairs.
};

//--------------------------------------------------------------------
/** @brief Millisecond counter incremented by the Timer0 compare ISR. */
//--------------------------------------------------------------------
extern volatile uint16_t gMilliseconds;

} // namespace hardware
} // namespace spooler

#endif // PRUSA_SPOOLER_BOARD_HH
