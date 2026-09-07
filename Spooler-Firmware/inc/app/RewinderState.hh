//--------------------------------------------------------------------
/**
 * @file RewinderState.hh
 * @brief Defines the top-level operating states and fatal error codes.
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
 * @date 2026-09-05
 *
 * @details
 * The original Prusa MMU application state machine is not used by the spooler.
 * The controller board and low-level motor hardware are reused, but the new
 * application has a much smaller state machine tailored to the rewinder.
 *
 * The normal startup path is:
 *
 *   Boot -> Homing -> HomeBackoff -> Ready
 *
 * From Ready, either arrow button enters AdjustOuterLimit.  A short center-button
 * press starts winding.  While winding, another short center-button press enters
 * Paused without changing the shuttle position or traverse direction, and a further
 * short press resumes from that exact point.  Holding the center button for five
 * seconds starts a complete shuttle re-home cycle.
 *
 * From Ready, either arrow button enters AdjustOuterLimit.  That state first
 * moves the shuttle to the previously saved outer winding limit, then allows
 * the operator to jog the limit inward or outward.  After one minute without
 * an adjustment the shuttle returns to the inner/start position and Ready is
 * restored.  Pressing the center button also returns the shuttle to the inner
 * position, then begins winding when filament is present.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_REWINDER_STATE_HH
#define PRUSA_SPOOLER_REWINDER_STATE_HH

#include <stdint.h>

namespace spooler
{

//--------------------------------------------------------------------
/**
 * @brief High-level rewinder operating states.
 */
//--------------------------------------------------------------------
enum class RewinderState : uint8_t
{
  Boot,             ///< Initial state before the application is initialized.
  Homing,           ///< Shuttle is moving toward the fixed inner spool edge.
  HomeBackoff,      ///< Shuttle is backing away after the StallGuard home event.
  Ready,            ///< Homed and waiting for the operator to begin winding.
  AdjustOuterLimit, ///< Operator is positioning the outer winding boundary.
  Winding,          ///< Take-up spool, shuttle, and brake are actively operating.
  Paused,           ///< Winding is stopped while shuttle position/direction are retained.
  OutOfFilament,    ///< FINDA indicates that the supply filament has run out.
  Error             ///< A fatal condition has stopped the rewinder.
};

//--------------------------------------------------------------------
/**
 * @brief Fatal errors retained by the application state machine.
 */
//--------------------------------------------------------------------
enum class ErrorCode : uint8_t
{
  None,                   ///< No error has been detected.
  ShuttleHomeTimeout,     ///< The shuttle never produced a valid home stall.
  ShuttleUnexpectedStall, ///< Reserved for an unexpected shuttle stall in use.
  DriverInitialization    ///< One or more TMC2130 drivers failed to initialize.
};

} // namespace spooler

#endif // PRUSA_SPOOLER_REWINDER_STATE_HH
