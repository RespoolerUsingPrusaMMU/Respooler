//--------------------------------------------------------------------
/**
 * @file StepperAxis.cc
 * @brief Implements the generic TMC2130-backed motion axis.
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
// Global Includes
#include <util/atomic.h>

// Local Includes
#include "motion/StepperAxis.hh"
#include "config/Defaults.hh"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace spooler
{
//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace motion
{

//--------------------------------------------------------------------
/**
 * @brief Construct a new StepperAxis object.
 *
 * @param aDriver Reference to the TMC2130 driver.
 * @param aParams Reference to the TMC2130 driver parameters.
 */
//--------------------------------------------------------------------
StepperAxis::StepperAxis(hardware::Tmc2130 &aDriver,
                         const hardware::Tmc2130Params &aParams)
  : mDriver(aDriver),
    mParams(aParams),
    mPhaseAccumulator(0UL),
    mPhaseIncrement(0UL),
    mPosition(0L),
    mTargetPosition(0L),
    mDirectionPositive(true),
    mEnabled(false),
    mMode(Mode::Stopped)
{
}

//--------------------------------------------------------------------
/**
 * @brief Enable or disable the stepper axis.
 *
 * @param aEnabled True to enable the axis, false to disable it.
 */
//--------------------------------------------------------------------
void StepperAxis::setEnabled(bool aEnabled)
{
  mDriver.setEnabled(mParams, aEnabled);

  // Ensure that changes to mEnabled and mMode are atomic with respect to interrupts.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    mEnabled = aEnabled;

    if(!aEnabled)
    {
      mMode = Mode::Stopped;
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Get the enabled state of the stepper axis.
 *
 * @return True if the axis is enabled, false otherwise.
 */
//--------------------------------------------------------------------  
bool StepperAxis::enabled() const
{
  bool tEnabled = false;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    tEnabled = mEnabled;
  }

  return tEnabled;
}

//--------------------------------------------------------------------
/**
 * @brief Set the run and hold currents for the stepper axis.
 *
 * @param aRunCurrent The run current in arbitrary units.
 * @param aHoldCurrent The hold current in arbitrary units.
 */
//--------------------------------------------------------------------
void StepperAxis::setCurrent(uint8_t aRunCurrent,
                             uint8_t aHoldCurrent)
{
  mDriver.setCurrents(
    mParams,
    hardware::Tmc2130Currents(aRunCurrent, aHoldCurrent));
}

//--------------------------------------------------------------------
/**
 * @brief Set the direction of the stepper axis.
 *
 * @param aPositive True for positive direction, false for negative direction.
 */
//--------------------------------------------------------------------
void StepperAxis::setDirection(bool aPositive)
{
  hardware::Tmc2130::setDirection(mParams, aPositive);

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    mDirectionPositive = aPositive;
  }
}

//--------------------------------------------------------------------
/**
 * @brief Get the current direction of the stepper axis.
 *
 * @return True if the direction is positive, false if negative.
 */
//--------------------------------------------------------------------
bool StepperAxis::directionPositive() const
{
  bool tDirectionPositive = false;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    tDirectionPositive = mDirectionPositive;
  }

  return tDirectionPositive;
}

//--------------------------------------------------------------------
/**
 * @brief Calculate the phase increment for a given step rate.
 *
 * This method converts a desired step rate in steps per second into a
 * phase increment suitable for the stepper axis's internal accumulator.
 *
 * @param aStepsPerSecond The desired step rate in steps per second.
 * @return The calculated phase increment.
 */
//--------------------------------------------------------------------
uint32_t StepperAxis::calculatePhaseIncrement(float aStepsPerSecond) const
{
  // A full 32-bit accumulator wrap corresponds to one motor step.  Therefore:
  //
  //   increment = requested_steps_per_second * 2^32 / scheduler_hz
  //
  // The calculation is performed outside the ISR; the ISR only adds integers.
  uint32_t tIncrement = 0UL;

  if(aStepsPerSecond > 0.0F)
  {
    const float tScale = 4294967296.0F / static_cast<float>(config::STEP_SCHEDULER_HZ);
    tIncrement = static_cast<uint32_t>(aStepsPerSecond * tScale);

    if(tIncrement == 0UL)
    {
      tIncrement = 1UL;
    }
  }

  return tIncrement;
}

//--------------------------------------------------------------------
/**
 * @brief Set the step rate for the stepper axis.
 *
 * @param aStepsPerSecond The desired step rate in steps per second.
 */
//--------------------------------------------------------------------
void StepperAxis::setRate(float aStepsPerSecond)
{
  const uint32_t tIncrement = calculatePhaseIncrement(aStepsPerSecond);

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    mPhaseIncrement = tIncrement;
    mMode = (tIncrement == 0UL) ? Mode::Stopped : Mode::Continuous;
  }
}

//--------------------------------------------------------------------
/**
 * @brief Stop the stepper axis.
 *
 * This method immediately stops the stepper axis by setting the phase
 * increment to zero and updating the mode to Stopped.
 */
//--------------------------------------------------------------------
void StepperAxis::stop()
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    mPhaseIncrement = 0UL;
    mMode = Mode::Stopped;
  }
}

//--------------------------------------------------------------------
/**
 * @brief Move the stepper axis to a specific target position.
 *
 * @param aTargetPosition The desired target position in steps.
 * @param aStepsPerSecond The desired step rate in steps per second.
 */
//--------------------------------------------------------------------
void StepperAxis::moveTo(int32_t aTargetPosition,
                         float aStepsPerSecond)
{
  const int32_t tCurrentPosition = position();
  const uint32_t tIncrement = calculatePhaseIncrement(aStepsPerSecond);

  // If the target position is the same as the current position or the 
  // step rate is zero, stop the axis.
  if((aTargetPosition == tCurrentPosition) || (tIncrement == 0UL))
  {
    stop();
  }
  // Otherwise, move towards the target position.
  else
  {
    setDirection(aTargetPosition > tCurrentPosition);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      mTargetPosition = aTargetPosition;
      mPhaseIncrement = tIncrement;
      mMode = Mode::Target;
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Move the stepper axis by a relative number of steps.
 *
 * @param aDeltaSteps The number of steps to move relative to the current position.
 * @param aStepsPerSecond The desired step rate in steps per second.
 */
//--------------------------------------------------------------------
void StepperAxis::moveBy(int32_t aDeltaSteps,
                         float aStepsPerSecond)
{
  moveTo(position() + aDeltaSteps, aStepsPerSecond);
}

//--------------------------------------------------------------------
/**
 * @brief Check if the move is complete.
 *
 * @return true if the stepper axis has stopped moving, false otherwise.
 */
//--------------------------------------------------------------------
bool StepperAxis::moveComplete() const
{
  Mode tMode = Mode::Stopped;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    tMode = mMode;
  }

  return (tMode == Mode::Stopped);
}

//--------------------------------------------------------------------
/**
 * @brief Set the current position of the stepper axis.
 *
 * @param aPosition The new position in steps.
 */
//--------------------------------------------------------------------
void StepperAxis::setPosition(int32_t aPosition)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    mPosition = aPosition;
    mTargetPosition = aPosition;
  }
}

//--------------------------------------------------------------------
/**
 * @brief Get the current position of the stepper axis.
 *
 * @return The current position in steps.
 */
//--------------------------------------------------------------------
int32_t StepperAxis::position() const
{
  int32_t tPosition = 0L;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    tPosition = mPosition;
  }

  return tPosition;
}

//--------------------------------------------------------------------
/**
 * @brief Check if the stepper axis is currently stalled.
 *
 * @return true if the stepper axis is stalled, false otherwise.
 */
//--------------------------------------------------------------------
bool StepperAxis::stallActive() const
{
  const bool tStallActive = hardware::Tmc2130::stallActive(mParams);
  return tStallActive;
}

//--------------------------------------------------------------------
/**
 * @brief Interrupt service routine tick for the stepper axis.
 *
 * This function should be called periodically from a timer interrupt.
 */
//--------------------------------------------------------------------
void StepperAxis::isrTick()
{
  bool tGenerateStep = false;

  // Unsigned overflow is the DDS carry event.  Comparing the accumulator after
  // addition with its previous value detects the wrap without 64-bit arithmetic.
  if(mEnabled && (mMode != Mode::Stopped) && (mPhaseIncrement != 0UL))
  {
    const uint32_t tBefore = mPhaseAccumulator;
    mPhaseAccumulator += mPhaseIncrement;

    if(mPhaseAccumulator < tBefore)
    {
      tGenerateStep = true;
    }
  }

  // Generate a step pulse if needed.
  if(tGenerateStep)
  {
    hardware::Tmc2130::step(mParams);
    mPosition += mDirectionPositive ? 1L : -1L;

    // Target moves use signed logical position and stop on or beyond the target
    // so one delayed scheduler tick cannot cause an endless move past the edge.
    if(mMode == Mode::Target)
    {
      const bool tReachedPositiveTarget = mDirectionPositive && (mPosition >= mTargetPosition);
      const bool tReachedNegativeTarget = (!mDirectionPositive) && (mPosition <= mTargetPosition);

      // Check if the target position has been reached.
      if(tReachedPositiveTarget || tReachedNegativeTarget)
      {
        mPhaseIncrement = 0UL;
        mMode = Mode::Stopped;
      }
    }
  }
}

} // namespace motion
} // namespace spooler
//--------------------------------------------------------------------
