#include "hardware/DebugUsb.hh"

#include <avr/io.h>

extern "C"
{
#include "Descriptors.h"
}

#if !defined(__AVR_ATmega32U4__)
#error "DebugUsb requires an ATmega32U4."
#endif

#if F_CPU != 16000000UL
#error "DebugUsb requires the MMU board's 16 MHz CPU clock."
#endif

namespace
{

USB_ClassInfo_CDC_Device_t sCdc = {};
bool sInitialized = false;
volatile bool sEndpointsReady = false;
uint8_t sBuffer[256];
uint8_t sWriteIndex = 0U;
uint8_t sReadIndex = 0U;
uint32_t sDroppedCount = 0UL;

//--------------------------------------------------------------------
// Discard queued text when there is no open host terminal.
//--------------------------------------------------------------------
void discardOutput()
{
  sDroppedCount += static_cast<uint8_t>(sWriteIndex - sReadIndex);
  sReadIndex = sWriteIndex;
}

}

//--------------------------------------------------------------------
// LUFA callbacks must use C linkage.
//--------------------------------------------------------------------
extern "C"
{

void EVENT_USB_Device_ConfigurationChanged(void)
{
  sEndpointsReady = CDC_Device_ConfigureEndpoints(&sCdc);
}

void EVENT_USB_Device_Disconnect(void)
{
  sEndpointsReady = false;
}

void EVENT_USB_Device_Reset(void)
{
  sEndpointsReady = false;
}

void EVENT_USB_Device_ControlRequest(void)
{
  CDC_Device_ProcessControlRequest(&sCdc);
}

}

namespace spooler
{
namespace hardware
{

//--------------------------------------------------------------------
// Initialize the CDC interface and the MMU's native USB hardware.
// Global interrupts and the 16 MHz CPU clock are the caller's responsibility.
//--------------------------------------------------------------------
void DebugUsb::init(uint32_t aBaudRate)
{
  (void)aBaudRate;

  if(sInitialized)
  {
    return;
  }

  sCdc.Config.ControlInterfaceNumber = INTERFACE_ID_CDC_CCI;
  sCdc.Config.DataINEndpoint.Address = CDC_TX_EPADDR;
  sCdc.Config.DataINEndpoint.Size = CDC_TXRX_EPSIZE;
  sCdc.Config.DataINEndpoint.Banks = 1U;
  sCdc.Config.DataOUTEndpoint.Address = CDC_RX_EPADDR;
  sCdc.Config.DataOUTEndpoint.Size = CDC_TXRX_EPSIZE;
  sCdc.Config.DataOUTEndpoint.Banks = 1U;
  sCdc.Config.NotificationEndpoint.Address = CDC_NOTIFICATION_EPADDR;
  sCdc.Config.NotificationEndpoint.Size = CDC_NOTIFICATION_EPSIZE;
  sCdc.Config.NotificationEndpoint.Banks = 1U;

  sReadIndex = 0U;
  sWriteIndex = 0U;
  sDroppedCount = 0UL;
  sEndpointsReady = false;

  //--------------------------------------------------------------------
  // Match the PLL setup used by PRUSA's ATmega32U4 MMU firmware.
  //--------------------------------------------------------------------
  PLLFRQ = static_cast<uint8_t>((1U << PLLUSB) |
                               (1U << PDIV3) |
                               (1U << PDIV1));
  USB_PLL_On();

  while(!USB_PLL_IsReady())
  {
  }

  USB_Init();
  sInitialized = true;
}

//--------------------------------------------------------------------
// A terminal must configure the device, set line coding, and assert DTR.
//--------------------------------------------------------------------
bool DebugUsb::isConnected()
{
  return sInitialized && sEndpointsReady &&
    (USB_DeviceState == DEVICE_STATE_Configured) &&
    (sCdc.State.LineEncoding.BaudRateBPS != 0UL) &&
    ((sCdc.State.ControlLineStates.HostToDevice &
      CDC_CONTROL_LINE_OUT_DTR) != 0U);
}

//--------------------------------------------------------------------
// Queue output without waiting for USB bandwidth or a host connection.
//--------------------------------------------------------------------
void DebugUsb::putChar(char aCharacter)
{
  if(!isConnected())
  {
    discardOutput();
    ++sDroppedCount;
    return;
  }

  const uint8_t tNextIndex = static_cast<uint8_t>(sWriteIndex + 1U);

  if(tNextIndex == sReadIndex)
  {
    ++sDroppedCount;
    return;
  }

  sBuffer[sWriteIndex] = static_cast<uint8_t>(aCharacter);
  sWriteIndex = tNextIndex;
}

//--------------------------------------------------------------------
// Service USB, discard input, and submit at most one short IN packet.
// Short packets avoid needing a subsequent zero-length termination packet.
// Automatic CDC flushing is disabled in lufa_config.h because it can wait.
//--------------------------------------------------------------------
void DebugUsb::task()
{
  if(!sInitialized)
  {
    return;
  }

  USB_USBTask();
  CDC_Device_USBTask(&sCdc);

  for(uint8_t tIndex = 0U; tIndex < CDC_TXRX_EPSIZE; ++tIndex)
  {
    if(CDC_Device_ReceiveByte(&sCdc) < 0)
    {
      break;
    }
  }

  if(!isConnected())
  {
    discardOutput();
    return;
  }

  if(sReadIndex == sWriteIndex)
  {
    return;
  }

  Endpoint_SelectEndpoint(CDC_TX_EPADDR);

  if(!Endpoint_IsINReady())
  {
    return;
  }

  for(uint8_t tIndex = 0U;
      (tIndex < (CDC_TXRX_EPSIZE - 1U)) && (sReadIndex != sWriteIndex);
      ++tIndex)
  {
    Endpoint_Write_8(sBuffer[sReadIndex]);
    ++sReadIndex;
  }

  Endpoint_ClearIN();
}

//--------------------------------------------------------------------
// Return the best-effort output loss counter.
//--------------------------------------------------------------------
uint32_t DebugUsb::getDroppedCount()
{
  return sDroppedCount;
}

//--------------------------------------------------------------------
// Send a null-terminated string.
//--------------------------------------------------------------------
void DebugUsb::print(const char *aString)
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
void DebugUsb::printLine(const char *aString)
{
  print(aString);
  putChar('\r');
  putChar('\n');
}

//--------------------------------------------------------------------
// Print a uint32_t without pulling printf() into the firmware.
//--------------------------------------------------------------------
void DebugUsb::print(uint32_t aValue)
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
