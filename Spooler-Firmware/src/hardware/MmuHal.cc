//--------------------------------------------------------------------
/**
 * @file MmuHal.cc
 * @brief Implements the rewinder-local MMU board hardware abstraction.
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
 */
//--------------------------------------------------------------------
// Global includes

// Local includes
#include "hardware/MmuHal.hh"
#include <util/atomic.h>
#include <util/delay.h>

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace hal
{
//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace gpio
{

//--------------------------------------------------------------------
/**
 * @brief Write a logical level to a GPIO pin.
 *
 * @param aPin The GPIO pin to write to.
 * @param aLevel The logical level to set on the pin.
 */
//--------------------------------------------------------------------
void WritePin(GPIO_pin aPin, Level aLevel)
{
  // Perform the write operation atomically to avoid race conditions.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    if(aLevel == Level::high)
    {
      aPin.port->PORTx |= aPin.pin_mask;
    }
    else
    {
      aPin.port->PORTx &= static_cast<uint8_t>(~aPin.pin_mask);
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Read the logical level from a GPIO pin.
 *
 * @param aPin The GPIO pin to read from.
 * @return The logical level of the pin.
 */
//--------------------------------------------------------------------
Level ReadPin(GPIO_pin aPin)
{
  const bool tHigh = (aPin.port->PINx & aPin.pin_mask) != 0U;
  return tHigh ? Level::high : Level::low;
}

//--------------------------------------------------------------------
/**
 * @brief Toggle the logical level of a GPIO pin.
 *
 * @param aPin The GPIO pin to toggle.
 */
//--------------------------------------------------------------------
void TogglePin(GPIO_pin aPin)
{
  // On AVR, writing a one to PINx toggles the corresponding PORTx output bit.
  aPin.port->PINx = aPin.pin_mask;
}

//--------------------------------------------------------------------
/**
 * @brief Initialize a GPIO pin with the specified configuration.
 *
 * @param aPin The GPIO pin to initialize.
 * @param aConfiguration The configuration settings for the pin.
 */
//--------------------------------------------------------------------
void Init(GPIO_pin aPin, GPIO_InitTypeDef aConfiguration)
{
  if(aConfiguration.mode == Mode::output)
  {
    WritePin(aPin, aConfiguration.level);
    aPin.port->DDRx |= aPin.pin_mask;
  }
  else
  {
    aPin.port->DDRx &= static_cast<uint8_t>(~aPin.pin_mask);
    WritePin(aPin,
             (aConfiguration.pull == Pull::up) ? Level::high : Level::low);
  }
}

} // namespace gpio
//--------------------------------------------------------------------

//--------------------------------------------------------------------
/**
 * @brief SPI bus interface.
 */
//--------------------------------------------------------------------
namespace spi
{

volatile SPI_TypeDef *const TmcSpiBus = reinterpret_cast<volatile SPI_TypeDef *>(&SPCR);

//--------------------------------------------------------------------
/**
 * @brief Initialize the SPI bus with the specified configuration.
 *
 * @param aSpi The SPI peripheral to initialize.
 * @param aConfiguration The configuration settings for the SPI bus.
 */
//--------------------------------------------------------------------
void Init(volatile SPI_TypeDef *const aSpi, SPI_InitTypeDef *const aConfiguration)
{
  gpio::Init(aConfiguration->miso_pin, gpio::GPIO_InitTypeDef(gpio::Mode::input,  gpio::Pull::none));
  gpio::Init(aConfiguration->mosi_pin, gpio::GPIO_InitTypeDef(gpio::Mode::output, gpio::Level::low));
  gpio::Init(aConfiguration->sck_pin,  gpio::GPIO_InitTypeDef(gpio::Mode::output, gpio::Level::low));
  gpio::Init(aConfiguration->ss_pin,   gpio::GPIO_InitTypeDef(gpio::Mode::output, gpio::Level::high));

  // Calculate SPI prescaler values based on the configuration.
  const uint8_t tSpi2x = (aConfiguration->prescaler == 7U) ? 0U : static_cast<uint8_t>(aConfiguration->prescaler & 0x01U);
  const uint8_t tSpr = static_cast<uint8_t>(((aConfiguration->prescaler - 1U) >> 1U) & 0x03U);

  // Configure SPI control registers based on the calculated prescaler values.
  aSpi->SPCRx = static_cast<uint8_t>(
    (1U << SPE) |
    (1U << MSTR) |
    ((aConfiguration->cpol & 0x01U) << CPOL) |
    ((aConfiguration->cpha & 0x01U) << CPHA) |
    (tSpr << SPR0));
  aSpi->SPSRx = static_cast<uint8_t>(tSpi2x << SPI2X);
}

//--------------------------------------------------------------------
/**
 * @brief Transmit and receive a byte over the SPI bus.
 *
 * @param aSpi The SPI peripheral to use for communication.
 * @param aValue The byte to transmit.
 * @return The received byte.
 */
//--------------------------------------------------------------------
uint8_t TxRx(volatile SPI_TypeDef *aSpi, uint8_t aValue)
{
  aSpi->SPDRx = aValue;
  while((aSpi->SPSRx & static_cast<uint8_t>(1U << SPIF)) == 0U)
  {
  }
  return aSpi->SPDRx;
}

} // namespace spi
//--------------------------------------------------------------------

//--------------------------------------------------------------------
//--------------------------------------------------------------------
namespace adc
{

//--------------------------------------------------------------------
/**
 * @brief Initialize the ADC peripheral.
 */
//--------------------------------------------------------------------
void Init()
{
  ADCSRA |= static_cast<uint8_t>((1U << ADPS2) | (1U << ADPS1) | (1U << ADPS0));
  ADMUX  |= static_cast<uint8_t>(1U << REFS0);
  ADCSRA |= static_cast<uint8_t>(1U << ADEN);
}

//--------------------------------------------------------------------
/**
 * @brief Read a value from the specified ADC channel.
 *
 * @param aChannel The ADC channel to read from.
 * @return The ADC conversion result.
 */
//--------------------------------------------------------------------
uint16_t ReadADC(uint8_t aChannel)
{
  // Select the ADC channel by configuring the ADMUX and ADCSRB registers.
  uint8_t tAdmux = ADMUX;
  tAdmux &= static_cast<uint8_t>(~0x1FU);
  tAdmux |= static_cast<uint8_t>(aChannel & 0x1FU);
  ADMUX = tAdmux;

  // Configure the ADCSRB register for the selected ADC channel.
  uint8_t tAdcsrb = ADCSRB;
  tAdcsrb &= static_cast<uint8_t>(~(1U << MUX5));

  // Set the MUX5 bit in ADCSRB if the channel number requires it.
  if((aChannel & 0x20U) != 0U)
  {
    tAdcsrb |= static_cast<uint8_t>(1U << MUX5);
  }
  // Write the updated ADCSRB value back to the register.
  ADCSRB = tAdcsrb;

  // Start the ADC conversion.
  ADCSRA |= static_cast<uint8_t>(1U << ADSC);

  // Wait for the ADC conversion to complete.
  while((ADCSRA & static_cast<uint8_t>(1U << ADSC)) != 0U)
  {
  }

  return ADC;
}

} // namespace adc
//--------------------------------------------------------------------

//--------------------------------------------------------------------
/**
 * @brief SHR16 namespace for controlling the 16-bit shift register.
 *
 * @details The SHR16 is the pair of cascaded 74HC595 shift registers 
 *          used to create 16 digital output bits from the ATmega32U4. 
 *          Each bit controls either a TMC2130 motor-driver signal or 
 *          one of the MMU LEDs.
 *
 * Bit:  15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 *        |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
 *        ------------------------------------------------
 *        LED outputs              Motor control outputs
 *
 *  Bit	Hex Mask	Function
 *   0	0x0001	  Pulley motor direction
 *   1	0x0002	  Pulley motor enable
 *   2	0x0004	  Selector motor direction
 *   3	0x0008	  Selector motor enable
 *   4	0x0010	  Idler motor direction
 *   5	0x0020	  Idler motor enable
 *   6	0x0040	  Slot 0 green LED
 *   7	0x0080	  Slot 0 red LED
 *   8	0x0100	  Slot 4 green LED
 *   9	0x0200	  Slot 4 red LED
 *   10	0x0400	  Slot 3 green LED
 *   11	0x0800	  Slot 3 red LED
 *   12	0x1000	  Slot 2 green LED
 *   13	0x2000	  Slot 2 red LED
 *   14	0x4000	  Slot 1 green LED
 *   15	0x8000	  Slot 1 red LED
 *
 *    15       14       13       12       11       10        9        8
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * | Slot1  | Slot1  | Slot2  | Slot2  | Slot3  | Slot3  | Slot4  | Slot4  |
 * |  RED   | GREEN  |  RED   | GREEN  |  RED   | GREEN  |  RED   | GREEN  |
 * +--------+--------+--------+--------+--------+--------+--------+--------+

 *     7        6        5        4        3        2        1        0
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * | Slot0  | Slot0  | Brake  | Brake  |Take-up |Take-up |Shuttle |Shuttle |
 * |  RED   | GREEN  | ENABLE |  DIR   | ENABLE |  DIR   | ENABLE |  DIR   |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 */
//--------------------------------------------------------------------
namespace shr16
{

//--------------------------------------------------------------------
/**
 * @brief Private namespace for internal constants used by the SHR16 class.
 */
//--------------------------------------------------------------------
namespace
{
constexpr uint16_t LED_MASK = 0xFFC0U;
constexpr uint16_t ENABLE_MASK = 0x002AU;
}

// Instance of the SHR16 class.
// This class is responsible for controlling the 16-bit shift register used for LEDs and TMC drivers.
// The instance is used to interact with the shift register throughout the firmware.
// This is a global instance of the SHR16 class.
SHR16 shr16;

//--------------------------------------------------------------------
/**
 * @brief Initialize the SHR16 shift register.
 */
//--------------------------------------------------------------------
void SHR16::Init()
{
  gpio::Init(SHR16_DATA,  gpio::GPIO_InitTypeDef(gpio::Mode::output, gpio::Level::low));
  gpio::Init(SHR16_LATCH, gpio::GPIO_InitTypeDef(gpio::Mode::output, gpio::Level::low));
  gpio::Init(SHR16_CLOCK, gpio::GPIO_InitTypeDef(gpio::Mode::output, gpio::Level::low));
  Write(ENABLE_MASK);
}

//--------------------------------------------------------------------
/**
 * @brief Write a value to the SHR16 shift register.
 * @param aValue The 16-bit value to write to the shift register.
 */
//--------------------------------------------------------------------
void SHR16::Write(uint16_t aValue)
{
  // Shift out each bit of the 16-bit value, starting with the most significant bit.
  for(uint16_t tMask = 0x8000U; tMask != 0U; tMask >>= 1U)
  {
    gpio::WritePin(SHR16_CLOCK, gpio::Level::low);
    gpio::WritePin(SHR16_DATA, ((aValue & tMask) != 0U) ? gpio::Level::high : gpio::Level::low);
    gpio::WritePin(SHR16_CLOCK, gpio::Level::high);
  }

  // Latch the shifted data into the output register.
  gpio::WritePin(SHR16_CLOCK, gpio::Level::low);
  gpio::WritePin(SHR16_LATCH, gpio::Level::high);

  // Ensure the latch pulse is long enough for the data to be registered.
  _delay_us(15);

  // Bring the latch pin low to complete the latch cycle.
  gpio::WritePin(SHR16_LATCH, gpio::Level::low);

  // Wait for the latch pulse to complete before proceeding.
  _delay_us(15);

  mValue = aValue;
}

//--------------------------------------------------------------------
/**
 * @brief Set the state of the LEDs.
 * @param aLedBits The logical LED bits to set.
 */
//--------------------------------------------------------------------
void SHR16::SetLED(uint16_t aLedBits)
{
  // Ensure atomic update of the shift register.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    // Logical LED bits 0..7 map to physical bits 8..15. Logical bits 8..9
    // map to physical bits 6..7. This is the original MMU PCB wiring.
    const uint16_t tEncoded = static_cast<uint16_t>( ((aLedBits & 0x00FFU) << 8U) | ((aLedBits & 0x0300U) >> 2U));
    Write(static_cast<uint16_t>((mValue & static_cast<uint16_t>(~LED_MASK)) | (tEncoded & LED_MASK)));
  }
}

//--------------------------------------------------------------------
/**
 * @brief Set the enabled state of a TMC (Trinamic Motor Controller) channel.
 * @param aIndex The index of the TMC channel.
 * @param aEnabled True to enable the channel, false to disable it.
 */
//--------------------------------------------------------------------
void SHR16::SetTMCEnabled(uint8_t aIndex, bool aEnabled)
{
  // Ensure atomic update of the shift register.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    const uint16_t tMask = static_cast<uint16_t>(1U << ((2U * aIndex) + 1U));
    if(aEnabled)
    {
      Write(static_cast<uint16_t>(mValue & static_cast<uint16_t>(~tMask)));
    }
    else
    {
      Write(static_cast<uint16_t>(mValue | tMask));
    }
  }
}

//--------------------------------------------------------------------
/**
 * @brief Set the direction of a TMC (Trinamic Motor Controller) channel.
 * @param aIndex The index of the TMC channel.
 * @param aDirection True for one direction, false for the opposite.
 */
//--------------------------------------------------------------------
void SHR16::SetTMCDir(uint8_t aIndex, bool aDirection)
{
  // Ensure atomic update of the shift register.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    const uint16_t tMask = static_cast<uint16_t>(1U << (2U * aIndex));
    if(aDirection)
    {
      Write(static_cast<uint16_t>(mValue & static_cast<uint16_t>(~tMask)));
    }
    else
    {
      Write(static_cast<uint16_t>(mValue | tMask));
    }
  }
}

} // namespace shr16
} // namespace hal
//--------------------------------------------------------------------

