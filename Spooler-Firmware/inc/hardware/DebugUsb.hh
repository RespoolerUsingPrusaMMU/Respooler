#ifndef SPOOLER_HARDWARE_DEBUG_USB_HH
#define SPOOLER_HARDWARE_DEBUG_USB_HH

#include <stdint.h>

namespace spooler
{
namespace hardware
{

//--------------------------------------------------------------------
/**
 * @brief USB CDC debug output for the ATmega32U4 PRUSA MMU board.
 *
 * Uses native USB and the bundled LUFA stack. Call task() regularly
 * from the main loop. Output is best effort: disconnected output and
 * bytes exceeding the 255-byte queue capacity are dropped.
 * Call only from the main context, never from an interrupt handler.
 * This class owns USB; do not link another USB stack or event handlers.
 */
//--------------------------------------------------------------------
class DebugUsb
{
public:

  //--------------------------------------------------------------------
  /**
   * @brief Initializes USB without waiting for a host. Call once at startup.
   *
   * @param aBaudRate Retained for compatibility; USB ignores this value.
   */
  //--------------------------------------------------------------------
  static void init(uint32_t aBaudRate = 115200UL);

  //--------------------------------------------------------------------
  /**
   * @brief Queues one character, or drops it if disconnected or the queue is full.
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
  //--------------------------------------------------------------------
  /**
   * @brief Services enumeration and sends up to 63 queued bytes.
   *
   * Call on every main-loop iteration, preferably every millisecond.
   * There is no wait for the host to consume debug output. USB control
   * request handling still uses LUFA and is not a hard real-time API.
   */
  //--------------------------------------------------------------------
  static void task();

  //--------------------------------------------------------------------
  /**
   * @brief True when configured and the terminal has asserted DTR.
   */
  //--------------------------------------------------------------------
  static bool isConnected();

  //--------------------------------------------------------------------
  /**
   * @brief Returns the number of dropped bytes since init().
   *
   * Includes queued bytes discarded when a disconnect is observed.
   * Wraps after UINT32_MAX. Does not count USB packets lost in transit.
   */
  //--------------------------------------------------------------------
  static uint32_t getDroppedCount();

};

}
}

#endif
