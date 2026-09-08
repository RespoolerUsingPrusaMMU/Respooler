cmake_minimum_required(VERSION 3.16)
project(PrusaMMUSimulator VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Expected layout:
#   PrusaMMUFirmware/
#     simavr/
#     Spooler-Firmware/
#     Prusa-MMU-Simulator/
set(SIMAVR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../simavr" CACHE PATH
    "Path to the simavr source tree")

set(SIMAVR_INCLUDE_DIR "${SIMAVR_ROOT}/simavr/sim")
if(NOT EXISTS "${SIMAVR_INCLUDE_DIR}/sim_avr.h")
    message(FATAL_ERROR
        "simavr headers were not found at ${SIMAVR_INCLUDE_DIR}. "
        "Set -DSIMAVR_ROOT=/path/to/simavr")
endif()

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

# simavr requires libelf. Resolve the actual library file instead of
# linking with a bare -lelf, because Homebrew and MacPorts install
# libraries outside the default Apple linker search path.
find_library(ELF_LIBRARY
    NAMES elf
    HINTS
        /opt/homebrew/lib
        /opt/local/lib
    REQUIRED)
message(STATUS "Using libelf library: ${ELF_LIBRARY}")

add_library(prusa_mmu_sim_core STATIC
    src/MmuBoard.cpp
    src/ShiftRegister.cpp
    src/StepperMotor.cpp
    src/Tmc2130.cpp
    src/ControlInterface.cpp
    src/SimulatorController.cpp)

target_include_directories(prusa_mmu_sim_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${SIMAVR_INCLUDE_DIR}")

target_compile_options(prusa_mmu_sim_core PRIVATE
    -Wall -Wextra -Wpedantic)

target_link_libraries(prusa_mmu_sim_core PUBLIC
    "${SIMAVR_LIBRARY}"
    "${ELF_LIBRARY}"
    m)

# Original command-line simulator. It remains available for automated tests,
# scripting, CI, and workflows that do not need the GUI.
add_executable(prusa_mmu_sim src/main.cpp)
target_link_libraries(prusa_mmu_sim PRIVATE prusa_mmu_sim_core)

# Qt is optional so the command-line simulator can still be built on systems
# that do not have Qt installed.
find_package(Qt6 COMPONENTS Core Widgets QUIET)
if(Qt6_FOUND)
    # Generate Qt meta-object sources explicitly.  This avoids relying on
    # AUTOMOC's header-discovery behavior for the project's .hh files and
    # guarantees that every class containing Q_OBJECT receives its moc code.
    qt6_wrap_cpp(PRUSA_MMU_GUI_MOC_SOURCES
        src/gui/MainWindow.hh
        src/gui/MachineWidget.hh
        src/gui/SimulationWorker.hh)

    add_executable(prusa_mmu_sim_gui
        src/gui/main_gui.cpp
        src/gui/MainWindow.cpp
        src/gui/MainWindow.hh
        src/gui/MachineWidget.cpp
        src/gui/MachineWidget.hh
        src/gui/SimulationWorker.cpp
        src/gui/SimulationWorker.hh
        ${PRUSA_MMU_GUI_MOC_SOURCES})

    # MOC is generated explicitly above, so disable AUTOMOC for this target
    # to prevent duplicate meta-object generation.
    set_target_properties(prusa_mmu_sim_gui PROPERTIES AUTOMOC OFF)

    target_include_directories(prusa_mmu_sim_gui PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/gui")

    target_compile_options(prusa_mmu_sim_gui PRIVATE
        -Wall -Wextra -Wpedantic)

    target_link_libraries(prusa_mmu_sim_gui PRIVATE
        prusa_mmu_sim_core
        Qt6::Core
        Qt6::Widgets)

    message(STATUS "Qt6 found: building prusa_mmu_sim_gui")
else()
    message(STATUS "Qt6 not found: building command-line simulator only")
    message(STATUS "Install Qt6 and reconfigure to build prusa_mmu_sim_gui")
endif()

if(APPLE)
    set_target_properties(prusa_mmu_sim PROPERTIES
        BUILD_RPATH "${SIMAVR_ROOT}/simavr")

    if(TARGET prusa_mmu_sim_gui)
        set_target_properties(prusa_mmu_sim_gui PROPERTIES
            BUILD_RPATH "${SIMAVR_ROOT}/simavr")
    endif()
endif()
