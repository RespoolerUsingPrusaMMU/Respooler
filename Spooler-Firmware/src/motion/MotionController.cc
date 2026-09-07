//--------------------------------------------------------------------
/**
 * @file MotionController.cc
 * @brief Implements the timer-driven scheduler for the three rewinder axes.
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
#include <avr/interrupt.h>
#include <avr/io.h>

// Local Includes
#include "motion/MotionController.hh"
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
 * @brief Global pointer to the active MotionController instance.
 */
//--------------------------------------------------------------------
MotionController *gMotionController = 0;

//--------------------------------------------------------------------
/**
 * @brief Construct a new MotionController object.
 *
 * @param aShuttle Reference to the shuttle axis stepper.
 * @param aTakeup Reference to the takeup axis stepper.
 * @param aBrake Reference to the brake axis stepper.
 */
//--------------------------------------------------------------------
MotionController::MotionController(StepperAxis &aShuttle,
                                   StepperAxis &aTakeup,
                                   StepperAxis &aBrake)
  : mShuttle(aShuttle),
    mTakeup(aTakeup),
    mBrake(aBrake)
{
  gMotionController = this;
}

//--------------------------------------------------------------------
/**
 * @brief Initialize the motion controller's scheduler (Timer1).
 *
 * Configures Timer1 to generate interrupts at a fixed frequency defined
 * by config::STEP_SCHEDULER_HZ.
 *
 * @note The ISR frequency is critical for accurate stepper motor control.
 * 16 MHz CPU Clock
 *  |
 *  | prescaler / 8
 *  v
 * 2 MHz Timer1 clock
 *  |
 *  | counts 100 ticks: 0..99
 *  v
 * 20,000 interrupts/second (every 50 μs)
 */
//--------------------------------------------------------------------
void MotionController::initScheduler()
{
  // With F_CPU = 16 MHz and a Timer1 prescaler of 8, OCR1A = 99 produces:
  //   16,000,000 / 8 / (99 + 1) = 20,000 interrupts/second.
  // StepperAxis phase increments are calculated against this fixed frequency.
  static_assert(config::STEP_SCHEDULER_HZ == 20000UL, "Timer1 setup assumes a 20 kHz scheduler");

  // Configure Timer1 for CTC mode with OCR1A as the top value.
  TCCR1A =  0U;  ///< Clear Timer/Counter Control Register A
  TCCR1B =  0U;  ///< Clear Timer/Counter Control Register B
  TCNT1  =  0U;  ///< Clear Timer/Counter1
  OCR1A  = 99U;  ///< Set Output Compare Register A for 20 kHz interrupts
  TCCR1B = static_cast<uint8_t>((1U << WGM12) | (1U << CS11));
  TIMSK1 |= static_cast<uint8_t>(1U << OCIE1A);
}

//--------------------------------------------------------------------
/**
 * @brief ISR tick handler for the motion controller.
 *
 * This function is called every 50 μs by the Timer1 compare-match interrupt.
 */
//--------------------------------------------------------------------
void MotionController::isrTick()
{
  // Keep the ISR deterministic: each axis performs only phase accumulation,
  // optional STEP generation, and target-position bookkeeping.
  mShuttle.isrTick();
  mTakeup.isrTick();
  mBrake.isrTick();
}

//--------------------------------------------------------------------
/**
 * @brief Get a reference to the shuttle stepper axis.
 * @return Reference to the shuttle StepperAxis object.
 */
//--------------------------------------------------------------------
StepperAxis &MotionController::shuttle()
{
  return mShuttle;
}

//--------------------------------------------------------------------
/**
 * @brief Get a reference to the takeup stepper axis.
 * @return Reference to the takeup StepperAxis object.
 */
//--------------------------------------------------------------------
StepperAxis &MotionController::takeup()
{
  return mTakeup;
}

//--------------------------------------------------------------------
/**
 * @brief Get a reference to the brake stepper axis.
 * @return Reference to the brake StepperAxis object.
 */
//--------------------------------------------------------------------
StepperAxis &MotionController::brake()
{
  return mBrake;
}

} // namespace motion
} // namespace spooler

//--------------------------------------------------------------------
/**
 * @brief Timer1 compare-match interrupt service routine.
 *
 * This ISR is triggered every 50 μs and calls the motion controller's isrTick() method.
 */
//--------------------------------------------------------------------
ISR(TIMER1_COMPA_vect)
{
  if(spooler::motion::gMotionController != 0)
  {
    spooler::motion::gMotionController->isrTick();
  }
}
