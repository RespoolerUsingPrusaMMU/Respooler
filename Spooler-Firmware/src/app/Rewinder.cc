//--------------------------------------------------------------------
/**
 * @file Rewinder.cc
 * @brief Implements the high-level filament rewinder application state machine.
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
 */
//--------------------------------------------------------------------
// Global Includes
#include <avr/eeprom.h>

// Local Includes
#include "app/Rewinder.hh"
#include "config/Defaults.hh"

#ifdef SPOOLER_DEBUG
#include "hardware/Debug.hh"
#endif

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace
{
//--------------------------------------------------------------------
/** EEPROM marker used to distinguish initialized outer-limit data. */
//--------------------------------------------------------------------
uint16_t EEMEM gOuterLimitMagic = 0xFFFFU;

//--------------------------------------------------------------------
/** EEPROM copy of the operator-selected outer limit in shuttle steps. */
//--------------------------------------------------------------------
uint16_t EEMEM gOuterLimitSteps = 0xFFFFU;

} // namespace

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace spooler
{

Rewinder::Rewinder(hardware::Board &aBoard)
  : mBoard(aBoard),
    mState(RewinderState::Boot),
    mError(ErrorCode::None),
    mSpeedLevel(config::DEFAULT_SPEED_LEVEL),
    mStateStartMs(0U),
    mHomingStallSamples(0U),
    mShuttleMovingOutward(true),
    mCenterButtonPressActive(false),
    mCenterButtonLongPressHandled(false),
    mCenterButtonPressedMs(0U),
    mCenterButtonAction(CenterButtonAction::None),
    mOuterLimitSteps(0L),
    mLastOuterLimitActivityMs(0U),
    mOuterLimitPhase(OuterLimitPhase::MovingToSavedLimit),
    mStartAfterOuterLimitReturn(false)
{
}

//--------------------------------------------------------------------
/**
 * @brief Initialize the rewinder.
 *
 * @param aDriversOk Indicates whether the motor drivers are correctly initialized.
 */
//--------------------------------------------------------------------
void Rewinder::init(bool aDriversOk)
{
  if(aDriversOk)
  {
    // Load the operator's last spool-width setting before homing.  The value is
    // not used until homing establishes the position-zero reference.
    loadOuterLimit();
    startHoming();
  }
  else
  {
    fail(ErrorCode::DriverInitialization);
  }
}

//--------------------------------------------------------------------
/**
 * @brief Enter a new rewinder state.
 *
 * @param aState The new state to enter.
 */
//--------------------------------------------------------------------
void Rewinder::enterState(RewinderState aState)
{
  mState        = aState;
  mStateStartMs = mBoard.millis();
}

//--------------------------------------------------------------------
/**
 * @brief Start the homing sequence for the shuttle.
 */
//--------------------------------------------------------------------
void Rewinder::startHoming()
{
#ifdef SPOOLER_DEBUG
  spooler::hardware::Debug::printLine("State -> Homing");
#endif

  // Homing always begins from a completely stopped winding state.  This
  // prevents the take-up spool or brake from influencing the shuttle load
  // while StallGuard is being used to locate the fixed inner edge.
  stopWinding();
  mBoard.shuttle().setEnabled(true);
  mBoard.shuttle().setDirection(false); // Set direction towards the home position
  mBoard.shuttle().setRate(config::SHUTTLE_HOME_SPEED_MM_PER_SEC * config::SHUTTLE_STEPS_PER_MM);
  mHomingStallSamples = 0U;
  enterState(RewinderState::Homing);
}

//--------------------------------------------------------------------
/**
 * @brief Update the homing sequence for the shuttle.
 */
//--------------------------------------------------------------------
void Rewinder::updateHoming()
{
  bool tHomingComplete = false;

  // Require several consecutive StallGuard samples before accepting home.
  // The configured threshold is reached long before an uint8_t counter could
  // overflow, so a separate saturation check is unnecessary here.
  if(mBoard.shuttle().stallActive())
  {
    ++mHomingStallSamples;
  }
  else
  {
    mHomingStallSamples = 0U;
  }

  // Check if the required number of StallGuard samples has been reached.
  if(mHomingStallSamples >= config::SHUTTLE_STALL_SAMPLES_REQUIRED)
  {
    // The first coordinate origin is the physical stall point.  The shuttle
    // is then backed away from the hard stop and zero is re-established at the
    // backed-off position so normal winding never runs against the end stop.
    mBoard.shuttle().stop();
    mBoard.shuttle().setPosition(0L);

    const int32_t tBackoff = static_cast<int32_t>((config::SHUTTLE_HOME_BACKOFF_MM * config::SHUTTLE_STEPS_PER_MM) + 0.5F);
    mBoard.shuttle().setDirection(true);
    mBoard.shuttle().moveTo(tBackoff, config::SHUTTLE_HOME_SPEED_MM_PER_SEC * config::SHUTTLE_STEPS_PER_MM);
    enterState(RewinderState::HomeBackoff);
    tHomingComplete = true;
  }

  // If homing is not complete, check for a timeout.
  if(!tHomingComplete)
  {
    // Unsigned subtraction intentionally handles rollover of the 16-bit
    // millisecond counter as long as the timeout remains below 65536 ms.
    const uint16_t tElapsed = static_cast<uint16_t>(mBoard.millis() - mStateStartMs);

    if(tElapsed > config::SHUTTLE_HOME_TIMEOUT_MS)
    {
      fail(ErrorCode::ShuttleHomeTimeout);
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the shuttle state after backing off from the home position.
 */
//--------------------------------------------------------------------
void Rewinder::updateHomeBackoff()
{
  // Check if the shuttle has completed its move after backing off from home.
  if(mBoard.shuttle().moveComplete())
  {
    // The backed-off point, rather than the hard stop itself, is the normal
    // inner/start position.  All operator-selected outer limits are measured
    // from this logical zero.
    mBoard.shuttle().setPosition(0L);
    mShuttleMovingOutward = true;
    mBoard.shuttle().setDirection(true);
    enterState(RewinderState::Ready);
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the shuttle state when it is ready for operation.
 */
//--------------------------------------------------------------------
void Rewinder::updateReady()
{
  const bool tLeftPressed = mBoard.buttons().justPressed(hardware::Button::Left);
  const bool tRightPressed = mBoard.buttons().justPressed(hardware::Button::Right);

  // Either arrow enters adjustment mode.  The first press intentionally does
  // not alter the saved limit; it only asks the shuttle to travel to that
  // location so the operator can see the current setting before changing it.
  if(tLeftPressed || tRightPressed)
  {
    startOuterLimitAdjustment();
  }
  else if(mCenterButtonAction == CenterButtonAction::ShortPress)
  {
    requestStartFromReady();
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the rewinder while winding is paused.
 *
 * @details
 * Pausing intentionally leaves both the logical shuttle position and
 * mShuttleMovingOutward unchanged.  A short center-button press therefore
 * resumes the traverse from the exact location and direction at which the
 * operator paused it.
 */
//--------------------------------------------------------------------
void Rewinder::updatePaused()
{
  if(mCenterButtonAction == CenterButtonAction::ShortPress)
  {
    if(mBoard.finda().filamentPresent())
    {
      startWinding();
    }
    else
    {
      enterState(RewinderState::OutOfFilament);
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Decode center-button short-press and long-hold gestures.
 *
 * @details
 * A short press is reported only after the debounced center button is released.
 * This delay is necessary because the firmware cannot know at button-down time
 * whether the operator intends a normal start/pause press or a five-second
 * re-home hold.  When the hold threshold is reached, LongPress is returned
 * immediately and the eventual release is suppressed so it cannot also create
 * a ShortPress action.
 *
 * Unsigned 16-bit subtraction intentionally handles the wrapping millisecond
 * counter.  The five-second interval is far below one complete 16-bit period.
 *
 * @return CenterButtonAction generated during this update.
 */
//--------------------------------------------------------------------
Rewinder::CenterButtonAction Rewinder::updateCenterButtonGesture()
{
  CenterButtonAction tAction = CenterButtonAction::None;
  const bool tPressed = mBoard.buttons().pressed(hardware::Button::Middle);
  const uint16_t tNowMs = mBoard.millis();

  if(tPressed)
  {
    if(!mCenterButtonPressActive)
    {
      mCenterButtonPressActive = true;
      mCenterButtonLongPressHandled = false;
      mCenterButtonPressedMs = tNowMs;
    }
    else if(!mCenterButtonLongPressHandled)
    {
      const uint16_t tElapsed =
        static_cast<uint16_t>(tNowMs - mCenterButtonPressedMs);

      if(tElapsed >= config::CENTER_BUTTON_REHOME_HOLD_MS)
      {
        mCenterButtonLongPressHandled = true;
        tAction = CenterButtonAction::LongPress;
      }
    }
  }
  else if(mCenterButtonPressActive)
  {
    mCenterButtonPressActive = false;

    if(!mCenterButtonLongPressHandled)
    {
      tAction = CenterButtonAction::ShortPress;
    }

    mCenterButtonLongPressHandled = false;
  }

  return tAction;
}

//--------------------------------------------------------------------
/**
 * @brief Begin the process of adjusting the outer limit.
 */
//--------------------------------------------------------------------
void Rewinder::startOuterLimitAdjustment()
{
  mBoard.shuttle().setEnabled(true);
  mLastOuterLimitActivityMs = mBoard.millis();
  mOuterLimitPhase = OuterLimitPhase::MovingToSavedLimit;
  mStartAfterOuterLimitReturn = false;

  mBoard.shuttle().moveTo(mOuterLimitSteps, outerLimitAdjustStepsPerSecond());

  enterState(RewinderState::AdjustOuterLimit);
}

//--------------------------------------------------------------------
/**
 * @brief Update the shuttle state during outer limit adjustment.
 */
//--------------------------------------------------------------------
void Rewinder::updateOuterLimitAdjustment()
{
  // Determine the current phase of outer limit adjustment and call the
  // appropriate update function.
  switch(mOuterLimitPhase)
  {
    case OuterLimitPhase::MovingToSavedLimit:updateMovingToSavedOuterLimit(); break;
    case OuterLimitPhase::Adjusting:         updateOuterLimitJogging();       break;
    case OuterLimitPhase::ReturningHome:     updateOuterLimitReturnHome();    break;
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the shuttle state while moving to the saved outer limit.
 */
//--------------------------------------------------------------------
void Rewinder::updateMovingToSavedOuterLimit()
{
  // The center button always means "start".  If it is pressed while traveling
  // toward the saved limit, immediately reverse the shuttle and return to the
  // inner/start position before winding begins.
  if(mCenterButtonAction == CenterButtonAction::ShortPress)
  {
    beginOuterLimitReturn(false);
  }
  else if(mBoard.shuttle().moveComplete())
  {
    mOuterLimitPhase = OuterLimitPhase::Adjusting;
    mLastOuterLimitActivityMs = mBoard.millis();
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the shuttle state while jogging the outer limit.
 */
//--------------------------------------------------------------------
void Rewinder::updateOuterLimitJogging()
{
  bool tActivity = false;

  if(mBoard.buttons().justPressed(hardware::Button::Right))
  {
    jogOuterLimit(outerLimitJogSteps());
    tActivity = true;
  }
  else if(mBoard.buttons().justPressed(hardware::Button::Left))
  {
    jogOuterLimit(-outerLimitJogSteps());
    tActivity = true;
  }
  else if(mCenterButtonAction == CenterButtonAction::ShortPress)
  {
    //--------------------------------------------------------------------
    // Save the selected outer limit and return the shuttle to logical
    // position zero.  The rewinder will enter Ready after the return
    // completes.  Starting the winding operation requires a separate
    // center-button press from Ready.
    //--------------------------------------------------------------------
    beginOuterLimitReturn(false);
    tActivity = true;
  }

  if(tActivity)
  {
    mLastOuterLimitActivityMs = mBoard.millis();
  }
  else
  {
    // uint16_t subtraction deliberately provides correct elapsed time across
    // the wrapping millisecond clock.  The 60-second timeout fits within one
    // complete 16-bit interval.
    const uint16_t tElapsed = static_cast<uint16_t>(mBoard.millis() - mLastOuterLimitActivityMs);
    if(tElapsed >= config::OUTER_LIMIT_TIMEOUT_MS)
    {
      beginOuterLimitReturn(false);
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the shuttle state while returning the outer limit to home.
 */
//--------------------------------------------------------------------
void Rewinder::updateOuterLimitReturnHome()
{
  // Check if the shuttle has completed its move to the home position.
  if(mBoard.shuttle().moveComplete())
  {
    // Position zero is already the target of the bounded move.  Explicitly
    // restore the normal outward direction so winding always starts by moving
    // away from the inner edge.
    mShuttleMovingOutward = true;
    mBoard.shuttle().setDirection(true);

    // Transition to the next state based on whether we should start winding immediately.
    if(mStartAfterOuterLimitReturn)
    {
      if(mBoard.finda().filamentPresent())
      {
        startWinding();
      }
      else
      {
        enterState(RewinderState::OutOfFilament);
      }
    }
    else
    {
      enterState(RewinderState::Ready);
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Jog the outer limit by a specified number of steps.
 *
 * @param aDeltaSteps The number of steps to jog the outer limit.
 */
//--------------------------------------------------------------------
void Rewinder::jogOuterLimit(int32_t aDeltaSteps)
{
  int32_t tNewLimit = mOuterLimitSteps + aDeltaSteps;
  const int32_t tMinimum = minimumOuterLimitSteps();
  const int32_t tMaximum = maximumOuterLimitSteps();

  // Clamp the requested limit to configured safe bounds.  This prevents a
  // long sequence of Left presses from collapsing the usable winding width,
  // and prevents Right presses from driving beyond the known shuttle range.
  if(tNewLimit < tMinimum)
  {
    tNewLimit = tMinimum;
  }
  else if(tNewLimit > tMaximum)
  {
    tNewLimit = tMaximum;
  }

  mOuterLimitSteps = tNewLimit;
  mBoard.shuttle().moveTo(mOuterLimitSteps, outerLimitAdjustStepsPerSecond());
}

//--------------------------------------------------------------------
/**
 * @brief Begin the process of returning the outer limit to the home position.
 *
 * @param aStartAfterReturn Whether to start winding immediately after returning.
 */
//--------------------------------------------------------------------
void Rewinder::beginOuterLimitReturn(bool aStartAfterReturn)
{
  // Save once when the adjustment session ends.  eeprom_update_word() avoids a
  // physical EEPROM write when the selected value has not actually changed.
  saveOuterLimit();

  mStartAfterOuterLimitReturn = aStartAfterReturn;
  mOuterLimitPhase = OuterLimitPhase::ReturningHome;
  mBoard.shuttle().setEnabled(true);
  mBoard.shuttle().moveTo(0L, outerLimitAdjustStepsPerSecond());
}

//--------------------------------------------------------------------
/**
 * @brief Request to start winding from the ready state.
 */
//--------------------------------------------------------------------
void Rewinder::requestStartFromReady()
{
  // Ready is entered after homing or after an outer-limit adjustment has
  // returned to logical zero.  A normal start therefore begins from the
  // current position without forcing another return-to-zero move.
  if(mBoard.finda().filamentPresent())
  {
    startWinding();
  }
  else
  {
    enterState(RewinderState::OutOfFilament);
  }
}

//--------------------------------------------------------------------
/**
 * @brief Start the winding process.
 */
//--------------------------------------------------------------------
void Rewinder::startWinding()
{
  mBoard.takeup().setEnabled(true);
  mBoard.shuttle().setEnabled(true);
  setBrake(true);
  applyWindingRates();
  enterState(RewinderState::Winding);
}

//--------------------------------------------------------------------
/**
 * @brief Stop the winding process.
 */
//--------------------------------------------------------------------
void Rewinder::stopWinding()
{
  mBoard.takeup().stop();
  mBoard.shuttle().stop();
  mBoard.takeup().setEnabled(false);
  setBrake(false);
}

//--------------------------------------------------------------------
/**
 * @brief Calculate the take-up motor steps per second based on the current speed level.
 *
 * @return The take-up motor steps per second.
 */
//--------------------------------------------------------------------
float Rewinder::takeupStepsPerSecond() const
{
  const float tRpm = config::TAKEUP_RPM[mSpeedLevel];
  const float tStepsPerSecond = tRpm * config::TAKEUP_MOTOR_STEPS_PER_REVOLUTION / 60.0F;
  return tStepsPerSecond;
}

//--------------------------------------------------------------------
/**
 * @brief Calculate the shuttle motor steps per second based on the current take-up rate.
 *
 * @return The shuttle motor steps per second.
 */
//--------------------------------------------------------------------
float Rewinder::shuttleStepsPerSecond() const
{
  // One take-up spool revolution should move the shuttle by approximately one
  // winding pitch.  Deriving shuttle rate from spool revolutions preserves the
  // filament spacing when the operator changes the overall winding speed.
  const float tRevolutionsPerSecond = takeupStepsPerSecond() / config::TAKEUP_MOTOR_STEPS_PER_REVOLUTION;
  const float tStepsPerSecond = tRevolutionsPerSecond * config::WINDING_PITCH_MM * config::SHUTTLE_STEPS_PER_MM;
  return tStepsPerSecond;
}

//--------------------------------------------------------------------
/**
 * @brief Apply the current winding rates to the take-up and shuttle motors.
 */
//--------------------------------------------------------------------
void Rewinder::applyWindingRates()
{
  mBoard.takeup().setDirection(config::TAKEUP_WINDING_DIRECTION_POSITIVE);
  mBoard.takeup().setRate(takeupStepsPerSecond());
  mBoard.shuttle().setDirection(mShuttleMovingOutward);
  mBoard.shuttle().setRate(shuttleStepsPerSecond());
}

//--------------------------------------------------------------------
/**
 * @brief Get the default outer limit in shuttle steps.
 *
 * @return The default outer limit in shuttle steps.
 */
//--------------------------------------------------------------------
int32_t Rewinder::defaultOuterLimitSteps() const
{
  const float tUsableMm = config::SPOOL_WINDING_WIDTH_MM - config::SHUTTLE_EDGE_MARGIN_MM;
  const int32_t tSteps = static_cast<int32_t>((tUsableMm * config::SHUTTLE_STEPS_PER_MM) + 0.5F);
  return tSteps;
}

//--------------------------------------------------------------------
/**
 * @brief Get the minimum outer limit in shuttle steps.
 *
 * @return The minimum outer limit in shuttle steps.
 */
//--------------------------------------------------------------------
int32_t Rewinder::minimumOuterLimitSteps() const
{
  const float tUsableMm = config::MINIMUM_SPOOL_WINDING_WIDTH_MM - config::SHUTTLE_EDGE_MARGIN_MM;
  const int32_t tSteps = static_cast<int32_t>((tUsableMm * config::SHUTTLE_STEPS_PER_MM) + 0.5F);
  return tSteps;
}

//--------------------------------------------------------------------
/**
 * @brief Get the maximum outer limit in shuttle steps.
 *
 * @return The maximum outer limit in shuttle steps.
 */
//--------------------------------------------------------------------
int32_t Rewinder::maximumOuterLimitSteps() const
{
  const float tUsableMm = config::MAXIMUM_SPOOL_WINDING_WIDTH_MM - config::SHUTTLE_EDGE_MARGIN_MM;
  const int32_t tSteps = static_cast<int32_t>((tUsableMm * config::SHUTTLE_STEPS_PER_MM) + 0.5F);
  return tSteps;
}

//--------------------------------------------------------------------
/**
 * @brief Get the outer limit jog steps in shuttle steps.
 *
 * @return The outer limit jog steps in shuttle steps.
 */
//--------------------------------------------------------------------
int32_t Rewinder::outerLimitJogSteps() const
{
  const int32_t tSteps = static_cast<int32_t>((config::OUTER_LIMIT_JOG_MM * config::SHUTTLE_STEPS_PER_MM) + 0.5F);
  return tSteps;
}

//--------------------------------------------------------------------
/**
 * @brief Get the outer limit adjust speed in shuttle steps per second.
 *
 * @return The outer limit adjust speed in shuttle steps per second.
 */
//--------------------------------------------------------------------
float Rewinder::outerLimitAdjustStepsPerSecond() const
{
  const float tStepsPerSecond = config::OUTER_LIMIT_ADJUST_SPEED_MM_PER_SEC * config::SHUTTLE_STEPS_PER_MM;
  return tStepsPerSecond;
}

//--------------------------------------------------------------------
/**
 * @brief Load the outer limit from EEPROM.
 */
//--------------------------------------------------------------------
void Rewinder::loadOuterLimit()
{
  const uint16_t tMagic       = eeprom_read_word(&gOuterLimitMagic);
  const uint16_t tStoredSteps = eeprom_read_word(&gOuterLimitSteps);
  const int32_t tCandidate    = static_cast<int32_t>(tStoredSteps);

  if((tMagic == config::OUTER_LIMIT_EEPROM_MAGIC) &&
     (tCandidate >= minimumOuterLimitSteps()) &&
     (tCandidate <= maximumOuterLimitSteps()))
  {
    mOuterLimitSteps = tCandidate;
  }
  else
  {
    mOuterLimitSteps = defaultOuterLimitSteps();
  }
}

//--------------------------------------------------------------------
/**
 * @brief Save the outer limit to EEPROM.
 */
//--------------------------------------------------------------------
void Rewinder::saveOuterLimit() const
{
  eeprom_update_word(&gOuterLimitSteps, static_cast<uint16_t>(mOuterLimitSteps));
  eeprom_update_word(&gOuterLimitMagic, config::OUTER_LIMIT_EEPROM_MAGIC);
}

//--------------------------------------------------------------------
/**
 * @brief Update the shuttle direction when it reaches the inner or outer limits.
 */
//--------------------------------------------------------------------
void Rewinder::updateShuttleDirectionAtEdges()
{
  // The operator-selected outer limit replaces the former fixed spool-width
  // calculation.  Position zero remains the backed-off inner edge established
  // during homing.
  const int32_t tPosition = mBoard.shuttle().position();

  // Check if the shuttle has reached the outer limit.
  if(mShuttleMovingOutward && (tPosition >= mOuterLimitSteps))
  {
    mShuttleMovingOutward = false;
    mBoard.shuttle().setDirection(false);
  }
  // Check if the shuttle has reached the inner limit.
  else if((!mShuttleMovingOutward) && (tPosition <= 0L))
  {
    mShuttleMovingOutward = true;
    mBoard.shuttle().setDirection(true);
  }
}

//--------------------------------------------------------------------
/**
 * @brief Enable or disable the brake.
 *
 * @param aEnabled True to enable the brake, false to disable it.
 */
//--------------------------------------------------------------------
void Rewinder::setBrake(bool aEnabled)
{
  // The brake motor is held stationary.  TMC2130 current produces magnetic
  // holding torque, which creates drag on the supply spool.  No brake steps
  // are generated in this initial control strategy.
  if(aEnabled)
  {
    mBoard.brake().setCurrent(config::BRAKE_RUN_CURRENT, config::BRAKE_HOLD_CURRENT);
    mBoard.brake().setEnabled(true);
    mBoard.brake().stop();
  }
  else
  {
    mBoard.brake().setEnabled(false);
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the winding process.
 */
//--------------------------------------------------------------------
void Rewinder::updateWinding()
{
  bool tContinueProcessing = true;

  // Check if filament is present.
  if(!mBoard.finda().filamentPresent())
  {
    stopWinding();
    enterState(RewinderState::OutOfFilament);
    tContinueProcessing = false;
  }

  // A short center-button press pauses winding without changing the logical
  // shuttle position or the remembered traverse direction.
  if(tContinueProcessing && (mCenterButtonAction == CenterButtonAction::ShortPress))
  {
    stopWinding();
    enterState(RewinderState::Paused);
    tContinueProcessing = false;
  }

  // Check if the left button was just pressed to decrease the speed level.
  if(tContinueProcessing && mBoard.buttons().justPressed(hardware::Button::Left))
  {
    if(mSpeedLevel > 0U)
    {
      --mSpeedLevel;
      applyWindingRates();
    }
  }

  // Check if the right button was just pressed to increase the speed level.
  if(tContinueProcessing && mBoard.buttons().justPressed(hardware::Button::Right))
  {
    if((mSpeedLevel + 1U) < config::SPEED_LEVEL_COUNT)
    {
      ++mSpeedLevel;
      applyWindingRates();
    }
  }

  // Update the shuttle direction if still processing.
  if(tContinueProcessing)
  {
    updateShuttleDirectionAtEdges();
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the out-of-filament state.
 *
 * @details Winding remains stopped while filament is absent.  Once filament
 * has been restored, a short press of the center button immediately
 * resumes winding from the current shuttle position and in the
 * previously selected shuttle direction.
 */
//--------------------------------------------------------------------
void Rewinder::updateOutOfFilament()
{
  stopWinding();

  if(mBoard.finda().filamentPresent() &&
     (mCenterButtonAction == CenterButtonAction::ShortPress))
  {
    startWinding();
  }
}

//--------------------------------------------------------------------
/**
 * @brief Update the error state.
 */
//--------------------------------------------------------------------
void Rewinder::updateError()
{
  stopWinding();
}

//--------------------------------------------------------------------
/**
 * @brief Handle a failure and enter the error state.
 */
//--------------------------------------------------------------------
void Rewinder::fail(ErrorCode aError)
{
  mError = aError;
  stopWinding();
  mBoard.shuttle().stop();
  enterState(RewinderState::Error);
}

//--------------------------------------------------------------------
/**
 * @brief Update the rewinder state machine.
 */
//--------------------------------------------------------------------
void Rewinder::update()
{
  mBoard.updateInputs();

  // Decode the center button once per application update.  All state handlers
  // consume the same gesture result, preventing one physical press from being
  // interpreted differently by multiple states during a transition.
  mCenterButtonAction = updateCenterButtonGesture();

  // A five-second center-button hold is a global re-home command during normal
  // operation.  Error and active homing states are excluded so a held button
  // cannot repeatedly restart homing or bypass a fatal driver error.
  const bool tRehomeRequested =
    (mCenterButtonAction == CenterButtonAction::LongPress) &&
    (mState != RewinderState::Boot) &&
    (mState != RewinderState::Homing) &&
    (mState != RewinderState::HomeBackoff) &&
    (mState != RewinderState::Error);

  if(tRehomeRequested)
  {
#ifdef SPOOLER_DEBUG
    hardware::Debug::printLine("Center button held 5 seconds -> rehoming shuttle.");
#endif
    startHoming();
  }
  else
  {
    // Process the current state of the rewinder.
    switch(mState)
    {
      case RewinderState::Boot:                                           break;
      case RewinderState::Homing:           updateHoming();               break;
      case RewinderState::HomeBackoff:      updateHomeBackoff();          break;
      case RewinderState::Ready:            updateReady();                break;
      case RewinderState::AdjustOuterLimit: updateOuterLimitAdjustment(); break;
      case RewinderState::Winding:          updateWinding();              break;
      case RewinderState::Paused:           updatePaused();               break;
      case RewinderState::OutOfFilament:    updateOutOfFilament();        break;
      case RewinderState::Error:            updateError();                break;
    }
  }

  // Update the LEDs based on the current state and speed level.
  mBoard.leds().update(mBoard.millis(), mState, mSpeedLevel);

  #ifdef SPOOLER_DEBUG
  printDebugStatus();
  #endif
}

//--------------------------------------------------------------------
/**
 * @brief Get the current state of the rewinder.
 */
//--------------------------------------------------------------------
RewinderState Rewinder::state() const
{
  const RewinderState tState = mState;
  return tState;
}

//--------------------------------------------------------------------
/**
 * @brief Get the current error code of the rewinder.
 */
//--------------------------------------------------------------------
ErrorCode Rewinder::error() const
{
  const ErrorCode tError = mError;
  return tError;
}

//--------------------------------------------------------------------
/**
 * @brief Get the current speed level of the rewinder.
 */
//--------------------------------------------------------------------
uint8_t Rewinder::speedLevel() const
{
  const uint8_t tSpeedLevel = mSpeedLevel;
  return tSpeedLevel;
}

#ifdef SPOOLER_DEBUG

//--------------------------------------------------------------------
/**
 * @brief Prints the current rewinder state and status when they change.
 *
 * @details
 * The method retains the values that were printed during the previous
 * call.  If none of the monitored values have changed, nothing is
 * transmitted.  This allows the method to be called during every
 * Rewinder::update() cycle without continuously flooding the diagnostic
 * UART.
 *
 * The following values are monitored:
 *
 *   - Application state.
 *   - Fatal error code.
 *   - Winding speed level.
 *   - FINDA filament-present status.
 */
//--------------------------------------------------------------------
void Rewinder::printDebugStatus()
{
  static bool tFirstCall = true;
  static RewinderState tLastState = RewinderState::Boot;
  static ErrorCode tLastError = ErrorCode::None;
  static uint8_t tLastSpeedLevel = 0U;
  static bool tLastFilamentPresent = false;

  const bool tFilamentPresent =
    mBoard.finda().filamentPresent();

  //--------------------------------------------------------------------
  // Return immediately if none of the monitored values changed since
  // the previous diagnostic message.
  //--------------------------------------------------------------------
  if(!tFirstCall &&
     (mState == tLastState) &&
     (mError == tLastError) &&
     (mSpeedLevel == tLastSpeedLevel) &&
     (tFilamentPresent == tLastFilamentPresent))
  {
    return;
  }

  tFirstCall = false;
  tLastState = mState;
  tLastError = mError;
  tLastSpeedLevel = mSpeedLevel;
  tLastFilamentPresent = tFilamentPresent;

  //--------------------------------------------------------------------
  // Print the application state.
  //--------------------------------------------------------------------
  hardware::Debug::print("State=");

  switch(mState)
  {
    case RewinderState::Boot:
      hardware::Debug::print("Boot");
      break;

    case RewinderState::Homing:
      hardware::Debug::print("Homing");
      break;

    case RewinderState::HomeBackoff:
      hardware::Debug::print("HomeBackoff");
      break;

    case RewinderState::Ready:
      hardware::Debug::print("Ready");
      break;

    case RewinderState::AdjustOuterLimit:
      hardware::Debug::print("AdjustOuterLimit");
      break;

    case RewinderState::Winding:
      hardware::Debug::print("Winding");
      break;

    case RewinderState::Paused:
      hardware::Debug::print("Paused");
      break;

    case RewinderState::OutOfFilament:
      hardware::Debug::print("OutOfFilament");
      break;

    case RewinderState::Error:
      hardware::Debug::print("Error");
      break;
  }

  //--------------------------------------------------------------------
  // Print the active error code.
  //--------------------------------------------------------------------
  hardware::Debug::print(" Error=");

  switch(mError)
  {
    case ErrorCode::None:
      hardware::Debug::print("None");
      break;

    case ErrorCode::ShuttleHomeTimeout:
      hardware::Debug::print("ShuttleHomeTimeout");
      break;

    case ErrorCode::ShuttleUnexpectedStall:
      hardware::Debug::print("ShuttleUnexpectedStall");
      break;

    case ErrorCode::DriverInitialization:
      hardware::Debug::print("DriverInitialization");
      break;
  }

  //--------------------------------------------------------------------
  // Print the operator-selected winding speed.
  //--------------------------------------------------------------------
  hardware::Debug::print(" Speed=");
  hardware::Debug::print(
    static_cast<uint32_t>(mSpeedLevel));

  //--------------------------------------------------------------------
  // Print the current FINDA status.
  //--------------------------------------------------------------------
  hardware::Debug::print(" Filament=");
  hardware::Debug::printLine(
    tFilamentPresent ? "Present" : "Absent");
}

#endif

} // namespace spooler
//--------------------------------------------------------------------
