//--------------------------------------------------------------------
/**
 * @file Defaults.hh
 * @brief Defines mechanical, electrical, timing, and user-interface defaults.
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
 * Values expected to change during mechanical calibration are collected here so
 * the state-machine and driver code remain free of rewinder-specific magic
 * numbers.  The initial values are engineering starting points and must be
 * verified on the actual rewinder before unattended operation.
 *
 * All fixed-width integers intentionally come from <stdint.h>.  The AVR GCC
 * 7.3.0 toolchain bundled with the Prusa firmware does not provide the hosted
 * hosted C++ fixed-width-integer header used by desktop toolchains.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_DEFAULTS_HH
#define PRUSA_SPOOLER_DEFAULTS_HH

#include <stdint.h>

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace spooler
{

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace config
{
//--------------------------------------------------------------------
// Filament and spool geometry.
//--------------------------------------------------------------------
constexpr float FILAMENT_DIAMETER_MM   =  1.75F;  ///< Nominal filament size.
constexpr float WINDING_PITCH_MM       =  1.80F;  ///< Traverse advance/rev.
constexpr float SPOOL_WINDING_WIDTH_MM = 60.00F;  ///< Usable flange-to-flange width.
constexpr float SHUTTLE_EDGE_MARGIN_MM =  1.50F;  ///< Keep filament off flange wall.

//--------------------------------------------------------------------
// Shuttle mechanics.
// The starting value assumes a 200-step motor, 8x microstepping, and the
// existing MMU drive geometry.  Measure the actual shuttle travel and update
// this value before relying on calculated edge positions.
//--------------------------------------------------------------------
constexpr float SHUTTLE_STEPS_PER_MM = (200.0F * 8.0F / 19.147274F);
constexpr float TAKEUP_MOTOR_STEPS_PER_REVOLUTION = 200.0F * 8.0F;

//--------------------------------------------------------------------
// Shuttle homing.
// Homing drives toward the fixed inner spool edge until several consecutive
// StallGuard samples are active.  A timeout prevents unlimited motion when the
// sensor/driver or mechanics fail to produce a valid stall indication.
//--------------------------------------------------------------------
constexpr float SHUTTLE_HOME_SPEED_MM_PER_SEC    = 8.0F;
constexpr float SHUTTLE_HOME_BACKOFF_MM          = 2.0F;
constexpr uint16_t SHUTTLE_HOME_TIMEOUT_MS       = 15000U;
constexpr uint8_t SHUTTLE_STALL_SAMPLES_REQUIRED = 4U;

//--------------------------------------------------------------------
// Adjustable outer winding limit.
//
// While stopped, either arrow button enters the outer-limit adjustment state.
// The shuttle first moves to the last saved limit.  Subsequent arrow presses
// move that limit in small increments.  The minimum prevents the outer edge
// from being moved effectively on top of the inner edge.  The maximum is a
// configurable mechanical safety bound and initially matches the existing
// spool-width assumption.
//--------------------------------------------------------------------
constexpr float OUTER_LIMIT_JOG_MM                  =  0.50F;
constexpr float OUTER_LIMIT_ADJUST_SPEED_MM_PER_SEC =  8.0F;
constexpr float MINIMUM_SPOOL_WINDING_WIDTH_MM      = 10.0F;
constexpr float MAXIMUM_SPOOL_WINDING_WIDTH_MM      = 60.0F;
constexpr uint16_t OUTER_LIMIT_TIMEOUT_MS           = 60000U;

//--------------------------------------------------------------------
// EEPROM values are written only when the operator leaves adjustment mode, not
// after every jog.  This preserves the selected spool width across power cycles
// without unnecessarily consuming EEPROM write endurance.
//--------------------------------------------------------------------
constexpr uint16_t OUTER_LIMIT_EEPROM_MAGIC = 0xA55AU;

//--------------------------------------------------------------------
// Driver direction conventions.
// These constants map logical positive rewinder motion to the electrical DIR
// polarity expected by the reused MMU TMC2130/shift-register hardware.
//--------------------------------------------------------------------
constexpr bool SHUTTLE_DRIVER_DIR_ON             = false;
constexpr bool TAKEUP_DRIVER_DIR_ON              = true;
constexpr bool BRAKE_DRIVER_DIR_ON               = true;
constexpr bool TAKEUP_WINDING_DIRECTION_POSITIVE = true;

//--------------------------------------------------------------------
// Operator-selected winding speeds.
// SPEED_LEVEL_COUNT must match the number of elements in TAKEUP_RPM.
//--------------------------------------------------------------------
constexpr uint8_t SPEED_LEVEL_COUNT   = 5U;
constexpr uint8_t DEFAULT_SPEED_LEVEL = 2U;
constexpr float TAKEUP_RPM[SPEED_LEVEL_COUNT] =
{
  8.0F,
  12.0F,
  18.0F,
  25.0F,
  35.0F
};

//--------------------------------------------------------------------
// Supply-spool brake current.
// The initial implementation holds the brake motor stationary and uses TMC2130
// current as drag torque.  Begin conservatively and verify motor/driver heating
// and filament tension on the physical rewinder.
//--------------------------------------------------------------------
constexpr uint8_t BRAKE_RUN_CURRENT  = 6U;
constexpr uint8_t BRAKE_HOLD_CURRENT = 6U;

//--------------------------------------------------------------------
// Buttons and FINDA input filtering.
// The three buttons share ADC channel 5 through the original MMU resistor
// network.  Values between the defined ranges are treated as no button.
//--------------------------------------------------------------------
constexpr uint8_t BUTTONS_ADC_CHANNEL =  5U;
constexpr uint16_t BUTTON_DEBOUNCE_MS = 30U;
constexpr uint16_t FINDA_DEBOUNCE_MS  = 80U;

//--------------------------------------------------------------------
// Center-button gesture timing.
// A normal press/release starts or pauses winding.  Holding the center button
// continuously for this interval cancels the short-press action and starts a
// full StallGuard shuttle re-home cycle.
//--------------------------------------------------------------------
constexpr uint16_t CENTER_BUTTON_REHOME_HOLD_MS = 5000U;

constexpr uint16_t RIGHT_BUTTON_MAX   =  50U;
constexpr uint16_t MIDDLE_BUTTON_MIN  =  80U;
constexpr uint16_t MIDDLE_BUTTON_MAX  = 100U;
constexpr uint16_t LEFT_BUTTON_MIN    = 160U;
constexpr uint16_t LEFT_BUTTON_MAX    = 180U;

//--------------------------------------------------------------------
// In the rewinder mechanics, a FINDA-active/high condition represents the ball
// near the sensor and therefore NO filament.  Change this constant if the final
// wiring or sensor polarity is different.
//--------------------------------------------------------------------
constexpr bool FINDA_HIGH_MEANS_NO_FILAMENT = true;

//--------------------------------------------------------------------
// Scheduler and display timing.
// Timer1 configuration in MotionController.cc currently assumes exactly 20 kHz.
//--------------------------------------------------------------------
constexpr uint32_t STEP_SCHEDULER_HZ = 20000UL;
constexpr uint16_t LED_BLINK_PERIOD_MS = 500U;

} // namespace config
} // namespace spooler

#endif // PRUSA_SPOOLER_DEFAULTS_HH
