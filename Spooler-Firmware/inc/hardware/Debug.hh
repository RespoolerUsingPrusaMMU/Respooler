#ifndef SPOOLER_HARDWARE_DEBUG_HH
#define SPOOLER_HARDWARE_DEBUG_HH

#include <stdint.h>

namespace spooler
{
namespace hardware
{

//--------------------------------------------------------------------
/**
 * @brief Simple UART debug output for the ATmega32U4.
 *
 * Debug text is transmitted through USART1 TX on PD3.  The simulator
 * can capture this UART stream and display it on the host.
 */
//--------------------------------------------------------------------
class Debug
{
public:

  //--------------------------------------------------------------------
  /**
   * @brief Initializes USART1 for debug output.
   *
   * @param aBaudRate Desired UART baud rate.
   */
  //--------------------------------------------------------------------
  static void init(uint32_t aBaudRate = 115200UL);

  //--------------------------------------------------------------------
  /**
   * @brief Sends one character.
   *
   * @param aCharacter Character to transmit.
   */
  //--------------------------------------------------------------------
  static void putChar(char aCharacter);

  //--------------------------------------------------------------------
  /**
   * @brief Sends a null-terminated string.
   *
   * @param aString String to transmit.
   */
  //--------------------------------------------------------------------
  static void print(const char *aString);

  //--------------------------------------------------------------------
  /**
   * @brief Sends a null-terminated string followed by a newline.
   *
   * @param aString String to transmit.
   */
  //--------------------------------------------------------------------
  static void printLine(const char *aString);

  //--------------------------------------------------------------------
  /**
   * @brief Prints an unsigned 32-bit integer in decimal.
   *
   * @param aValue Value to print.
   */
  //--------------------------------------------------------------------
  static void print(uint32_t aValue);
};

}
}

#endif