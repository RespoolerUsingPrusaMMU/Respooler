# Native USB CDC support for the ATmega32U4 debug build.
set(LUFA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lufa/LUFA")
add_library(spooler_usb_debug STATIC EXCLUDE_FROM_ALL
  src/hardware/DebugUsb.cc
  src/hardware/usb/Descriptors.c
  ${LUFA_ROOT}/Drivers/USB/Class/Device/CDCClassDevice.c
  ${LUFA_ROOT}/Drivers/USB/Core/AVR8/Device_AVR8.c
  ${LUFA_ROOT}/Drivers/USB/Core/AVR8/Endpoint_AVR8.c
  ${LUFA_ROOT}/Drivers/USB/Core/AVR8/EndpointStream_AVR8.c
  ${LUFA_ROOT}/Drivers/USB/Core/AVR8/USBController_AVR8.c
  ${LUFA_ROOT}/Drivers/USB/Core/AVR8/USBInterrupt_AVR8.c
  ${LUFA_ROOT}/Drivers/USB/Core/ConfigDescriptors.c
  ${LUFA_ROOT}/Drivers/USB/Core/DeviceStandardReq.c
  ${LUFA_ROOT}/Drivers/USB/Core/Events.c
  ${LUFA_ROOT}/Drivers/USB/Core/USBTask.c
)
set_target_properties(spooler_usb_debug PROPERTIES C_STANDARD 99 CXX_STANDARD 11
  CXX_STANDARD_REQUIRED YES)
target_include_directories(spooler_usb_debug PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc"
  PRIVATE src/hardware/usb third_party third_party/lufa)
target_compile_definitions(spooler_usb_debug PRIVATE F_CPU=16000000UL)
target_compile_options(spooler_usb_debug PRIVATE -mmcu=atmega32u4 -Os
  -ffunction-sections -fdata-sections
  -include "${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/usb/lufa_config.h")
target_link_options(spooler_usb_debug INTERFACE -mmcu=atmega32u4 -Wl,--gc-sections)

