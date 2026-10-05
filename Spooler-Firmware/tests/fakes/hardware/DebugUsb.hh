//--------------------------------------------------------------------
/**
 * @file DebugUsb.hh
 * @brief Host-side diagnostic capture used by the Rewinder gtest suite.
 */
//--------------------------------------------------------------------
#ifndef PRUSA_SPOOLER_TEST_FAKE_DEBUG_USB_HH
#define PRUSA_SPOOLER_TEST_FAKE_DEBUG_USB_HH

#include <stdint.h>
#include <string>

namespace spooler
{
namespace hardware
{

class DebugUsb
{
public:
  static bool isConnected()
  {
    return connected();
  }

  static void setConnected(bool aConnected)
  {
    connected() = aConnected;
  }

  static void print(const char *aText)
  {
    buffer() += aText;
  }

  static void print(uint32_t aValue)
  {
    buffer() += std::to_string(aValue);
  }

  static void printLine(const char *aText)
  {
    buffer() += aText;
    buffer() += '\n';
  }

  static const std::string &text()
  {
    return buffer();
  }

  static void clear()
  {
    buffer().clear();
  }

private:
  static bool &connected()
  {
    static bool tConnected = true;
    return tConnected;
  }

  static std::string &buffer()
  {
    static std::string tBuffer;
    return tBuffer;
  }
};

} // namespace hardware
} // namespace spooler

#endif // PRUSA_SPOOLER_TEST_FAKE_DEBUG_USB_HH
