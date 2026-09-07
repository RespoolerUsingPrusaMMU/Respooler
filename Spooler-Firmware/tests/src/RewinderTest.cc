//--------------------------------------------------------------------
/**
 * @file RewinderTest.cc
 * @brief GoogleTest coverage for the filament rewinder state machine.
 *
 * Copyright (C) 2026 Andre Pruitt
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
//--------------------------------------------------------------------
#include <gtest/gtest.h>

#include <cmath>
#include <stdint.h>

#include "avr/eeprom.h"
#include "hardware/Debug.hh"

//--------------------------------------------------------------------
// The production class intentionally keeps state-machine implementation
// helpers private.  Unit tests expose them in this translation unit only so
// each method can be tested directly without changing the production API.
//--------------------------------------------------------------------
#define private public
#include "app/Rewinder.hh"
#undef private

#include "config/Defaults.hh"

namespace
{

using spooler::ErrorCode;
using spooler::Rewinder;
using spooler::RewinderState;
using spooler::hardware::Board;
using spooler::hardware::Button;

//--------------------------------------------------------------------
/** @brief GoogleTest fixture providing deterministic board control. */
//--------------------------------------------------------------------
class RewinderTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    fake_avr::resetEeprom();
    spooler::hardware::Debug::clear();
    mBoard.finda().setFilamentPresent(true);
  }

  void homeToReady()
  {
    mRewinder.init(true);
    ASSERT_EQ(RewinderState::Homing, mRewinder.state());

    mBoard.shuttle().setStallActive(true);

    for(uint8_t tSample = 0U;
        tSample < spooler::config::SHUTTLE_STALL_SAMPLES_REQUIRED;
        ++tSample)
    {
      mRewinder.update();
    }

    ASSERT_EQ(RewinderState::HomeBackoff, mRewinder.state());

    mBoard.shuttle().completeMove();
    mRewinder.update();

    ASSERT_EQ(RewinderState::Ready, mRewinder.state());
  }

  void shortCenterPress(uint16_t aPressMs = 100U)
  {
    mBoard.buttons().setPressed(Button::Middle, true);
    mRewinder.update();

    mBoard.advanceMillis(aPressMs);
    mBoard.buttons().setPressed(Button::Middle, false);
    mRewinder.update();
  }

  void longCenterPress()
  {
    mBoard.buttons().setPressed(Button::Middle, true);
    mRewinder.update();

    mBoard.advanceMillis(spooler::config::CENTER_BUTTON_REHOME_HOLD_MS);
    mRewinder.update();
  }

  void pressArrow(Button aButton)
  {
    mBoard.buttons().setPressed(aButton, true);
    mRewinder.update();
    mBoard.buttons().setPressed(aButton, false);
  }

  void startWindingFromReady()
  {
    homeToReady();
    shortCenterPress();
    ASSERT_EQ(RewinderState::Winding, mRewinder.state());
  }

  Board mBoard;
  Rewinder mRewinder{mBoard};
};

//--------------------------------------------------------------------
// Construction, accessors, and initialization.
//--------------------------------------------------------------------
TEST_F(RewinderTest, ConstructorStartsInBootWithoutErrorAtDefaultSpeed)
{
  EXPECT_EQ(RewinderState::Boot, mRewinder.state());
  EXPECT_EQ(ErrorCode::None, mRewinder.error());
  EXPECT_EQ(spooler::config::DEFAULT_SPEED_LEVEL, mRewinder.speedLevel());
}

TEST_F(RewinderTest, InitWithValidDriversStartsHoming)
{
  mRewinder.init(true);

  EXPECT_EQ(RewinderState::Homing, mRewinder.state());
  EXPECT_TRUE(mBoard.shuttle().enabled());
  EXPECT_FALSE(mBoard.shuttle().directionPositive());
  EXPECT_GT(mBoard.shuttle().rate(), 0.0F);
}

TEST_F(RewinderTest, InitWithInvalidDriversEntersDriverInitializationError)
{
  mRewinder.init(false);

  EXPECT_EQ(RewinderState::Error, mRewinder.state());
  EXPECT_EQ(ErrorCode::DriverInitialization, mRewinder.error());
  EXPECT_FALSE(mBoard.takeup().enabled());
  EXPECT_FALSE(mBoard.brake().enabled());
}

TEST_F(RewinderTest, EnterStateRecordsStateAndEntryTime)
{
  mBoard.setMillis(1234U);
  mRewinder.enterState(RewinderState::Paused);

  EXPECT_EQ(RewinderState::Paused, mRewinder.state());
  EXPECT_EQ(1234U, mRewinder.mStateStartMs);
}

//--------------------------------------------------------------------
// Homing behavior and homing errors.
//--------------------------------------------------------------------
TEST_F(RewinderTest, HomingRequiresConsecutiveStallSamples)
{
  mRewinder.init(true);
  mBoard.shuttle().setStallActive(true);

  for(uint8_t tSample = 0U;
      tSample < static_cast<uint8_t>(spooler::config::SHUTTLE_STALL_SAMPLES_REQUIRED - 1U);
      ++tSample)
  {
    mRewinder.update();
    EXPECT_EQ(RewinderState::Homing, mRewinder.state());
  }

  mBoard.shuttle().setStallActive(false);
  mRewinder.update();
  EXPECT_EQ(0U, mRewinder.mHomingStallSamples);

  mBoard.shuttle().setStallActive(true);
  for(uint8_t tSample = 0U;
      tSample < spooler::config::SHUTTLE_STALL_SAMPLES_REQUIRED;
      ++tSample)
  {
    mRewinder.update();
  }

  EXPECT_EQ(RewinderState::HomeBackoff, mRewinder.state());
}

TEST_F(RewinderTest, HomingStallStartsBackoffMove)
{
  mRewinder.init(true);
  mBoard.shuttle().setStallActive(true);

  for(uint8_t tSample = 0U;
      tSample < spooler::config::SHUTTLE_STALL_SAMPLES_REQUIRED;
      ++tSample)
  {
    mRewinder.update();
  }

  const int32_t tExpectedBackoff = static_cast<int32_t>(
    (spooler::config::SHUTTLE_HOME_BACKOFF_MM *
     spooler::config::SHUTTLE_STEPS_PER_MM) + 0.5F);

  EXPECT_EQ(RewinderState::HomeBackoff, mRewinder.state());
  EXPECT_EQ(tExpectedBackoff, mBoard.shuttle().targetPosition());
  EXPECT_TRUE(mBoard.shuttle().directionPositive());
}

TEST_F(RewinderTest, HomeBackoffCompletionEstablishesLogicalZeroAndReady)
{
  mRewinder.startHoming();
  mRewinder.enterState(RewinderState::HomeBackoff);
  mBoard.shuttle().moveTo(100L, 100.0F);
  mBoard.shuttle().completeMove();

  mRewinder.updateHomeBackoff();

  EXPECT_EQ(0L, mBoard.shuttle().position());
  EXPECT_TRUE(mBoard.shuttle().directionPositive());
  EXPECT_TRUE(mRewinder.mShuttleMovingOutward);
  EXPECT_EQ(RewinderState::Ready, mRewinder.state());
}

TEST_F(RewinderTest, HomingTimeoutEntersFatalError)
{
  mRewinder.init(true);
  mBoard.shuttle().setStallActive(false);
  mBoard.advanceMillis(static_cast<uint16_t>(spooler::config::SHUTTLE_HOME_TIMEOUT_MS + 1U));

  mRewinder.update();

  EXPECT_EQ(RewinderState::Error, mRewinder.state());
  EXPECT_EQ(ErrorCode::ShuttleHomeTimeout, mRewinder.error());
}

//--------------------------------------------------------------------
// Center-button gesture decoding.
//--------------------------------------------------------------------
TEST_F(RewinderTest, CenterButtonShortPressIsReportedOnRelease)
{
  mBoard.buttons().setPressed(Button::Middle, true);
  EXPECT_EQ(Rewinder::CenterButtonAction::None,
            mRewinder.updateCenterButtonGesture());

  mBoard.advanceMillis(500U);
  EXPECT_EQ(Rewinder::CenterButtonAction::None,
            mRewinder.updateCenterButtonGesture());

  mBoard.buttons().setPressed(Button::Middle, false);
  EXPECT_EQ(Rewinder::CenterButtonAction::ShortPress,
            mRewinder.updateCenterButtonGesture());
}

TEST_F(RewinderTest, CenterButtonFiveSecondHoldReportsLongPressOnce)
{
  mBoard.buttons().setPressed(Button::Middle, true);
  EXPECT_EQ(Rewinder::CenterButtonAction::None,
            mRewinder.updateCenterButtonGesture());

  mBoard.advanceMillis(spooler::config::CENTER_BUTTON_REHOME_HOLD_MS);
  EXPECT_EQ(Rewinder::CenterButtonAction::LongPress,
            mRewinder.updateCenterButtonGesture());

  mBoard.advanceMillis(1000U);
  EXPECT_EQ(Rewinder::CenterButtonAction::None,
            mRewinder.updateCenterButtonGesture());

  mBoard.buttons().setPressed(Button::Middle, false);
  EXPECT_EQ(Rewinder::CenterButtonAction::None,
            mRewinder.updateCenterButtonGesture());
}

TEST_F(RewinderTest, LongCenterHoldFromWindingRehomesAndStopsWinding)
{
  startWindingFromReady();

  longCenterPress();

  EXPECT_EQ(RewinderState::Homing, mRewinder.state());
  EXPECT_FALSE(mBoard.takeup().enabled());
  EXPECT_FALSE(mBoard.brake().enabled());
  EXPECT_FALSE(mBoard.shuttle().directionPositive());
}

//--------------------------------------------------------------------
// Ready, winding, pause, and runout transitions.
//--------------------------------------------------------------------
TEST_F(RewinderTest, ShortCenterPressFromReadyStartsWinding)
{
  homeToReady();

  shortCenterPress();

  EXPECT_EQ(RewinderState::Winding, mRewinder.state());
  EXPECT_TRUE(mBoard.takeup().enabled());
  EXPECT_TRUE(mBoard.shuttle().enabled());
  EXPECT_TRUE(mBoard.brake().enabled());
}

TEST_F(RewinderTest, StartWithoutFilamentEntersOutOfFilament)
{
  homeToReady();
  mBoard.finda().setFilamentPresent(false);

  shortCenterPress();

  EXPECT_EQ(RewinderState::OutOfFilament, mRewinder.state());
}

TEST_F(RewinderTest, ShortCenterPressWhileWindingPausesAtCurrentPosition)
{
  startWindingFromReady();
  mBoard.shuttle().setPosition(321L);
  mRewinder.mShuttleMovingOutward = false;
  mBoard.shuttle().setDirection(false);

  shortCenterPress();

  EXPECT_EQ(RewinderState::Paused, mRewinder.state());
  EXPECT_EQ(321L, mBoard.shuttle().position());
  EXPECT_FALSE(mRewinder.mShuttleMovingOutward);
  EXPECT_FALSE(mBoard.takeup().enabled());
  EXPECT_FALSE(mBoard.brake().enabled());
}

TEST_F(RewinderTest, ShortCenterPressFromPausedResumesPositionAndDirection)
{
  startWindingFromReady();
  mBoard.shuttle().setPosition(222L);
  mRewinder.mShuttleMovingOutward = false;

  shortCenterPress();
  ASSERT_EQ(RewinderState::Paused, mRewinder.state());

  shortCenterPress();

  EXPECT_EQ(RewinderState::Winding, mRewinder.state());
  EXPECT_EQ(222L, mBoard.shuttle().position());
  EXPECT_FALSE(mBoard.shuttle().directionPositive());
}

TEST_F(RewinderTest, ResumeFromPausedWithoutFilamentEntersOutOfFilament)
{
  startWindingFromReady();
  shortCenterPress();
  ASSERT_EQ(RewinderState::Paused, mRewinder.state());

  mBoard.finda().setFilamentPresent(false);
  shortCenterPress();

  EXPECT_EQ(RewinderState::OutOfFilament, mRewinder.state());
}

TEST_F(RewinderTest, FilamentLossWhileWindingStopsAndEntersOutOfFilament)
{
  startWindingFromReady();
  mBoard.finda().setFilamentPresent(false);

  mRewinder.update();

  EXPECT_EQ(RewinderState::OutOfFilament, mRewinder.state());
  EXPECT_FALSE(mBoard.takeup().enabled());
  EXPECT_FALSE(mBoard.brake().enabled());
}

TEST_F(RewinderTest, RestoredFilamentAndCenterPressResumesWindingImmediately)
{
  startWindingFromReady();
  mBoard.shuttle().setPosition(444L);
  mRewinder.mShuttleMovingOutward = false;
  mBoard.finda().setFilamentPresent(false);
  mRewinder.update();
  ASSERT_EQ(RewinderState::OutOfFilament, mRewinder.state());

  mBoard.finda().setFilamentPresent(true);
  shortCenterPress();

  EXPECT_EQ(RewinderState::Winding, mRewinder.state());
  EXPECT_EQ(444L, mBoard.shuttle().position());
  EXPECT_FALSE(mBoard.shuttle().directionPositive());
}

//--------------------------------------------------------------------
// Speed and winding-rate behavior.
//--------------------------------------------------------------------
TEST_F(RewinderTest, LeftAndRightButtonsAdjustSpeedWithinConfiguredBounds)
{
  startWindingFromReady();

  for(uint8_t tCount = 0U; tCount < 10U; ++tCount)
  {
    pressArrow(Button::Left);
  }
  EXPECT_EQ(0U, mRewinder.speedLevel());

  for(uint8_t tCount = 0U; tCount < 10U; ++tCount)
  {
    pressArrow(Button::Right);
  }
  EXPECT_EQ(static_cast<uint8_t>(spooler::config::SPEED_LEVEL_COUNT - 1U),
            mRewinder.speedLevel());
}

TEST_F(RewinderTest, TakeupStepsPerSecondMatchesConfiguredRpm)
{
  mRewinder.mSpeedLevel = 2U;

  const float tExpected =
    spooler::config::TAKEUP_RPM[2U] *
    spooler::config::TAKEUP_MOTOR_STEPS_PER_REVOLUTION / 60.0F;

  EXPECT_FLOAT_EQ(tExpected, mRewinder.takeupStepsPerSecond());
}

TEST_F(RewinderTest, ShuttleRateTracksTakeupRevolutionsAndPitch)
{
  const float tTakeup = mRewinder.takeupStepsPerSecond();
  const float tExpected =
    (tTakeup / spooler::config::TAKEUP_MOTOR_STEPS_PER_REVOLUTION) *
    spooler::config::WINDING_PITCH_MM *
    spooler::config::SHUTTLE_STEPS_PER_MM;

  EXPECT_FLOAT_EQ(tExpected, mRewinder.shuttleStepsPerSecond());
}

TEST_F(RewinderTest, ApplyWindingRatesCommandsTakeupAndShuttle)
{
  mRewinder.mShuttleMovingOutward = false;
  mRewinder.applyWindingRates();

  EXPECT_EQ(spooler::config::TAKEUP_WINDING_DIRECTION_POSITIVE,
            mBoard.takeup().directionPositive());
  EXPECT_FLOAT_EQ(mRewinder.takeupStepsPerSecond(), mBoard.takeup().rate());
  EXPECT_FALSE(mBoard.shuttle().directionPositive());
  EXPECT_FLOAT_EQ(mRewinder.shuttleStepsPerSecond(), mBoard.shuttle().rate());
}

//--------------------------------------------------------------------
// Shuttle edge reversal.
//--------------------------------------------------------------------
TEST_F(RewinderTest, ShuttleReversesInwardAtOuterLimit)
{
  mRewinder.mOuterLimitSteps = 500L;
  mRewinder.mShuttleMovingOutward = true;
  mBoard.shuttle().setPosition(500L);

  mRewinder.updateShuttleDirectionAtEdges();

  EXPECT_FALSE(mRewinder.mShuttleMovingOutward);
  EXPECT_FALSE(mBoard.shuttle().directionPositive());
}

TEST_F(RewinderTest, ShuttleReversesOutwardAtInnerLimit)
{
  mRewinder.mOuterLimitSteps = 500L;
  mRewinder.mShuttleMovingOutward = false;
  mBoard.shuttle().setPosition(0L);

  mRewinder.updateShuttleDirectionAtEdges();

  EXPECT_TRUE(mRewinder.mShuttleMovingOutward);
  EXPECT_TRUE(mBoard.shuttle().directionPositive());
}

//--------------------------------------------------------------------
// Outer-limit geometry, persistence, and state transitions.
//--------------------------------------------------------------------
TEST_F(RewinderTest, OuterLimitGeometryMethodsMatchConfiguration)
{
  const int32_t tDefault = static_cast<int32_t>(
    ((spooler::config::SPOOL_WINDING_WIDTH_MM -
      spooler::config::SHUTTLE_EDGE_MARGIN_MM) *
     spooler::config::SHUTTLE_STEPS_PER_MM) + 0.5F);

  const int32_t tMinimum = static_cast<int32_t>(
    ((spooler::config::MINIMUM_SPOOL_WINDING_WIDTH_MM -
      spooler::config::SHUTTLE_EDGE_MARGIN_MM) *
     spooler::config::SHUTTLE_STEPS_PER_MM) + 0.5F);

  const int32_t tMaximum = static_cast<int32_t>(
    ((spooler::config::MAXIMUM_SPOOL_WINDING_WIDTH_MM -
      spooler::config::SHUTTLE_EDGE_MARGIN_MM) *
     spooler::config::SHUTTLE_STEPS_PER_MM) + 0.5F);

  const int32_t tJog = static_cast<int32_t>(
    (spooler::config::OUTER_LIMIT_JOG_MM *
     spooler::config::SHUTTLE_STEPS_PER_MM) + 0.5F);

  EXPECT_EQ(tDefault, mRewinder.defaultOuterLimitSteps());
  EXPECT_EQ(tMinimum, mRewinder.minimumOuterLimitSteps());
  EXPECT_EQ(tMaximum, mRewinder.maximumOuterLimitSteps());
  EXPECT_EQ(tJog, mRewinder.outerLimitJogSteps());
  EXPECT_FLOAT_EQ(spooler::config::OUTER_LIMIT_ADJUST_SPEED_MM_PER_SEC *
                    spooler::config::SHUTTLE_STEPS_PER_MM,
                  mRewinder.outerLimitAdjustStepsPerSecond());
}

TEST_F(RewinderTest, InvalidEepromLoadsConfiguredDefaultOuterLimit)
{
  mRewinder.loadOuterLimit();

  EXPECT_EQ(mRewinder.defaultOuterLimitSteps(), mRewinder.mOuterLimitSteps);
}

TEST_F(RewinderTest, SavedOuterLimitLoadsIntoNewRewinder)
{
  mRewinder.mOuterLimitSteps = mRewinder.minimumOuterLimitSteps() + 25L;
  const int32_t tSaved = mRewinder.mOuterLimitSteps;
  mRewinder.saveOuterLimit();

  Board tOtherBoard;
  Rewinder tOtherRewinder(tOtherBoard);
  tOtherRewinder.loadOuterLimit();

  EXPECT_EQ(tSaved, tOtherRewinder.mOuterLimitSteps);
}

TEST_F(RewinderTest, ArrowFromReadyMovesToSavedLimitAndEntersAdjustment)
{
  homeToReady();
  const int32_t tSavedLimit = mRewinder.mOuterLimitSteps;

  pressArrow(Button::Left);

  EXPECT_EQ(RewinderState::AdjustOuterLimit, mRewinder.state());
  EXPECT_EQ(Rewinder::OuterLimitPhase::MovingToSavedLimit,
            mRewinder.mOuterLimitPhase);
  EXPECT_EQ(tSavedLimit, mBoard.shuttle().targetPosition());
}

TEST_F(RewinderTest, ArrivalAtSavedLimitEntersJoggingPhase)
{
  homeToReady();
  pressArrow(Button::Left);
  ASSERT_EQ(RewinderState::AdjustOuterLimit, mRewinder.state());

  mBoard.shuttle().completeMove();
  mRewinder.update();

  EXPECT_EQ(Rewinder::OuterLimitPhase::Adjusting,
            mRewinder.mOuterLimitPhase);
}

TEST_F(RewinderTest, ShortCenterWhileMovingToSavedLimitReturnsHomeThenReady)
{
  homeToReady();
  pressArrow(Button::Left);
  ASSERT_EQ(Rewinder::OuterLimitPhase::MovingToSavedLimit,
            mRewinder.mOuterLimitPhase);

  shortCenterPress();

  EXPECT_EQ(Rewinder::OuterLimitPhase::ReturningHome,
            mRewinder.mOuterLimitPhase);
  EXPECT_FALSE(mRewinder.mStartAfterOuterLimitReturn);
  EXPECT_EQ(0L, mBoard.shuttle().targetPosition());

  mBoard.shuttle().completeMove();
  mRewinder.update();

  EXPECT_EQ(RewinderState::Ready, mRewinder.state());
}

TEST_F(RewinderTest, LeftAndRightJogCommandsChangeOuterLimitByOneIncrement)
{
  homeToReady();
  pressArrow(Button::Left);
  mBoard.shuttle().completeMove();
  mRewinder.update();
  ASSERT_EQ(Rewinder::OuterLimitPhase::Adjusting,
            mRewinder.mOuterLimitPhase);

  const int32_t tOriginal = mRewinder.mOuterLimitSteps;
  const int32_t tJog = mRewinder.outerLimitJogSteps();

  pressArrow(Button::Left);
  EXPECT_EQ(tOriginal - tJog, mRewinder.mOuterLimitSteps);

  mBoard.shuttle().completeMove();
  pressArrow(Button::Right);
  EXPECT_EQ(tOriginal, mRewinder.mOuterLimitSteps);
}

TEST_F(RewinderTest, JogOuterLimitClampsToMinimumAndMaximum)
{
  mRewinder.mOuterLimitSteps = mRewinder.minimumOuterLimitSteps();
  mRewinder.jogOuterLimit(-100000L);
  EXPECT_EQ(mRewinder.minimumOuterLimitSteps(), mRewinder.mOuterLimitSteps);

  mRewinder.mOuterLimitSteps = mRewinder.maximumOuterLimitSteps();
  mRewinder.jogOuterLimit(100000L);
  EXPECT_EQ(mRewinder.maximumOuterLimitSteps(), mRewinder.mOuterLimitSteps);
}

TEST_F(RewinderTest, ShortCenterDuringOuterLimitAdjustmentReturnsHomeThenReady)
{
  homeToReady();
  pressArrow(Button::Left);
  mBoard.shuttle().completeMove();
  mRewinder.update();
  ASSERT_EQ(Rewinder::OuterLimitPhase::Adjusting,
            mRewinder.mOuterLimitPhase);

  shortCenterPress();

  ASSERT_EQ(RewinderState::AdjustOuterLimit, mRewinder.state());
  EXPECT_EQ(Rewinder::OuterLimitPhase::ReturningHome,
            mRewinder.mOuterLimitPhase);
  EXPECT_EQ(0L, mBoard.shuttle().targetPosition());
  EXPECT_FALSE(mRewinder.mStartAfterOuterLimitReturn);

  mBoard.shuttle().completeMove();
  mRewinder.update();

  EXPECT_EQ(RewinderState::Ready, mRewinder.state());
}

TEST_F(RewinderTest, OuterLimitTimeoutReturnsHomeWithoutStartingWinding)
{
  homeToReady();
  pressArrow(Button::Right);
  mBoard.shuttle().completeMove();
  mRewinder.update();
  ASSERT_EQ(Rewinder::OuterLimitPhase::Adjusting,
            mRewinder.mOuterLimitPhase);

  mBoard.advanceMillis(spooler::config::OUTER_LIMIT_TIMEOUT_MS);
  mRewinder.update();

  EXPECT_EQ(Rewinder::OuterLimitPhase::ReturningHome,
            mRewinder.mOuterLimitPhase);
  EXPECT_FALSE(mRewinder.mStartAfterOuterLimitReturn);
}

TEST_F(RewinderTest, ExplicitReturnWithStartFlagCanStillStartWinding)
{
  homeToReady();
  mRewinder.enterState(RewinderState::AdjustOuterLimit);
  mRewinder.beginOuterLimitReturn(true);
  mBoard.shuttle().completeMove();

  mRewinder.updateOuterLimitReturnHome();

  EXPECT_EQ(RewinderState::Winding, mRewinder.state());
}

//--------------------------------------------------------------------
// Brake, stop, and error behavior.
//--------------------------------------------------------------------
TEST_F(RewinderTest, SetBrakeEnablesConfiguredHoldingCurrent)
{
  mRewinder.setBrake(true);

  EXPECT_TRUE(mBoard.brake().enabled());
  EXPECT_EQ(spooler::config::BRAKE_RUN_CURRENT, mBoard.brake().runCurrent());
  EXPECT_EQ(spooler::config::BRAKE_HOLD_CURRENT, mBoard.brake().holdCurrent());

  mRewinder.setBrake(false);
  EXPECT_FALSE(mBoard.brake().enabled());
}

TEST_F(RewinderTest, StopWindingStopsTakeupShuttleAndBrake)
{
  mRewinder.startWinding();
  ASSERT_TRUE(mBoard.takeup().enabled());
  ASSERT_TRUE(mBoard.brake().enabled());

  mRewinder.stopWinding();

  EXPECT_FALSE(mBoard.takeup().enabled());
  EXPECT_FALSE(mBoard.brake().enabled());
  EXPECT_FLOAT_EQ(0.0F, mBoard.takeup().rate());
  EXPECT_FLOAT_EQ(0.0F, mBoard.shuttle().rate());
}

TEST_F(RewinderTest, ReservedUnexpectedStallErrorCanBeLatchedSafely)
{
  mRewinder.startWinding();

  mRewinder.fail(ErrorCode::ShuttleUnexpectedStall);

  EXPECT_EQ(RewinderState::Error, mRewinder.state());
  EXPECT_EQ(ErrorCode::ShuttleUnexpectedStall, mRewinder.error());
  EXPECT_FALSE(mBoard.takeup().enabled());
  EXPECT_FALSE(mBoard.brake().enabled());
  EXPECT_FLOAT_EQ(0.0F, mBoard.shuttle().rate());
}

TEST_F(RewinderTest, ErrorStateContinuesToKeepWindingStopped)
{
  mRewinder.fail(ErrorCode::ShuttleUnexpectedStall);
  mBoard.takeup().setEnabled(true);
  mBoard.brake().setEnabled(true);

  mRewinder.updateError();

  EXPECT_FALSE(mBoard.takeup().enabled());
  EXPECT_FALSE(mBoard.brake().enabled());
}

//--------------------------------------------------------------------
// Top-level update and debug behavior.
//--------------------------------------------------------------------
TEST_F(RewinderTest, UpdateServicesInputsAndLedsEveryIteration)
{
  mRewinder.init(true);
  const uint32_t tInputsBefore = mBoard.updateInputsCount();
  const uint32_t tLedsBefore = mBoard.leds().updateCount();

  mRewinder.update();

  EXPECT_EQ(tInputsBefore + 1U, mBoard.updateInputsCount());
  EXPECT_EQ(tLedsBefore + 1U, mBoard.leds().updateCount());
  EXPECT_EQ(mRewinder.state(), mBoard.leds().lastState());
  EXPECT_EQ(mRewinder.speedLevel(), mBoard.leds().lastSpeedLevel());
}

TEST_F(RewinderTest, DebugStatusContainsCurrentStateErrorSpeedAndFilament)
{
  // Force one state change first so this test does not depend on the static
  // history retained by printDebugStatus() from previously executed tests.
  mRewinder.enterState(RewinderState::Error);
  mRewinder.printDebugStatus();

  spooler::hardware::Debug::clear();
  mRewinder.enterState(RewinderState::Paused);
  mBoard.finda().setFilamentPresent(true);
  mRewinder.printDebugStatus();

  const std::string &tText = spooler::hardware::Debug::text();
  EXPECT_NE(std::string::npos, tText.find("State=Paused"));
  EXPECT_NE(std::string::npos, tText.find("Error=None"));
  EXPECT_NE(std::string::npos, tText.find("Filament=Present"));
}

} // namespace
