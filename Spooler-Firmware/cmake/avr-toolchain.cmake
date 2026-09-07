# =============================================================================
# AVR CMake Toolchain
#
# Configures CMake to build firmware for an AVR microcontroller rather than
# for the macOS host computer.
# =============================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

# Locate the AVR compiler from PATH.
find_program(AVR_GCC avr-gcc REQUIRED)
find_program(AVR_GXX avr-g++ REQUIRED)
find_program(AVR_AR avr-ar REQUIRED)
find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
find_program(AVR_SIZE avr-size REQUIRED)

set(CMAKE_C_COMPILER   "${AVR_GCC}")
set(CMAKE_CXX_COMPILER "${AVR_GXX}")
set(CMAKE_ASM_COMPILER "${AVR_GCC}")
set(CMAKE_AR           "${AVR_AR}")

# The resulting firmware cannot be executed on the build machine.
# Tell CMake's compiler tests to build static libraries instead of trying
# to link and/or execute native test programs.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Prevent any macOS-specific SDK/architecture settings from being inherited.
set(CMAKE_OSX_ARCHITECTURES "" CACHE STRING "" FORCE)
set(CMAKE_OSX_SYSROOT "" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "" FORCE)
