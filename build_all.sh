#!/usr/bin/env bash

set -euo pipefail

# ============================================================================
# build_all.sh
#
# Complete first-time Debug build for the Respooler project.
#
# Builds:
#
#   1. simavr
#   2. Prusa-MMU-Simulator
#   3. Spooler-Firmware
#
# Usage:
#
#   git clone <repository>
#   cd Respooler
#   ./build_all.sh
#
# Optional:
#
#   ./build_all.sh --clean
#
# removes all generated build directories before rebuilding.
# ============================================================================

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SIMAVR_DIR="${REPO_DIR}/simavr"
MMU_SIM_DIR="${REPO_DIR}/Prusa-MMU-Simulator"
SPOOLER_DIR="${REPO_DIR}/Spooler-Firmware"

BUILD_ROOT="${REPO_DIR}/build"

CLEAN_BUILD=false

if [[ "${1:-}" == "--clean" ]]; then
    CLEAN_BUILD=true
fi


# ============================================================================
# Utility functions
# ============================================================================

print_header()
{
    echo
    echo "============================================================"
    echo " $1"
    echo "============================================================"
    echo
}


check_command()
{
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ERROR: Required command '$1' was not found."
        echo
        exit 1
    fi
}


check_directory()
{
    if [[ ! -d "$1" ]]; then
        echo "ERROR: Required directory does not exist:"
        echo
        echo "  $1"
        echo
        exit 1
    fi
}


# ============================================================================
# Initial setup
# ============================================================================

print_header "Respooler Complete Debug Build"

echo "Repository directory:"
echo "  ${REPO_DIR}"
echo


# --------------------------------------------------------------------------
# Verify the basic host tools.
# --------------------------------------------------------------------------

echo "Checking required development tools..."

check_command git
check_command cmake
check_command make

echo "Required basic tools found."


# --------------------------------------------------------------------------
# Initialize any Git submodules.
#
# This is especially useful if simavr or another component is stored as a
# Git submodule.
# --------------------------------------------------------------------------

echo
echo "Initializing Git submodules..."

#git -C "${REPO_DIR}" submodule update --init --recursive
# --------------------------------------------------------------------------
# Initialize Git submodules only if this repository actually defines them.
# --------------------------------------------------------------------------

if [[ -f "${REPO_DIR}/.gitmodules" ]]; then
    if grep -q 'url =' "${REPO_DIR}/.gitmodules"; then
        echo
        echo "Initializing Git submodules..."
        git -C "${REPO_DIR}" submodule update --init --recursive
    else
        echo
        echo "No configured Git submodules found."
    fi
fi

# --------------------------------------------------------------------------
# Verify that the expected source directories exist.
# --------------------------------------------------------------------------

check_directory "${SIMAVR_DIR}"
check_directory "${MMU_SIM_DIR}"
check_directory "${SPOOLER_DIR}"


# --------------------------------------------------------------------------
# Perform a clean build if requested.
# --------------------------------------------------------------------------

if [[ "${CLEAN_BUILD}" == true ]]; then
    print_header "Cleaning Previous Builds"

    rm -rf "${BUILD_ROOT}"

    echo "Build directories removed."
fi

mkdir -p "${BUILD_ROOT}"


# ============================================================================
# Build simavr
# ============================================================================

print_header "Building simavr"

#
# simavr normally uses GNU Make rather than CMake.
#
# Build directly in the simavr source tree. This creates the simavr
# libraries and simulator executable needed by the MMU simulator.
#

pushd "${SIMAVR_DIR}" >/dev/null

if [[ "${CLEAN_BUILD}" == true ]]; then
    make clean || true
fi

make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

popd >/dev/null

echo
echo "simavr build completed."


# ============================================================================
# Build Prusa-MMU-Simulator
# ============================================================================

print_header "Building Prusa-MMU-Simulator"

MMU_SIM_BUILD="${BUILD_ROOT}/Prusa-MMU-Simulator/debug"

mkdir -p "${MMU_SIM_BUILD}"

if [[ ! -f "${MMU_SIM_DIR}/CMakeLists.txt" ]]; then
    echo "ERROR: Cannot find:"
    echo
    echo "  ${MMU_SIM_DIR}/CMakeLists.txt"
    echo
    exit 1
fi

cmake \
    -S "${MMU_SIM_DIR}" \
    -B "${MMU_SIM_BUILD}" \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build "${MMU_SIM_BUILD}" --parallel

echo
echo "Prusa-MMU-Simulator build completed."

# ============================================================================
# Build Spooler-Firmware
# ============================================================================

print_header "Building Spooler-Firmware"

SPOOLER_BUILD="${BUILD_ROOT}/Spooler-Firmware/debug"
AVR_TOOLCHAIN_FILE="${SPOOLER_DIR}/cmake/avr-toolchain.cmake"

# --------------------------------------------------------------------------
# Verify that the AVR compiler is installed.
# --------------------------------------------------------------------------

if ! command -v avr-gcc >/dev/null 2>&1; then
    echo
    echo "ERROR: avr-gcc was not found."
    echo
    exit 1
fi

if ! command -v avr-g++ >/dev/null 2>&1; then
    echo
    echo "ERROR: avr-g++ was not found."
    echo
    exit 1
fi

if [[ ! -f "${AVR_TOOLCHAIN_FILE}" ]]; then
    echo
    echo "ERROR: AVR CMake toolchain file not found:"
    echo
    echo "  ${AVR_TOOLCHAIN_FILE}"
    echo
    exit 1
fi

echo "Using AVR toolchain:"
echo
echo "  C compiler   : $(command -v avr-gcc)"
echo "  C++ compiler : $(command -v avr-g++)"
echo

avr-gcc --version | head -1

# --------------------------------------------------------------------------
# Always recreate the firmware build directory.
#
# The compiler and target platform are cached by CMake. Starting with a clean
# build directory prevents an earlier native macOS configuration from being
# accidentally reused.
# --------------------------------------------------------------------------

rm -rf "${SPOOLER_BUILD}"
mkdir -p "${SPOOLER_BUILD}"

# --------------------------------------------------------------------------
# Configure the firmware as an AVR cross-compiled project.
# --------------------------------------------------------------------------

cmake \
    -S "${SPOOLER_DIR}" \
    -B "${SPOOLER_BUILD}" \
    -DCMAKE_TOOLCHAIN_FILE="${AVR_TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE=Debug

# --------------------------------------------------------------------------
# Build the firmware.
# --------------------------------------------------------------------------

cmake --build "${SPOOLER_BUILD}" --parallel

echo
echo "Spooler-Firmware build completed."

# ============================================================================
# Finished
# ============================================================================

print_header "Complete Build Successful"

echo "The following components were built successfully:"
echo
echo "  simavr"
echo "  Prusa-MMU-Simulator"
echo "  Spooler-Firmware"
echo
echo "Generated CMake build directories:"
echo
echo "  ${MMU_SIM_BUILD}"
echo "  ${SPOOLER_BUILD}"
echo

