# Prusa MMU Spooler Development Environment

This directory contains the complete development environment for the Prusa MMU-based filament spool rewinder project.

The project is divided into three separate directories:

```text
PrusaMMUFirmware/
├── simavr/
├── Prusa-MMU-Simulator/
└── Spooler-Firmware/
```

Each directory has a different purpose and is built independently.

## Directory Overview

### `Spooler-Firmware`

This directory contains the actual firmware that runs on the ATmega32U4 processor on the Prusa MMU controller board.

The firmware implements the filament rewinder application, including:

- Shuttle/traverse motor control
- Take-up spool motor control
- Supply spool braking and tension
- TMC2130 stepper-driver configuration
- FINDA filament detection
- MMU front-panel button handling
- LED status indication
- Shuttle homing using TMC2130 StallGuard
- Adjustable outer winding limit
- Pause and resume operation
- Filament runout handling
- Manual shuttle re-homing
- EEPROM storage of configuration information
- Debug UART output when `SPOOLER_DEBUG` is enabled

The firmware is cross-compiled for the ATmega32U4 using the AVR GCC toolchain.

The current development environment uses the AVR tools installed under:

```text
/opt/local/bin/
```

```text
sudo port selfupdate
sudo port install avr-gcc avr-binutils avr-libc
```

If you don't have mac port then you can install it from here: https://www.macports.org/install.php

Typical tools include:

```text
avr-gcc
avr-g++
avr-objcopy
avr-objdump
avr-size
```

A normal Release build is created with:

```bash
cd Spooler-Firmware

cmake -S . -B build/release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/release
```

The resulting firmware ELF is:

```text
Spooler-Firmware/build/release/firmware
```

A Debug build is created with:

```bash
cmake -S . -B build/debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug
```

The Debug ELF is:

```text
Spooler-Firmware/build/debug/firmware
```

The Debug configuration defines:

```text
SPOOLER_DEBUG
```

which enables firmware diagnostic messages sent through the simulated or physical UART.

---

### `Prusa-MMU-Simulator`

This directory contains the host-side simulator used to test the spooler firmware without requiring the physical Prusa MMU controller.

The simulator is a native macOS C++ application. It does **not** run on the AVR processor.

It uses `simavr` to emulate the ATmega32U4 and adds models for the external MMU hardware that the spooler firmware expects to see.

The simulator models such functions as:

- ATmega32U4 execution
- TMC2130 SPI communications
- Stepper motors
- Shuttle mechanical position
- Shuttle end stops
- StallGuard behavior
- FINDA filament sensor
- MMU push buttons
- LED shift registers
- Take-up motor
- Brake motor
- Automatic filament runout
- TMC2130 fault conditions
- UART debug output

The simulator also provides a command interface using:

```text
/tmp/prusa-mmu-sim.cmd
```

For example:

```bash
echo 'status' > /tmp/prusa-mmu-sim.cmd
```

Simulate filament being installed:

```bash
echo 'filament present' > /tmp/prusa-mmu-sim.cmd
```

Simulate a center-button press:

```bash
echo 'tap middle 500' > /tmp/prusa-mmu-sim.cmd
```

The simulator is built as a native macOS program:

```bash
cd Prusa-MMU-Simulator

cmake -S . -B build \
  -DSIMAVR_ROOT=../simavr

cmake --build build
```

The resulting executable is:

```text
Prusa-MMU-Simulator/build/prusa_mmu_sim
```

A Debug version of the simulator can be built with:

```bash
cmake -S . -B build/debug -G Ninja \
  -DSIMAVR_ROOT=../simavr \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug
```

It is important to distinguish the two Debug builds:

```text
Spooler-Firmware/build/debug/firmware
```

is AVR firmware running inside the emulated ATmega32U4.

```text
Prusa-MMU-Simulator/build/debug/prusa_mmu_sim
```

is the native macOS program that performs the simulation.

---

### `simavr`

This directory contains the `simavr` AVR processor emulator.

`simavr` is a third-party open-source project and is not part of the spooler application itself.

It provides the low-level emulation of the ATmega32U4 processor, including:

- AVR instruction execution
- CPU registers
- SRAM
- Flash memory
- Interrupts
- Timers
- GPIO
- ADC
- SPI
- UART
- EEPROM
- Other AVR peripherals

The `Prusa-MMU-Simulator` links against the `simavr` library.

The relationship is:

```text
Spooler-Firmware
      |
      | AVR executable
      v
Prusa-MMU-Simulator
      |
      | uses
      v
simavr
      |
      v
Emulated ATmega32U4
```

The `Spooler-Firmware` does not directly link against `simavr`.

The firmware is intended to run unchanged either:

1. inside the simulator, or
2. on the actual ATmega32U4 MMU controller.

This is important because the simulator tests the same AVR firmware that will eventually run on the physical hardware.

## Overall Architecture

The three directories work together as follows:

```text
                       PrusaMMUFirmware
                              |
          +-------------------+-------------------+
          |                   |                   |
          v                   v                   v

  Spooler-Firmware     Prusa-MMU-Simulator      simavr
          |                   |                   |
          |                   |                   |
          | AVR ELF           | native C++        |
          |                   | simulator          |
          +--------->---------+--------->----------+
                              |
                              v
                    Emulated ATmega32U4
                              |
                              v
                    Simulated MMU hardware
```

The firmware produces an AVR ELF file such as:

```text
Spooler-Firmware/build/debug/firmware
```

The simulator loads that ELF file:

```bash
Prusa-MMU-Simulator/build/prusa_mmu_sim \
  Spooler-Firmware/build/debug/firmware
```

`simavr` then executes the AVR instructions contained in the firmware while `Prusa-MMU-Simulator` emulates the MMU-specific hardware surrounding the processor.

## Recommended Development Workflow

A typical development cycle is:

1. Modify the source code in:

   ```text
   Spooler-Firmware/
   ```

2. Build the Debug firmware:

   ```bash
   cd Spooler-Firmware

   cmake --build build/debug
   ```

3. Build the simulator if simulator code has changed:

   ```bash
   cd ../Prusa-MMU-Simulator

   cmake --build build/debug
   ```

4. Start the simulator using the Debug firmware ELF.

5. Monitor firmware debug messages.

6. Exercise the simulated hardware using commands such as:

   ```bash
   echo 'status' > /tmp/prusa-mmu-sim.cmd

   echo 'filament present' > /tmp/prusa-mmu-sim.cmd

   echo 'tap middle 500' > /tmp/prusa-mmu-sim.cmd

   echo 'tap left 500' > /tmp/prusa-mmu-sim.cmd

   echo 'tap right 500' > /tmp/prusa-mmu-sim.cmd
   ```

7. Once the behavior has been verified in the simulator, build the Release firmware for use on the physical MMU controller.

## Directory Responsibilities

The separation between the directories is intentional.

| Directory | Runs On | Purpose |
|---|---|---|
| `Spooler-Firmware` | ATmega32U4 | Actual rewinder firmware |
| `Prusa-MMU-Simulator` | macOS development computer | Simulates the MMU board and mechanical system |
| `simavr` | macOS development computer | Emulates the AVR processor and peripherals |

Changes to normal rewinder behavior should generally be made in:

```text
Spooler-Firmware/
```

Changes to simulated buttons, motors, FINDA, StallGuard, TMC2130 behavior, or other simulated hardware should generally be made in:

```text
Prusa-MMU-Simulator/
```

Changes to the AVR emulator itself should normally not be necessary. The contents of:

```text
simavr/
```

should generally be treated as third-party infrastructure.

## Important Path Relationship

With all three directories stored together, the preferred layout is:

```text
/Users/andrepruitt/PrusaMMUFirmware/
├── simavr/
├── Prusa-MMU-Simulator/
└── Spooler-Firmware/
```

This allows the simulator to refer to `simavr` using the relative path:

```text
../simavr
```

instead of depending on an absolute user-specific path such as:

```text
/Users/andrepruitt/simavr
```

Using relative paths makes the complete development tree easier to move, archive, copy to another computer, or place under source control.