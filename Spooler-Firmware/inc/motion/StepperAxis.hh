//--------------------------------------------------------------------
/**
 * @file StepperAxis.hh
 * @brief Declares a generic motion axis backed by a Prusa TMC2130 driver.
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
 * StepperAxis provides the common motion behavior used by the shuttle,
 * take-up spool, and brake motors.  The class deliberately hides the original
 * MMU pulley/selector/idler naming from the application layer.
 *
 * Step generation uses a 32-bit direct-digital-synthesis style phase
 * accumulator.  On every scheduler interrupt, mPhaseIncrement is added to
 * mPhaseAccumulator.  A 32-bit wrap indicates that one motor step is due.
 * This allows a fixed-rate timer ISR to produce a wide range of average step
 * frequencies without changing the timer period.
 *
 * Members shared between normal application code and the timer ISR are
 * volatile.  Functions that read or modify multi-byte ISR-visible values use
 * ATOMIC_BLOCK in the implementation file.  <util/atomic.h> is intentionally
 * not included here because it is an AVR implementation detail and is therefore
 * kept out of the public interface.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_STEPPER_AXIS_HH
#define PRUSA_SPOOLER_STEPPER_AXIS_HH

#include <stdint.h>

#include "hardware/Tmc2130.hh"

namespace spooler
{
namespace motion
{

class StepperAxis
{
public:
//--------------------------------------------------------------------
  /**
   * @brief Constructs an axis around an existing rewinder TMC2130 object.
   * @param aDriver TMC2130 driver instance owned by the board abstraction.
   * @param aParams Physical pin/bus parameters for this motor channel.
   */
//--------------------------------------------------------------------
  StepperAxis(hardware::Tmc2130 &aDriver,
              const hardware::Tmc2130Params &aParams);

//--------------------------------------------------------------------
  /** @brief Enables or disables motor drive current. */
//--------------------------------------------------------------------
  void setEnabled(bool aEnabled);

//--------------------------------------------------------------------
  /** @brief Returns the last commanded enable state. */
//--------------------------------------------------------------------
  bool enabled() const;

//--------------------------------------------------------------------
  /**
   * @brief Sets the TMC2130 run and hold current levels.
   * @param aRunCurrent Driver run-current setting.
   * @param aHoldCurrent Driver hold-current setting.
   */
//--------------------------------------------------------------------
  void setCurrent(uint8_t aRunCurrent, uint8_t aHoldCurrent);

//--------------------------------------------------------------------
  /**
   * @brief Sets the logical positive direction for subsequent steps.
   * @param aPositive true for positive logical motion, false for negative.
   */
//--------------------------------------------------------------------
  void setDirection(bool aPositive);

//--------------------------------------------------------------------
  /** @brief Returns the last commanded logical direction. */
//--------------------------------------------------------------------
  bool directionPositive() const;

//--------------------------------------------------------------------
  /**
   * @brief Starts continuous motion at the requested absolute step rate.
   * @param aStepsPerSecond Requested step frequency in steps/second.
   */
//--------------------------------------------------------------------
  void setRate(float aStepsPerSecond);

//--------------------------------------------------------------------
  /** @brief Stops step generation without changing the logical position. */
//--------------------------------------------------------------------
  void stop();

//--------------------------------------------------------------------
  /**
   * @brief Moves to an absolute logical position.
   * @param aTargetPosition Destination in motor steps.
   * @param aStepsPerSecond Positive step frequency used for the move.
   */
//--------------------------------------------------------------------
  void moveTo(int32_t aTargetPosition, float aStepsPerSecond);

//--------------------------------------------------------------------
  /**
   * @brief Moves by a relative number of steps.
   * @param aDeltaSteps Signed distance from the current position.
   * @param aStepsPerSecond Positive step frequency used for the move.
   */
//--------------------------------------------------------------------
  void moveBy(int32_t aDeltaSteps, float aStepsPerSecond);

//--------------------------------------------------------------------
  /** @brief Returns true when no bounded move remains active. */
//--------------------------------------------------------------------
  bool moveComplete() const;

//--------------------------------------------------------------------
  /**
   * @brief Assigns a new logical position without physically moving the motor.
   * @details Used after homing to establish the shuttle coordinate system.
   */
//--------------------------------------------------------------------
  void setPosition(int32_t aPosition);

//--------------------------------------------------------------------
  /** @brief Returns the current logical step position. */
//--------------------------------------------------------------------
  int32_t position() const;

//--------------------------------------------------------------------
  /**
   * @brief Samples the TMC2130 diagnostic/StallGuard input.
   * @return true when the driver diagnostic input is active.
   */
//--------------------------------------------------------------------
  bool stallActive() const;

//--------------------------------------------------------------------
  /**
   * @brief Advances the DDS motion generator by one scheduler tick.
   * @details This method is called only from the Timer1 motion ISR.
   */
//--------------------------------------------------------------------
  void isrTick();

private:
//--------------------------------------------------------------------
  /**
   * @brief Internal step-generation mode.
   */
//--------------------------------------------------------------------
  enum class Mode : uint8_t
  {
    Stopped,    ///< No steps are generated.
    Continuous, ///< Steps continue until stop() or another command is issued.
    Target      ///< Steps stop automatically when mTargetPosition is reached.
  };

//--------------------------------------------------------------------
  /**
   * @brief Converts a requested step rate to the 32-bit phase increment.
   * @param aStepsPerSecond Requested absolute motor step frequency.
   * @return Phase increment applied on every scheduler interrupt.
   */
//--------------------------------------------------------------------
  uint32_t calculatePhaseIncrement(float aStepsPerSecond) const;

  hardware::Tmc2130 &mDriver;              ///< Prusa TMC2130 driver object.
  const hardware::Tmc2130Params &mParams;    ///< Pin/bus mapping for this axis.

  volatile uint32_t mPhaseAccumulator;         ///< DDS accumulator updated in ISR.
  volatile uint32_t mPhaseIncrement;           ///< DDS increment for desired rate.
  volatile int32_t mPosition;                  ///< Signed logical position in steps.
  volatile int32_t mTargetPosition;            ///< Absolute target for bounded moves.
  volatile bool mDirectionPositive;            ///< Current logical motion direction.
  volatile bool mEnabled;                      ///< Last commanded driver enable state.
  volatile Mode mMode;                         ///< Current ISR step-generation mode.
};

} // namespace motion
} // namespace spooler

#endif // PRUSA_SPOOLER_STEPPER_AXIS_HH
