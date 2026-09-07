cmake_minimum_required(VERSION 3.16)
project(PrusaMMUSimulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Expected layout:
#   PrusaMMUFirmware/
#     simavr/
#     Prusa-Spooler-Firmware/
#     Prusa-MMU-Simulator/
set(SIMAVR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../simavr" CACHE PATH
    "Path to the simavr source tree")

set(SIMAVR_INCLUDE_DIR "${SIMAVR_ROOT}/simavr/sim")
if(NOT EXISTS "${SIMAVR_INCLUDE_DIR}/sim_avr.h")
    message(FATAL_ERROR
        "simavr headers were not found at ${SIMAVR_INCLUDE_DIR}. "
        "Set -DSIMAVR_ROOT=/path/to/simavr")
endif()

# A source-tree simavr build places libsimavr in simavr/obj-<host>/.
file(GLOB SIMAVR_LIBRARY_CANDIDATES
    "${SIMAVR_ROOT}/simavr/obj-*/libsimavr.a"
    "${SIMAVR_ROOT}/simavr/obj-*/libsimavr.dylib"
    "${SIMAVR_ROOT}/simavr/obj-*/libsimavr.so")

list(LENGTH SIMAVR_LIBRARY_CANDIDATES SIMAVR_LIBRARY_COUNT)
if(SIMAVR_LIBRARY_COUNT EQUAL 0)
    message(FATAL_ERROR
        "No built libsimavr was found below ${SIMAVR_ROOT}/simavr/obj-*. "
        "Build simavr first with 'make'.")
endif()
list(GET SIMAVR_LIBRARY_CANDIDATES 0 SIMAVR_LIBRARY)
message(STATUS "Using simavr library: ${SIMAVR_LIBRARY}")

add_executable(prusa_mmu_sim
    src/main.cpp
    src/MmuBoard.cpp
    src/ShiftRegister.cpp
    src/StepperMotor.cpp
    src/Tmc2130.cpp
    src/ControlInterface.cpp)

target_include_directories(prusa_mmu_sim PRIVATE
    "${SIMAVR_INCLUDE_DIR}")

target_compile_options(prusa_mmu_sim PRIVATE
    -Wall -Wextra -Wpedantic)

# simavr's ELF loader uses libelf when the library was built with it.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBELF QUIET libelf)
endif()
if(LIBELF_FOUND)
    target_include_directories(prusa_mmu_sim PRIVATE ${LIBELF_INCLUDE_DIRS})
    target_link_directories(prusa_mmu_sim PRIVATE ${LIBELF_LIBRARY_DIRS})
endif()

target_link_libraries(prusa_mmu_sim PRIVATE
    "${SIMAVR_LIBRARY}"
    ${LIBELF_LIBRARIES}
    m)

if(APPLE)
    # Keep the build-tree simulator independent of DYLD_LIBRARY_PATH when a
    # dylib build of simavr was selected.
    set_target_properties(prusa_mmu_sim PROPERTIES
        BUILD_RPATH "${SIMAVR_ROOT}/simavr")
endif()
