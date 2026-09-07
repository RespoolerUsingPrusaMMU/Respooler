//--------------------------------------------------------------------
/**
 * @file eeprom.h
 * @brief Host-side AVR EEPROM shim for unit testing Rewinder.cc.
 */
//--------------------------------------------------------------------
#ifndef PRUSA_SPOOLER_TEST_FAKE_AVR_EEPROM_H
#define PRUSA_SPOOLER_TEST_FAKE_AVR_EEPROM_H

#include <stdint.h>
#include <unordered_map>

#define EEMEM

namespace fake_avr
{
inline std::unordered_map<const uint16_t *, uint16_t> &eepromWords()
{
  static std::unordered_map<const uint16_t *, uint16_t> tWords;
  return tWords;
}

inline void resetEeprom()
{
  eepromWords().clear();
}

inline uint16_t readWord(const uint16_t *aAddress)
{
  const std::unordered_map<const uint16_t *, uint16_t>::const_iterator tIterator =
    eepromWords().find(aAddress);

  uint16_t tValue = 0xFFFFU;

  if(tIterator != eepromWords().end())
  {
    tValue = tIterator->second;
  }

  return tValue;
}

inline void updateWord(uint16_t *aAddress, uint16_t aValue)
{
  eepromWords()[aAddress] = aValue;
}
} // namespace fake_avr

inline uint16_t eeprom_read_word(const uint16_t *aAddress)
{
  return fake_avr::readWord(aAddress);
}

inline void eeprom_update_word(uint16_t *aAddress, uint16_t aValue)
{
  fake_avr::updateWord(aAddress, aValue);
}

#endif // PRUSA_SPOOLER_TEST_FAKE_AVR_EEPROM_H
