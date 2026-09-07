//--------------------------------------------------------------------
/**
 * @file Tmc2130.cc
 * @brief Implements the rewinder-local TMC2130 motor driver wrapper.
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
 * This file implements the rewinder-local TMC2130 motor driver wrapper.
 * The Tmc2130Currents class encapsulates the current settings for the 
 * TMC2130 motor driver.
 *
 * @author Andre Pruitt (andre@pruittfamily.com)
 * @date 2026-09-04
 */
//--------------------------------------------------------------------
// Global includes

// Local includes
#include "hardware/Tmc2130.hh"
#include "hardware/MmuHal.hh"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace spooler
{
//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace hardware
{

//--------------------------------------------------------------------
// This name space contains internal constants for the TMC2130 driver.
//--------------------------------------------------------------------
namespace
{
constexpr uint8_t  TOFF_DEFAULT    =   3U;
constexpr uint32_t TCOOL_THRESHOLD = 450UL;
constexpr uint32_t PWM_AMPLITUDE   = 240UL;
constexpr uint32_t PWM_GRADIENT    =   4UL;
constexpr uint32_t PWM_FREQUENCY   =   0UL;
constexpr uint32_t PWM_AUTOSCALE   =   1UL;
constexpr uint32_t PWM_FREEWHEEL   =   1UL;
} // End anonymous namespace

//--------------------------------------------------------------------
/**
 * @brief Construct a new Tmc2130Currents object.
 * @param aRunCurrent The run current setting for the motor.
 * @param aHoldCurrent The hold current setting for the motor.
 */
//--------------------------------------------------------------------
Tmc2130Currents::Tmc2130Currents(uint8_t aRunCurrent,
                                 uint8_t aHoldCurrent)
  : mVSense(aRunCurrent < 32U),
    mRunCurrent((aRunCurrent < 32U) ? aRunCurrent : (aRunCurrent >> 1U)),
    mHoldCurrent((aRunCurrent < 32U) ? aHoldCurrent : (aHoldCurrent >> 1U))
{
}

//--------------------------------------------------------------------
/**
 * @brief Construct a new Tmc2130 object.
 */
//--------------------------------------------------------------------
 Tmc2130::Tmc2130()
  : mEnabled(false)
{
}

//--------------------------------------------------------------------
/**
 * @brief Initialize the TMC2130 driver with the specified parameters and currents.
 * @param aParams The configuration parameters for the TMC2130 driver.
 * @param aCurrents The current settings for the TMC2130 driver.
 * @return true if the driver was successfully initialized, false otherwise.
 */
//--------------------------------------------------------------------
bool Tmc2130::init(const Tmc2130Params &aParams,
                   const Tmc2130Currents &aCurrents)
{
  // Initialize the chip select pin as an output and set it high.
  hal::gpio::Init(aParams.mChipSelectPin, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output, hal::gpio::Level::high));

  // Initialize the stall guard pin as an input with a pull-up resistor.
  hal::gpio::Init( aParams.mStallGuardPin,hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::input,hal::gpio::Pull::up));

  // Initialize the step pin as an output and set it low.
  hal::gpio::Init(aParams.mStepPin,hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output,hal::gpio::Level::low));

  // Verify that the attached device identifies itself as a compatible TMC2130.
  // Bits 31:24 contain the version value and bit 6 is expected to be high on
  // the driver variant used by the Prusa MMU controller.
  const uint32_t tIoin = readRegister(aParams, Register::Ioin);
  bool tInitialized = (((tIoin >> 24U) == 0x11U) && ((tIoin & (1UL << 6U)) != 0UL));

  // Return false if the device did not initialize correctly.
  if(tInitialized)
  {
    // Reading GSTAT clears reset/status flags before configuration begins.
    (void)readRegister(aParams, Register::Gstat);

    // Configure the chopper exactly as required for the MMU step interface.
    // DEDGE allows each GPIO toggle to represent one motor step, which halves
    // the ISR work needed to generate the requested step frequency.
    uint32_t tChopConf = 0UL;
    tChopConf |= static_cast<uint32_t>(TOFF_DEFAULT & 0x0FU);
    tChopConf |= static_cast<uint32_t>(5U & 0x07U) << 4U;
    tChopConf |= static_cast<uint32_t>(1U & 0x0FU) << 7U;
    tChopConf |= static_cast<uint32_t>(2U & 0x03U) << 15U;
    tChopConf |= static_cast<uint32_t>(aCurrents.mVSense ? 1U : 0U) << 17U;
    tChopConf |= static_cast<uint32_t>(aParams.mMicrostepResolution) << 24U;
    tChopConf |= 1UL << 28U; // interpolation enabled
    tChopConf |= 1UL << 29U; // DEDGE enabled
    writeRegister(aParams, Register::ChopConf, tChopConf);

    // Set the motor currents.
    setCurrents(aParams, aCurrents);
    writeRegister(aParams, Register::TPowerDown, 0UL);

    // Configure the StallGuard threshold and cool step parameters.
    setStallGuardThreshold(aParams);
    writeRegister(aParams, Register::TCoolThrs, TCOOL_THRESHOLD);

    // Enable PWM mode and route StallGuard to the active-low DIAG0 output.
    const uint32_t tGconf = (1UL << 2U) | (1UL << 7U);
    writeRegister(aParams, Register::Gconf, tGconf);

    const uint32_t tPwmConf =
      (PWM_AMPLITUDE << 0U) |
      (PWM_GRADIENT << 8U) |
      (PWM_FREQUENCY << 16U) |
      (PWM_AUTOSCALE << 18U) |
      (PWM_FREEWHEEL << 20U);
    writeRegister(aParams, Register::PwmConf, tPwmConf);

    // The large threshold selects spreadCycle for essentially all motion.  This
    // is the same Normal-mode behavior used by the Prusa MMU during StallGuard
    // homing and keeps StallGuard available at the shuttle homing speed.
    // Set the PWM threshold to a large value to select spreadCycle for most motion.
    writeRegister(aParams, Register::TPwmThrs, 0xFFFF0UL);
  }

  return tInitialized;
}

//--------------------------------------------------------------------
/** 
 * @brief Enable or disable the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 * @param aEnabled True to enable the driver, false to disable it.
 */
//--------------------------------------------------------------------
void Tmc2130::setEnabled(const Tmc2130Params &aParams, bool aEnabled)
{
  hal::shr16::shr16.SetTMCEnabled(aParams.mShiftRegisterIndex, aEnabled);
  mEnabled = aEnabled;
}

//--------------------------------------------------------------------
/**
 * @brief Set the motor currents for the TMC2130 driver.
 *
 * @param aParams The TMC2130 parameters.
 * @param aCurrents The motor currents to set.
 */
//--------------------------------------------------------------------
void Tmc2130::setCurrents(const Tmc2130Params &aParams,
                          const Tmc2130Currents &aCurrents)
{
  const uint8_t tRunCurrent = aCurrents.mRunCurrent & 0x1FU;
  uint8_t tHoldCurrent = aCurrents.mHoldCurrent & 0x1FU;

  // Ensure the hold current does not exceed the run current.
  if(tHoldCurrent > tRunCurrent)
  {
    tHoldCurrent = tRunCurrent;
  }

  //
  const uint32_t tIHoldIRun =
    static_cast<uint32_t>(tHoldCurrent) |
    (static_cast<uint32_t>(tRunCurrent) << 8U) |
    (15UL << 16U);

  writeRegister(aParams, Register::IHoldIRun, tIHoldIRun);
}

//--------------------------------------------------------------------
/**
 * @brief Set the direction of the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 * @param aDirection The desired motor direction.
 */
//--------------------------------------------------------------------
void Tmc2130::setDirection(const Tmc2130Params &aParams, bool aDirection)
{
  hal::shr16::shr16.SetTMCDir(aParams.mShiftRegisterIndex, aDirection ^ aParams.mDirectionInverted);
}

//--------------------------------------------------------------------
/**
 * @brief Step the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 */
//--------------------------------------------------------------------
void Tmc2130::step(const Tmc2130Params &aParams)
{
  hal::gpio::TogglePin(aParams.mStepPin);
}

//--------------------------------------------------------------------
/**
 * @brief Check if the stall guard is active for the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 * @return true if the stall guard is active, false otherwise.
 */
//--------------------------------------------------------------------
bool Tmc2130::stallActive(const Tmc2130Params &aParams)
{
  return hal::gpio::ReadPin(aParams.mStallGuardPin) == hal::gpio::Level::low;
}

//--------------------------------------------------------------------
/**
 * @brief Set the stall guard threshold for the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 */
//--------------------------------------------------------------------
void Tmc2130::setStallGuardThreshold(const Tmc2130Params &aParams)
{
  // COOLCONF.sgt occupies bits 22:16.  Preserve the signed seven-bit pattern
  // by first converting through uint8_t and then masking to seven bits.
  const uint32_t tThreshold = static_cast<uint32_t>(static_cast<uint8_t>(aParams.mStallGuardThreshold)) & 0x7FUL;
  writeRegister(aParams, Register::CoolConf, tThreshold << 16U);
}

//--------------------------------------------------------------------
/**
 * @brief Read a register from the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 * @param aRegister The register to read.
 * @return The value of the register.
 */
//--------------------------------------------------------------------
uint32_t Tmc2130::readRegister(const Tmc2130Params &aParams,
                               Register aRegister)
{
  uint8_t tData[5] =
  {
    static_cast<uint8_t>(aRegister), 0U, 0U, 0U, 0U
  };

  // TMC2130 SPI reads are pipelined.  The first transaction requests the
  // register and the second transaction clocks the returned value out.
  transfer(aParams, tData);
  tData[0] = 0U;
  transfer(aParams, tData);

  // Combine the received bytes into a 32-bit value.
  return (static_cast<uint32_t>(tData[1]) << 24U) |
         (static_cast<uint32_t>(tData[2]) << 16U) |
         (static_cast<uint32_t>(tData[3]) << 8U) |
         static_cast<uint32_t>(tData[4]);
}

//--------------------------------------------------------------------
/**
 * @brief Write a value to a register of the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 * @param aRegister The register to write.
 * @param aValue The value to write to the register.
 */
//--------------------------------------------------------------------
void Tmc2130::writeRegister(const Tmc2130Params &aParams,
                            Register aRegister,
                            uint32_t aValue)
{
  // Prepare the data array for the SPI transfer. The first byte is the 
  // register address with the write flag set.
  uint8_t tData[5] =
  {
    static_cast<uint8_t>(static_cast<uint8_t>(aRegister) | 0x80U),
    static_cast<uint8_t>(aValue >> 24U),
    static_cast<uint8_t>(aValue >> 16U),
    static_cast<uint8_t>(aValue >> 8U),
    static_cast<uint8_t>(aValue)
  };

  transfer(aParams, tData);
}

//--------------------------------------------------------------------
/**
 * @brief Perform an SPI transfer with the TMC2130 motor driver.
 *
 * @param aParams The TMC2130 parameters.
 * @param aData The data array to send and receive.
 */
//--------------------------------------------------------------------
void Tmc2130::transfer(const Tmc2130Params &aParams, uint8_t (&aData)[5])
{
  // Begin the SPI transfer by setting the chip select pin low.
  hal::gpio::WritePin(aParams.mChipSelectPin, hal::gpio::Level::low);

  // Perform the SPI transfer for each byte in the data array.
  for(uint8_t tByte = 0U; tByte < 5U; ++tByte)
  {
    aData[tByte] = hal::spi::TxRx(aParams.mSpi, aData[tByte]);
  }

  // End the SPI transfer by setting the chip select pin high.
  hal::gpio::WritePin(aParams.mChipSelectPin, hal::gpio::Level::high);
}

} // namespace hardware
} // namespace spooler
//--------------------------------------------------------------------
