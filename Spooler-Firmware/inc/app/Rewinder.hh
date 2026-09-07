//--------------------------------------------------------------------
/**
 * @file Rewinder.hh
 * @brief Declares the high-level filament rewinder application state machine.
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
 * Rewinder coordinates all application behavior while Board handles the reused
 * MMU electronics.  On power-up the shuttle is driven toward the fixed inner
 * edge of the receiving spool until TMC2130 StallGuard is detected.  The
 * shuttle then backs away from the physical stop and that backed-off location
 * becomes logical position zero.
 *
 * The outer winding edge is operator adjustable.  From Ready, either arrow
 * moves the shuttle to the last saved outer limit and enters AdjustOuterLimit.
 * Additional arrow presses jog the limit.  The selected limit is persisted in
 * EEPROM when adjustment ends, and is used directly by the winding reversal
 * logic instead of the fixed default spool width.
 */
//--------------------------------------------------------------------
#ifndef PRUSA_SPOOLER_REWINDER_HH
#define PRUSA_SPOOLER_REWINDER_HH

// Global includes
#include <stdint.h>

// Local includes
#include "app/RewinderState.hh"
#include "hardware/Board.hh"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace spooler
{

//--------------------------------------------------------------------
//--------------------------------------------------------------------
class Rewinder
{
public:
//--------------------------------------------------------------------
/** 
  *@brief Creates the application around the hardware board abstraction. 
  */
//--------------------------------------------------------------------
  explicit Rewinder(hardware::Board &aBoard);

//--------------------------------------------------------------------
  /**
   * @brief Starts application operation after hardware initialization.
   * @param aDriversOk Result returned by Board::init().
   */
//--------------------------------------------------------------------
  void init(bool aDriversOk);

//--------------------------------------------------------------------
  /** @brief Services one iteration of the rewinder state machine. */
//--------------------------------------------------------------------
  void update();

//--------------------------------------------------------------------
  /** @brief Returns the current high-level application state. */
//--------------------------------------------------------------------
  RewinderState state() const;

//--------------------------------------------------------------------
  /** @brief Returns the most recent fatal application error. */
//--------------------------------------------------------------------
  ErrorCode error() const;

//--------------------------------------------------------------------
  /** @brief Returns the current zero-based winding speed level. */
//--------------------------------------------------------------------
  uint8_t speedLevel() const;

private:
//--------------------------------------------------------------------
  /** @brief Phases used while the AdjustOuterLimit state is active. */
//--------------------------------------------------------------------
  enum class OuterLimitPhase : uint8_t
  {
    MovingToSavedLimit, ///< Traveling from the current position to saved limit.
    Adjusting,          ///< Waiting for Left/Right jog commands.
    ReturningHome       ///< Returning to position zero before exit/start.
  };

//--------------------------------------------------------------------
  /** @brief Result of decoding the center-button press/hold gesture. */
//--------------------------------------------------------------------
  enum class CenterButtonAction : uint8_t
  {
    None,       ///< No completed center-button gesture this update.
    ShortPress, ///< Button was released before the long-hold threshold.
    LongPress   ///< Button remained pressed through the re-home threshold.
  };

//--------------------------------------------------------------------
  /** @brief Records a new state and its entry time. */
//--------------------------------------------------------------------
  void enterState(RewinderState aState);

//--------------------------------------------------------------------
  /** @brief Starts inward shuttle motion used to establish the home edge. */
//--------------------------------------------------------------------
  void startHoming();

//--------------------------------------------------------------------
  /** @brief Processes StallGuard qualification and the homing timeout. */
//--------------------------------------------------------------------
  void updateHoming();

//--------------------------------------------------------------------
  /** @brief Waits for the post-stall backoff move to complete. */
//--------------------------------------------------------------------
  void updateHomeBackoff();

//--------------------------------------------------------------------
  /** @brief Processes the small set of commands available in Ready. */
//--------------------------------------------------------------------
  void updateReady();

//--------------------------------------------------------------------
  /** @brief Services the paused state while preserving traverse position/direction. */
//--------------------------------------------------------------------
  void updatePaused();

//--------------------------------------------------------------------
  /**
   * @brief Decodes center-button short presses and the five-second re-home hold.
   * @return Gesture completed during this update, or None.
   */
//--------------------------------------------------------------------
  CenterButtonAction updateCenterButtonGesture();

//--------------------------------------------------------------------
  /** @brief Starts outer-limit setup by moving to the saved outer position. */
//--------------------------------------------------------------------
  void startOuterLimitAdjustment();

//--------------------------------------------------------------------
  /** @brief Services the dedicated outer-limit adjustment state. */
//--------------------------------------------------------------------
  void updateOuterLimitAdjustment();

//--------------------------------------------------------------------
  /** @brief Handles travel to the previously stored outer limit. */
//--------------------------------------------------------------------
  void updateMovingToSavedOuterLimit();

//--------------------------------------------------------------------
  /** @brief Handles Left/Right jog commands and the inactivity timeout. */
//--------------------------------------------------------------------
  void updateOuterLimitJogging();

//--------------------------------------------------------------------
  /** @brief Waits for the shuttle to return to the inner/start position. */
//--------------------------------------------------------------------
  void updateOuterLimitReturnHome();

//--------------------------------------------------------------------
  /**
   * @brief Moves the outer limit by a signed number of steps.
   * @param aDeltaSteps Positive moves outward; negative moves inward.
   */
//--------------------------------------------------------------------
  void jogOuterLimit(int32_t aDeltaSteps);

//--------------------------------------------------------------------
  /**
   * @brief Starts the return from adjustment mode to position zero.
   * @param aStartAfterReturn true to begin winding after the return completes.
   */
//--------------------------------------------------------------------
  void beginOuterLimitReturn(bool aStartAfterReturn);

//--------------------------------------------------------------------
  /** @brief Attempts to start winding from Ready without moving the shuttle. */
//--------------------------------------------------------------------
  void requestStartFromReady();

//--------------------------------------------------------------------
  /** @brief Enables motors/brake and enters the Winding state. */
//--------------------------------------------------------------------
  void startWinding();

//--------------------------------------------------------------------
  /** @brief Stops take-up/shuttle motion and releases the brake driver. */
//--------------------------------------------------------------------
  void stopWinding();

//--------------------------------------------------------------------
  /** @brief Recalculates motor step rates for the current speed level. */
//--------------------------------------------------------------------
  void applyWindingRates();

//--------------------------------------------------------------------
  /** @brief Reverses shuttle direction at the selected spool edges. */
//--------------------------------------------------------------------
  void updateShuttleDirectionAtEdges();

//--------------------------------------------------------------------
  /** @brief Services buttons, filament sensing, and edge reversal while winding. */
//--------------------------------------------------------------------
  void updateWinding();

//--------------------------------------------------------------------
  /** @brief Handles recovery after the FINDA sensor reports no filament. */
//--------------------------------------------------------------------
  void updateOutOfFilament();

//--------------------------------------------------------------------
  /** @brief Maintains the fatal-error indication while the application is stopped. */
//--------------------------------------------------------------------
  void updateError();

//--------------------------------------------------------------------
  /** @brief Applies or removes supply-spool braking torque. */
//--------------------------------------------------------------------
  void setBrake(bool aEnabled);

//--------------------------------------------------------------------
  /** @brief Records a fatal error and transitions to Error. */
//--------------------------------------------------------------------
  void fail(ErrorCode aError);

//--------------------------------------------------------------------
  /** @brief Returns the default outer limit derived from configured geometry. */
//--------------------------------------------------------------------
  int32_t defaultOuterLimitSteps() const;

//--------------------------------------------------------------------
  /** @brief Returns the minimum permitted operator-selected outer limit. */
//--------------------------------------------------------------------
  int32_t minimumOuterLimitSteps() const;

//--------------------------------------------------------------------
  /** @brief Returns the maximum permitted operator-selected outer limit. */
//--------------------------------------------------------------------
  int32_t maximumOuterLimitSteps() const;

//--------------------------------------------------------------------
  /** @brief Returns the number of motor steps in one operator jog increment. */
//--------------------------------------------------------------------
  int32_t outerLimitJogSteps() const;

//--------------------------------------------------------------------
  /** @brief Returns the step rate used during outer-limit positioning. */
//--------------------------------------------------------------------
  float outerLimitAdjustStepsPerSecond() const;

//--------------------------------------------------------------------
  /** @brief Loads a valid persisted outer limit or the configured default. */
//--------------------------------------------------------------------
  void loadOuterLimit();

//--------------------------------------------------------------------
  /** @brief Persists the current outer limit to AVR EEPROM if it changed. */
//--------------------------------------------------------------------
  void saveOuterLimit() const;

//--------------------------------------------------------------------
  /** @brief Calculates take-up motor steps/second for mSpeedLevel. */
//--------------------------------------------------------------------
  float takeupStepsPerSecond() const;

//--------------------------------------------------------------------
  /** @brief Calculates traverse steps/second required for winding pitch. */
//--------------------------------------------------------------------
  float shuttleStepsPerSecond() const;

#ifdef SPOOLER_DEBUG
  //--------------------------------------------------------------------
  /**
   * @brief Prints the current rewinder state and status when they change.
   *
   * This method is compiled only when SPOOLER_DEBUG is defined.  It
   * remembers the last values that were printed and produces diagnostic
   * output only when the application state, error code, speed level, or
   * filament-present status changes.
   */
  //--------------------------------------------------------------------
  void printDebugStatus();
#endif

  hardware::Board &mBoard;              ///< Reused MMU board hardware abstraction.
  RewinderState mState;                 ///< Current application state.
  ErrorCode mError;                     ///< Latched fatal error code.
  uint8_t mSpeedLevel;                  ///< Zero-based index into TAKEUP_RPM[].
  uint16_t mStateStartMs;               ///< Millisecond timestamp of state entry.
  uint8_t mHomingStallSamples;          ///< Consecutive StallGuard samples at home.
  bool mShuttleMovingOutward;           ///< true when shuttle moves away from home.
  bool mCenterButtonPressActive;        ///< true while a debounced center press is active.
  bool mCenterButtonLongPressHandled;   ///< suppresses short action after a long hold.
  uint16_t mCenterButtonPressedMs;      ///< Millisecond timestamp of center-button press.
  CenterButtonAction mCenterButtonAction; ///< Gesture decoded for the current update.
  int32_t mOuterLimitSteps;             ///< User-selected outer winding boundary.
  uint16_t mLastOuterLimitActivityMs;   ///< Last arrow activity during adjustment.
  OuterLimitPhase mOuterLimitPhase;     ///< Current phase of outer-limit setup.
  bool mStartAfterOuterLimitReturn;     ///< Start winding after returning to zero.
};

} // namespace spooler

#endif // PRUSA_SPOOLER_REWINDER_HH
