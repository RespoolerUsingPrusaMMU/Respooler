//--------------------------------------------------------------------
/**
 * @file Board.cc
 * @brief Implements the rewinder abstraction for the reused Prusa MMU controller board.
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
// Global includes
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

// Local includes
#include "hardware/Board.hh"
#include "config/Defaults.hh"
#include "hardware/MmuHal.hh"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace spooler
{
//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace hardware
{
// Global millisecond counter. It can count up to about 65.535 seconds before wrapping around.
volatile uint16_t gMilliseconds = 0U;

//--------------------------------------------------------------------
/**
 * @brief Constructs the Board object and initializes motor parameters.
 */
//--------------------------------------------------------------------
Board::Board()
  : mShuttleParams(
      { hal::spi::TmcSpiBus,            ///< The SPI bus used for the shuttle driver.
        0U,                             ///< The chip select index for the shuttle driver.
        config::SHUTTLE_DRIVER_DIR_ON,  ///< The direction control for the shuttle driver.
        PULLEY_CS_PIN,                  ///< The chip select pin for the shuttle driver.
        PULLEY_STEP_PIN,                ///< The step pin for the shuttle driver.
        PULLEY_SG_PIN,                  ///< The stall guard pin for the shuttle driver.
        MicrostepResolution::MRes8,     ///< The microstep resolution for the shuttle driver.
        8 }),                           ///< The current microstep setting for the shuttle driver.
    mTakeupParams(
      { hal::spi::TmcSpiBus,            ///< The SPI bus used for the take up driver.
        1U,                             ///< The chip select index for the take up driver.
        config::TAKEUP_DRIVER_DIR_ON,   ///< The direction control for the take up driver.
        SELECTOR_CS_PIN,                ///< The chip select pin for the take up driver.
        SELECTOR_STEP_PIN,              ///< The step pin for the take up driver.
        SELECTOR_SG_PIN,                ///< The stall guard pin for the take up driver.
        MicrostepResolution::MRes8,     ///< The microstep resolution for the take up driver.
        3 }),                           ///< The current microstep setting for the take up driver.
    mBrakeParams(
      { hal::spi::TmcSpiBus,            ///< The SPI bus used for the brake driver.
        2U,                             ///< The chip select index for the brake driver.
        config::BRAKE_DRIVER_DIR_ON,    ///< The direction control for the brake driver.
        IDLER_CS_PIN,                   ///< The chip select pin for the brake driver.
        IDLER_STEP_PIN,                 ///< The step pin for the brake driver.
        IDLER_SG_PIN,                   ///< The stall guard pin for the brake driver.
        MicrostepResolution::MRes16,    ///< The microstep resolution for the brake driver.
        7 }),                           ///< The current microstep setting for the brake driver.
    mShuttle(mShuttleDriver, mShuttleParams),
    mTakeup(mTakeupDriver,   mTakeupParams),
    mBrake(mBrakeDriver,     mBrakeParams),
    mMotion(mShuttle,        mTakeup, mBrake)
{
}

//--------------------------------------------------------------------
/**
 * @brief Initialize the board hardware.
 *
 * This function initializes the necessary hardware components for the board,
 * including the SPI bus, GPIO pins, and motor drivers. It also sets up the
 * millisecond timer and motion scheduler.
 *
 * @return true if all drivers are initialized successfully, false otherwise.
 */
bool Board::init()
{
  // Initialize the original MMU support hardware before configuring the motor
  // drivers.  Motor direction/enable outputs share the 16-bit shift register
  // with the front-panel LEDs.
  hal::shr16::shr16.Init();
  hal::adc::Init();

  // Initialize the FINDA pin as an input with a pull-up resistor.
  hal::gpio::Init(FINDA_PIN, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::input,hal::gpio::Pull::up));

  // Initialize the SPI bus for the TMC2130 motor drivers.
  hal::spi::SPI_InitTypeDef tSpi =
  {
    TMC2130_SPI_MISO_PIN,    ///< The MISO pin for the SPI bus.
    TMC2130_SPI_MOSI_PIN,    ///< The MOSI pin for the SPI bus.
    TMC2130_SPI_SCK_PIN,     ///< The SCK pin for the SPI bus.
    TMC2130_SPI_SS_PIN,      ///< The SS pin for the SPI bus.
    2U,                      ///< The SPI bus number.
    1U,                      ///< The SPI bus mode.
    1U                       ///< The SPI bus clock divider.
  };

  // Initialize the SPI bus with the specified configuration.
  hal::spi::Init(hal::spi::TmcSpiBus, &tSpi);

  // Initialize the shuttle, take up, and brake motor drivers with their respective parameters.
  const bool tShuttleOk = mShuttleDriver.init(mShuttleParams, Tmc2130Currents(13U, 2U));
  const bool tTakeupOk  = mTakeupDriver.init(mTakeupParams,   Tmc2130Currents(20U, 4U));
  const bool tBrakeOk   = mBrakeDriver.init(mBrakeParams,     Tmc2130Currents(config::BRAKE_RUN_CURRENT,config::BRAKE_HOLD_CURRENT));

  // Enable the shuttle motor and disable the take up and brake motors initially.
  mShuttle.setEnabled(true);
  mTakeup.setEnabled(false);
  mBrake.setEnabled(false);

  // Initialize the millisecond timer and motion scheduler.
  initMillisecondTimer();
  mMotion.initScheduler();

  // The application is allowed to home only when all three driver channels
  // initialized correctly.  A partial initialization is treated as fatal.
  const bool tDriversOk = tShuttleOk && tTakeupOk && tBrakeOk;
  return tDriversOk;
}

//--------------------------------------------------------------------
/**
 * @brief Initialize the millisecond timer using Timer0.
 *
 * This function configures Timer0 in CTC mode with a prescaler of 64 and
 * sets the compare match value to generate an interrupt every 1 millisecond.
 */
//--------------------------------------------------------------------
void Board::initMillisecondTimer()
{
  TCCR0A = static_cast<uint8_t>(1U << WGM01);
  TCCR0B = static_cast<uint8_t>((1U << CS01) | (1U << CS00));
  OCR0A = 249U;
  TCNT0 = 0U;
  TIMSK0 |= static_cast<uint8_t>(1U << OCIE0A);
}

//--------------------------------------------------------------------
/**
 * @brief Get the current millisecond count.
 *
 * This function returns the number of milliseconds elapsed since the board
 * was initialized. It reads the global millisecond counter atomically to
 * ensure a consistent value.
 *
 * The counter wont rollover for approximately 65.536 seconds.
 *
 * @note This function is safe to call from both main code and interrupt context.
 * @return The current millisecond count.
 */
//--------------------------------------------------------------------
uint16_t Board::millis() const
{
  // gMilliseconds is 16 bits on an 8-bit AVR, so copy it atomically to avoid
  // returning a value assembled from bytes on opposite sides of an ISR update.
  uint16_t tValue = 0U;

  // Enter an atomic block to read the millisecond counter safely.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    tValue = gMilliseconds;
  }

  return tValue;
}

//--------------------------------------------------------------------
/**
 * @brief Update the state of all input devices.
 *
 * This function updates the state of buttons and the FINDA sensor.
 * It uses a single timestamp for all updates to ensure consistent
 * debounce logic.
 */
//--------------------------------------------------------------------
void Board::updateInputs()
{
  // Use one timestamp for all input updates so button and FINDA debounce logic
  // observe a consistent application time during this service iteration.
  const uint16_t tNow = millis();
  mButtons.update(tNow);
  mFinda.update(tNow);
}

//--------------------------------------------------------------------
/**
 * @brief Get the buttons interface.
 *
 * @return Reference to the buttons interface.
 */
//--------------------------------------------------------------------
Buttons &Board::buttons()
{
  return mButtons;
}

//--------------------------------------------------------------------
/**
 * @brief Get the FINDA sensor interface.
 *
 * @return Reference to the FINDA sensor interface.
 */
//--------------------------------------------------------------------
Finda &Board::finda()
{
  return mFinda;
}

//--------------------------------------------------------------------
/**
 * @brief Get the LEDs interface.
 *
 * @return Reference to the LEDs interface.
 */
//--------------------------------------------------------------------
Leds &Board::leds()
{
  return mLeds;
}

//--------------------------------------------------------------------
/**
 * @brief Get the shuttle stepper axis interface.
 *
 * @return Reference to the shuttle stepper axis interface.
 */
//--------------------------------------------------------------------
motion::StepperAxis &Board::shuttle()
{
  return mShuttle;
}

//--------------------------------------------------------------------
/**
 * @brief Get the takeup stepper axis interface.
 *
 * @return Reference to the takeup stepper axis interface.
 */
//--------------------------------------------------------------------
motion::StepperAxis &Board::takeup()
{
  return mTakeup;
}

//--------------------------------------------------------------------
/**
 * @brief Get the brake stepper axis interface.
 *
 * @return Reference to the brake stepper axis interface.
 */
//--------------------------------------------------------------------
motion::StepperAxis &Board::brake()
{
  return mBrake;
}
//--------------------------------------------------------------------
/**
 * @brief Get the motion controller interface.
 *
 * @return Reference to the motion controller interface.
 */
//--------------------------------------------------------------------
motion::MotionController &Board::motion()
{
  return mMotion;
}

} // namespace hardware
} // namespace spooler

//--------------------------------------------------------------------
/** 
 * @brief Timer0 Compare Match A interrupt service routine.
 *
 * This ISR is called when Timer0 reaches the value in OCR0A.
 * It increments the global millisecond counter.
 * @note This ISR should be as short and fast as possible to avoid delaying other interrupts.
 *
 * @warning Modifying the global millisecond counter from other parts of the code should be done 
 * with caution to prevent race conditions.
 *
 * The global millisecond counter is incremented in this ISR and can count up to 
 * 49 days before overflowing.
 *
 * The sequence that gets to this method is:
 * ATmega32U4 Timer0
 *      |
 *      | timer counts
 *      v
 * OCR0A compare match occurs
 *      |
 *      | hardware raises TIMER0_COMPA interrupt
 *      v
 * AVR interrupt controller
 *      |
 *      | jumps automatically to interrupt vector
 *      v
 * ISR(TIMER0_COMPA_vect)
 *      |
 *      v
 * ++gMilliseconds
 *
 * The ISR Macro comes from comes from AVR Libc. It declares the function with the special 
 * name and attributes needed so the linker puts its address into the Timer0 Compare Match 
 * A interrupt vector.
 */
//--------------------------------------------------------------------
ISR(TIMER0_COMPA_vect)
{
  ++spooler::hardware::gMilliseconds;
}
//--------------------------------------------------------------------
