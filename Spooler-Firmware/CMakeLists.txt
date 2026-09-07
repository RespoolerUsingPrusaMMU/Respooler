cmake_minimum_required(VERSION 3.19)
project(PrusaSpoolerFirmware LANGUAGES C CXX ASM)

#set(PRUSA_MMU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Prusa-Firmware-MMU-main"
#    CACHE PATH "Path to the Prusa-Firmware-MMU source tree")

#if(NOT EXISTS "${PRUSA_MMU_ROOT}/src/hal/avr/spi.cpp")
#  message(FATAL_ERROR
#      "Prusa MMU sources were not found at ${PRUSA_MMU_ROOT}. "
#      "Configure with -DPRUSA_MMU_ROOT=/path/to/Prusa-Firmware-MMU-main")
#endif()

add_executable(spooler_firmware
    src/main.cc
    src/app/Rewinder.cc
    src/hardware/Board.cc
    src/hardware/Buttons.cc
    src/hardware/Debug.cc
    src/hardware/Finda.cc
    src/hardware/Leds.cc
    src/hardware/Tmc2130.cc
    src/hardware/MmuHal.cc
    src/motion/StepperAxis.cc
    src/motion/MotionController.cc

)

target_include_directories(spooler_firmware PRIVATE
    inc
)

set_target_properties(spooler_firmware PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
    OUTPUT_NAME firmware
)

target_compile_definitions(spooler_firmware PRIVATE F_CPU=16000000L)
target_compile_definitions(
    spooler_firmware
    PRIVATE
        $<$<CONFIG:Debug>:SPOOLER_DEBUG>
)
target_compile_options(spooler_firmware PRIVATE
    -mmcu=atmega32u4
    -Wall
    -Wextra
    -Wsign-compare
    -Os
    -g
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -ffunction-sections
    -fdata-sections
)

target_link_options(spooler_firmware PRIVATE
    -mmcu=atmega32u4
    -Wl,--gc-sections
    -Wl,--defsym=__TEXT_REGION_LENGTH__=28K
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/firmware.map
)

add_custom_command(TARGET spooler_firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex -S $<TARGET_FILE:spooler_firmware>
            ${CMAKE_CURRENT_BINARY_DIR}/firmware.hex
    COMMAND ${CMAKE_OBJDUMP} -CSd $<TARGET_FILE:spooler_firmware>
            > ${CMAKE_CURRENT_BINARY_DIR}/firmware.asm
    COMMAND ${CMAKE_SIZE_UTIL} -B -d $<TARGET_FILE:spooler_firmware>
    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.hex
        ${CMAKE_CURRENT_BINARY_DIR}/firmware.asm
    COMMENT "Generating HEX/ASM and reporting ATmega32U4 memory usage"
)
