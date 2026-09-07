//--------------------------------------------------------------------
/**
 * @file Board.hh
 * @brief Host-side fake MMU board used by the Rewinder gtest suite.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
//--------------------------------------------------------------------
#ifndef PRUSA_SPOOLER_TEST_FAKE_BOARD_HH
#define PRUSA_SPOOLER_TEST_FAKE_BOARD_HH

#include <stdint.h>

#include "app/RewinderState.hh"

namespace spooler
{
namespace hardware
{

//--------------------------------------------------------------------
/** @brief Buttons available on the MMU front panel. */
//--------------------------------------------------------------------
enum class Button : uint8_t
{
  None,
  Left,
  Middle,
  Right
};

//--------------------------------------------------------------------
/** @brief Controllable host fake for the debounced button interface. */
//--------------------------------------------------------------------
class FakeButtons
{
public:
  FakeButtons()
    : mLeft(false),
      mMiddle(false),
      mRight(false),
      mLeftJustPressed(false),
      mMiddleJustPressed(false),
      mRightJustPressed(false)
  {
  }

  bool pressed(Button aButton) const
  {
    bool tPressed = false;

    switch(aButton)
    {
      case Button::Left:   tPressed = mLeft;   break;
      case Button::Middle: tPressed = mMiddle; break;
      case Button::Right:  tPressed = mRight;  break;
      case Button::None:   tPressed = false;   break;
    }

    return tPressed;
  }

  bool justPressed(Button aButton)
  {
    bool tPressed = false;

    switch(aButton)
    {
      case Button::Left:
        tPressed = mLeftJustPressed;
        mLeftJustPressed = false;
        break;

      case Button::Middle:
        tPressed = mMiddleJustPressed;
        mMiddleJustPressed = false;
        break;

      case Button::Right:
        tPressed = mRightJustPressed;
        mRightJustPressed = false;
        break;

      case Button::None:
        break;
    }

    return tPressed;
  }

  void setPressed(Button aButton, bool aPressed)
  {
    bool *tState = nullptr;
    bool *tJustPressed = nullptr;

    switch(aButton)
    {
      case Button::Left:
        tState = &mLeft;
        tJustPressed = &mLeftJustPressed;
        break;

      case Button::Middle:
        tState = &mMiddle;
        tJustPressed = &mMiddleJustPressed;
        break;

      case Button::Right:
        tState = &mRight;
        tJustPressed = &mRightJustPressed;
        break;

      case Button::None:
        break;
    }

    if(tState != nullptr)
    {
      if(aPressed && !(*tState))
      {
        *tJustPressed = true;
      }

      *tState = aPressed;
    }
  }

  void releaseAll()
  {
    mLeft = false;
    mMiddle = false;
    mRight = false;
  }

private:
  bool mLeft;
  bool mMiddle;
  bool mRight;
  bool mLeftJustPressed;
  bool mMiddleJustPressed;
  bool mRightJustPressed;
};

//--------------------------------------------------------------------
/** @brief Controllable host fake for the FINDA filament sensor. */
//--------------------------------------------------------------------
class FakeFinda
{
public:
  FakeFinda()
    : mFilamentPresent(true)
  {
  }

  bool filamentPresent() const
  {
    return mFilamentPresent;
  }

  void setFilamentPresent(bool aPresent)
  {
    mFilamentPresent = aPresent;
  }

private:
  bool mFilamentPresent;
};

//--------------------------------------------------------------------
/** @brief Records the LED state-machine arguments supplied by Rewinder. */
//--------------------------------------------------------------------
class FakeLeds
{
public:
  FakeLeds()
    : mUpdateCount(0U),
      mLastMs(0U),
      mLastState(RewinderState::Boot),
      mLastSpeedLevel(0U)
  {
  }

  void update(uint16_t aNowMs,
              RewinderState aState,
              uint8_t aSpeedLevel)
  {
    ++mUpdateCount;
    mLastMs = aNowMs;
    mLastState = aState;
    mLastSpeedLevel = aSpeedLevel;
  }

  uint32_t updateCount() const { return mUpdateCount; }
  uint16_t lastMs() const { return mLastMs; }
  RewinderState lastState() const { return mLastState; }
  uint8_t lastSpeedLevel() const { return mLastSpeedLevel; }

private:
  uint32_t mUpdateCount;
  uint16_t mLastMs;
  RewinderState mLastState;
  uint8_t mLastSpeedLevel;
};

//--------------------------------------------------------------------
/** @brief Host fake implementing the StepperAxis API used by Rewinder. */
//--------------------------------------------------------------------
class FakeAxis
{
public:
  FakeAxis()
    : mEnabled(false),
      mDirectionPositive(true),
      mRate(0.0F),
      mPosition(0L),
      mTargetPosition(0L),
      mMoveComplete(true),
      mStallActive(false),
      mRunCurrent(0U),
      mHoldCurrent(0U),
      mStopCount(0U),
      mMoveToCount(0U),
      mSetRateCount(0U)
  {
  }

  void setEnabled(bool aEnabled)
  {
    mEnabled = aEnabled;
  }

  bool enabled() const
  {
    return mEnabled;
  }

  void setCurrent(uint8_t aRunCurrent, uint8_t aHoldCurrent)
  {
    mRunCurrent = aRunCurrent;
    mHoldCurrent = aHoldCurrent;
  }

  void setDirection(bool aPositive)
  {
    mDirectionPositive = aPositive;
  }

  bool directionPositive() const
  {
    return mDirectionPositive;
  }

  void setRate(float aStepsPerSecond)
  {
    mRate = aStepsPerSecond;
    mMoveComplete = false;
    ++mSetRateCount;
  }

  float rate() const
  {
    return mRate;
  }

  void stop()
  {
    mRate = 0.0F;
    mMoveComplete = true;
    ++mStopCount;
  }

  void moveTo(int32_t aTargetPosition, float aStepsPerSecond)
  {
    mTargetPosition = aTargetPosition;
    mDirectionPositive = (aTargetPosition >= mPosition);
    mRate = aStepsPerSecond;
    mMoveComplete = false;
    ++mMoveToCount;
  }

  void moveBy(int32_t aDeltaSteps, float aStepsPerSecond)
  {
    moveTo(mPosition + aDeltaSteps, aStepsPerSecond);
  }

  bool moveComplete() const
  {
    return mMoveComplete;
  }

  void setPosition(int32_t aPosition)
  {
    mPosition = aPosition;
  }

  int32_t position() const
  {
    return mPosition;
  }

  bool stallActive() const
  {
    return mStallActive;
  }

  void setStallActive(bool aActive)
  {
    mStallActive = aActive;
  }

  void completeMove()
  {
    mPosition = mTargetPosition;
    mRate = 0.0F;
    mMoveComplete = true;
  }

  void setMoveComplete(bool aComplete)
  {
    mMoveComplete = aComplete;
  }

  int32_t targetPosition() const { return mTargetPosition; }
  uint8_t runCurrent() const { return mRunCurrent; }
  uint8_t holdCurrent() const { return mHoldCurrent; }
  uint32_t stopCount() const { return mStopCount; }
  uint32_t moveToCount() const { return mMoveToCount; }
  uint32_t setRateCount() const { return mSetRateCount; }

private:
  bool mEnabled;
  bool mDirectionPositive;
  float mRate;
  int32_t mPosition;
  int32_t mTargetPosition;
  bool mMoveComplete;
  bool mStallActive;
  uint8_t mRunCurrent;
  uint8_t mHoldCurrent;
  uint32_t mStopCount;
  uint32_t mMoveToCount;
  uint32_t mSetRateCount;
};

//--------------------------------------------------------------------
/** @brief Host-side Board replacement used only by unit tests. */
//--------------------------------------------------------------------
class Board
{
public:
  Board()
    : mMillis(0U),
      mUpdateInputsCount(0U)
  {
  }

  void updateInputs()
  {
    ++mUpdateInputsCount;
  }

  uint16_t millis() const
  {
    return mMillis;
  }

  void setMillis(uint16_t aMillis)
  {
    mMillis = aMillis;
  }

  void advanceMillis(uint16_t aDelta)
  {
    mMillis = static_cast<uint16_t>(mMillis + aDelta);
  }

  FakeButtons &buttons() { return mButtons; }
  const FakeButtons &buttons() const { return mButtons; }

  FakeFinda &finda() { return mFinda; }
  const FakeFinda &finda() const { return mFinda; }

  FakeLeds &leds() { return mLeds; }
  const FakeLeds &leds() const { return mLeds; }

  FakeAxis &shuttle() { return mShuttle; }
  const FakeAxis &shuttle() const { return mShuttle; }

  FakeAxis &takeup() { return mTakeup; }
  const FakeAxis &takeup() const { return mTakeup; }

  FakeAxis &brake() { return mBrake; }
  const FakeAxis &brake() const { return mBrake; }

  uint32_t updateInputsCount() const { return mUpdateInputsCount; }

private:
  uint16_t mMillis;
  uint32_t mUpdateInputsCount;
  FakeButtons mButtons;
  FakeFinda mFinda;
  FakeLeds mLeds;
  FakeAxis mShuttle;
  FakeAxis mTakeup;
  FakeAxis mBrake;
};

} // namespace hardware
} // namespace spooler

#endif // PRUSA_SPOOLER_TEST_FAKE_BOARD_HH
