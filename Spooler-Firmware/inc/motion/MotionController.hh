//--------------------------------------------------------------------
/**
 * @file MotionController.hh
 * @brief Declares the fixed-frequency scheduler for the three rewinder axes.
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
 * Timer1 generates a 20 kHz interrupt.  Each interrupt calls isrTick() on the
 * shuttle, take-up, and brake StepperAxis objects.  Individual axes decide
 * whether the current scheduler tick requires an actual physical motor step.
 *
 * The brake normally does not step; it is included in the scheduler so the
 * same axis abstraction can be reused later if active brake motion is added.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_MOTION_CONTROLLER_HH
#define PRUSA_SPOOLER_MOTION_CONTROLLER_HH

#include "motion/StepperAxis.hh"

namespace spooler
{
namespace motion
{

class MotionController
{
public:
//--------------------------------------------------------------------
  /** @brief Binds the controller to the three board motor axes. */
//--------------------------------------------------------------------
  MotionController(StepperAxis &aShuttle,
                   StepperAxis &aTakeup,
                   StepperAxis &aBrake);

//--------------------------------------------------------------------
  /** @brief Configures AVR Timer1 for the fixed-rate motion scheduler. */
//--------------------------------------------------------------------
  void initScheduler();

//--------------------------------------------------------------------
  /** @brief Services all axes for one motion scheduler interrupt. */
//--------------------------------------------------------------------
  void isrTick();

//--------------------------------------------------------------------
  /** @brief Returns the shuttle axis. */
//--------------------------------------------------------------------
  StepperAxis &shuttle();

//--------------------------------------------------------------------
  /** @brief Returns the take-up spool axis. */
//--------------------------------------------------------------------
  StepperAxis &takeup();

//--------------------------------------------------------------------
  /** @brief Returns the supply-spool brake axis. */
//--------------------------------------------------------------------
  StepperAxis &brake();

private:
  StepperAxis &mShuttle; ///< Traverse mechanism driven by original pulley motor.
  StepperAxis &mTakeup;  ///< Receiving spool driven by original selector motor.
  StepperAxis &mBrake;   ///< Supply-spool brake driven by original idler motor.
};

//--------------------------------------------------------------------
/**
 * @brief Pointer used by the global Timer1 ISR to reach the C++ controller.
 */
//--------------------------------------------------------------------
extern MotionController *gMotionController;

} // namespace motion
} // namespace spooler

#endif // PRUSA_SPOOLER_MOTION_CONTROLLER_HH
