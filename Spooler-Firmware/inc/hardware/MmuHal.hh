//--------------------------------------------------------------------
/**
 * @file MmuHal.hh
 * @brief Small, self-contained subset of the Prusa MMU hardware abstraction.
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
 * The rewinder uses the original MMU board but does not require the complete
 * Prusa firmware HAL.  This interface intentionally mirrors only the original
 * GPIO, SPI, ADC, shift-register, and pin APIs that the rewinder uses.  Keeping
 * those names makes the rewinder source easy to compare with the Prusa board
 * implementation while removing cross-project linker dependencies.
 */
//--------------------------------------------------------------------

#ifndef PRUSA_SPOOLER_MMU_HAL_HH
#define PRUSA_SPOOLER_MMU_HAL_HH

#include <stdint.h>
#include <avr/io.h>

namespace hal
{
namespace gpio
{

struct GPIO_TypeDef
{
  volatile uint8_t PINx;
  volatile uint8_t DDRx;
  volatile uint8_t PORTx;
};

enum class Mode : uint8_t
{
  input = 0U,
  output
};

enum class Pull : uint8_t
{
  none = 0U,
  up,
  down
};

enum class Level : uint8_t
{
  low = 0U,
  high
};

struct GPIO_InitTypeDef
{
  Mode mode;
  Pull pull;
  Level level;

  GPIO_InitTypeDef(Mode aMode, Pull aPull)
    : mode(aMode), pull(aPull), level(Level::low)
  {
  }

  GPIO_InitTypeDef(Mode aMode, Level aLevel)
    : mode(aMode), pull(Pull::none), level(aLevel)
  {
  }
};

struct GPIO_pin
{
  volatile GPIO_TypeDef *const port;
  const uint8_t pin_mask;
};

void Init(GPIO_pin aPin, GPIO_InitTypeDef aConfiguration);
void WritePin(GPIO_pin aPin, Level aLevel);
Level ReadPin(GPIO_pin aPin);
void TogglePin(GPIO_pin aPin);

} // namespace gpio

namespace spi
{

struct SPI_TypeDef
{
  volatile uint8_t SPCRx;
  volatile uint8_t SPSRx;
  volatile uint8_t SPDRx;
};

struct SPI_InitTypeDef
{
  gpio::GPIO_pin miso_pin;
  gpio::GPIO_pin mosi_pin;
  gpio::GPIO_pin sck_pin;
  gpio::GPIO_pin ss_pin;
  uint8_t prescaler;
  uint8_t cpha;
  uint8_t cpol;
};

extern volatile SPI_TypeDef *const TmcSpiBus;

void Init(volatile SPI_TypeDef *const aSpi, SPI_InitTypeDef *const aConfiguration);
uint8_t TxRx(volatile SPI_TypeDef *aSpi, uint8_t aValue);

} // namespace spi

namespace adc
{

void Init();
uint16_t ReadADC(uint8_t aChannel);

} // namespace adc

namespace shr16
{

class SHR16
{
public:
  void Init();
  void SetLED(uint16_t aLedBits);
  void SetTMCEnabled(uint8_t aIndex, bool aEnabled);
  void SetTMCDir(uint8_t aIndex, bool aDirection);

private:
  void Write(uint16_t aValue);
  uint16_t mValue = 0U;
};

extern SHR16 shr16;

} // namespace shr16
} // namespace hal

#define GPIOB_LOCAL (reinterpret_cast<volatile hal::gpio::GPIO_TypeDef *>(&PINB))
#define GPIOC_LOCAL (reinterpret_cast<volatile hal::gpio::GPIO_TypeDef *>(&PINC))
#define GPIOD_LOCAL (reinterpret_cast<volatile hal::gpio::GPIO_TypeDef *>(&PIND))
#define GPIOF_LOCAL (reinterpret_cast<volatile hal::gpio::GPIO_TypeDef *>(&PINF))

static const hal::gpio::GPIO_pin TMC2130_SPI_MISO_PIN = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 3U) };
static const hal::gpio::GPIO_pin TMC2130_SPI_MOSI_PIN = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 2U) };
static const hal::gpio::GPIO_pin TMC2130_SPI_SCK_PIN  = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 1U) };
static const hal::gpio::GPIO_pin TMC2130_SPI_SS_PIN   = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 0U) };

static const hal::gpio::GPIO_pin SHR16_DATA  = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 5U) };
static const hal::gpio::GPIO_pin SHR16_LATCH = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 6U) };
static const hal::gpio::GPIO_pin SHR16_CLOCK = { GPIOC_LOCAL, static_cast<uint8_t>(1U << 7U) };

static const hal::gpio::GPIO_pin PULLEY_CS_PIN   = { GPIOC_LOCAL, static_cast<uint8_t>(1U << 6U) };
static const hal::gpio::GPIO_pin PULLEY_SG_PIN   = { GPIOF_LOCAL, static_cast<uint8_t>(1U << 4U) };
static const hal::gpio::GPIO_pin PULLEY_STEP_PIN = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 4U) };

static const hal::gpio::GPIO_pin SELECTOR_CS_PIN   = { GPIOD_LOCAL, static_cast<uint8_t>(1U << 7U) };
static const hal::gpio::GPIO_pin SELECTOR_SG_PIN   = { GPIOF_LOCAL, static_cast<uint8_t>(1U << 1U) };
static const hal::gpio::GPIO_pin SELECTOR_STEP_PIN = { GPIOD_LOCAL, static_cast<uint8_t>(1U << 4U) };

static const hal::gpio::GPIO_pin IDLER_CS_PIN   = { GPIOB_LOCAL, static_cast<uint8_t>(1U << 7U) };
static const hal::gpio::GPIO_pin IDLER_SG_PIN   = { GPIOF_LOCAL, static_cast<uint8_t>(1U << 0U) };
static const hal::gpio::GPIO_pin IDLER_STEP_PIN = { GPIOD_LOCAL, static_cast<uint8_t>(1U << 6U) };

static const hal::gpio::GPIO_pin FINDA_PIN = { GPIOF_LOCAL, static_cast<uint8_t>(1U << 6U) };

#endif // PRUSA_SPOOLER_MMU_HAL_HH
