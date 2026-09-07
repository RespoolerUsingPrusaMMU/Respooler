#include "hardware/Debug.hh"

#include <avr/io.h>

namespace spooler
{
namespace hardware
{

//--------------------------------------------------------------------
// Initialize USART1 for 8 data bits, no parity, and one stop bit.
//--------------------------------------------------------------------
void Debug::init(uint32_t aBaudRate)
{
  const uint32_t tBaudDivisor =
    (F_CPU / (16UL * aBaudRate)) - 1UL;

  UBRR1H = static_cast<uint8_t>(tBaudDivisor >> 8U);
  UBRR1L = static_cast<uint8_t>(tBaudDivisor);

  UCSR1A = 0U;

  //--------------------------------------------------------------------
  // Enable the USART1 transmitter.
  //--------------------------------------------------------------------
  UCSR1B = static_cast<uint8_t>(1U << TXEN1);

  //--------------------------------------------------------------------
  // 8 data bits, no parity, one stop bit.
  //--------------------------------------------------------------------
  UCSR1C =
    static_cast<uint8_t>((1U << UCSZ11) |
                         (1U << UCSZ10));
}

//--------------------------------------------------------------------
// Send one character through USART1.
//--------------------------------------------------------------------
void Debug::putChar(char aCharacter)
{
  while((UCSR1A & static_cast<uint8_t>(1U << UDRE1)) == 0U)
  {
  }

  UDR1 = static_cast<uint8_t>(aCharacter);
}

//--------------------------------------------------------------------
// Send a null-terminated string.
//--------------------------------------------------------------------
void Debug::print(const char *aString)
{
  if(aString == nullptr)
  {
    return;
  }

  while(*aString != '\0')
  {
    putChar(*aString);
    ++aString;
  }
}

//--------------------------------------------------------------------
// Send a string followed by CR/LF.
//--------------------------------------------------------------------
void Debug::printLine(const char *aString)
{
  print(aString);
  putChar('\r');
  putChar('\n');
}

//--------------------------------------------------------------------
// Print a uint32_t without pulling printf() into the firmware.
//--------------------------------------------------------------------
void Debug::print(uint32_t aValue)
{
  char tBuffer[11];
  uint8_t tIndex = 0U;

  if(aValue == 0U)
  {
    putChar('0');
    return;
  }

  while(aValue != 0U)
  {
    tBuffer[tIndex] =
      static_cast<char>('0' + (aValue % 10UL));

    ++tIndex;
    aValue /= 10UL;
  }

  while(tIndex != 0U)
  {
    --tIndex;
    putChar(tBuffer[tIndex]);
  }
}

}
}
