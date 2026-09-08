# Prusa MMU / Spooler Simulator with Qt GUI

This simulator runs the ATmega32U4 spooler firmware inside `simavr` and models
the MMU hardware used by the filament rewinder. This revision adds an optional
Qt 6 graphical front end while retaining the original command-line simulator,
FIFO command interface, and GDB server.

## Expected directory layout

```text
PrusaMMUFirmware/
├── simavr/
├── Prusa-MMU-Simulator/
└── Spooler-Firmware/
```

`simavr` and `Prusa-MMU-Simulator` are host applications/libraries. The
`Spooler-Firmware` directory contains the AVR firmware that runs inside the
simulated ATmega32U4.

## What the GUI shows

The Qt front end provides:

- five live MMU-style front-panel LEDs;
- Left, Center/Start, and Right push buttons;
- FINDA filament-present/absent controls;
- an animated shuttle moving along its mechanical travel;
- animated shuttle, take-up, and brake motor rotors driven from observed STEP
  counts;
- motor enabled/disabled state;
- visible motor/fault indication;
- mechanical automatic/manual controls;
- shuttle StallGuard injection;
- TMC2130 missing-driver and invalid-identity injection;
- TMC2130 over-temperature and under-voltage injection;
- the original FIFO command interface for scripts and regression tests;
- the simavr GDB server on port 1234 by default.

The GUI does not directly manipulate the simulated AVR from the Qt main thread.
A simulation worker owns `MmuBoard`, `ControlInterface`, and all calls to
`avr_run()`. The GUI communicates with that worker through queued Qt
signals/slots and receives read-only `SimulatorStatus` snapshots. This keeps
Qt responsive and prevents the GUI from racing simavr.

## Install dependencies on macOS

The project assumes MacPorts is installed. Install Qt 6 with:

```bash
sudo port selfupdate
sudo port install qt6-qtbase
```

The simulator still requires a previously built copy of `simavr` in the sibling
`simavr` directory.

## Build simavr

From the top-level directory:

```bash
cd simavr
make
cd ..
```

The simulator CMake file searches for `libsimavr` below:

```text
simavr/simavr/obj-*/
```

## Build the Qt simulator

```bash
cd Prusa-MMU-Simulator

rm -rf build/debug

cmake -S . -B build/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/local/libexec/qt6

cmake --build build/debug
```

When Qt 6 is found, two executables are built:

```text
build/debug/prusa_mmu_sim
build/debug/prusa_mmu_sim_gui
```

`prusa_mmu_sim` is the original command-line simulator.

`prusa_mmu_sim_gui` is the new Qt front end.

If Qt is not found, CMake still builds `prusa_mmu_sim` and reports that the GUI
was disabled.

## Build the spooler firmware for debugging

The simulator should normally be used with the firmware Debug ELF:

```bash
cd ../Spooler-Firmware

rm -rf build/debug

cmake -S . -B build/debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug
```

The firmware used by simavr is:

```text
Spooler-Firmware/build/debug/firmware
```

The Debug configuration is expected to define `SPOOLER_DEBUG`, so the firmware
contains its diagnostic output code.

## Run the GUI manually

From `Prusa-MMU-Simulator`:

```bash
./build/debug/prusa_mmu_sim_gui \
  ../Spooler-Firmware/build/debug/firmware
```

The default GDB endpoint is:

```text
localhost:1234
```

The default FIFO remains:

```text
/tmp/prusa-mmu-sim.cmd
```

Therefore terminal commands continue to work while the GUI is open:

```bash
echo 'status' > /tmp/prusa-mmu-sim.cmd
echo 'filament present' > /tmp/prusa-mmu-sim.cmd
echo 'filament absent' > /tmp/prusa-mmu-sim.cmd
echo 'tap middle 500' > /tmp/prusa-mmu-sim.cmd
echo 'tap middle 5500' > /tmp/prusa-mmu-sim.cmd
echo 'stall shuttle' > /tmp/prusa-mmu-sim.cmd
echo 'unstall shuttle' > /tmp/prusa-mmu-sim.cmd
```

## VS Code firmware debugging

Example VS Code files are supplied as:

```text
.vscode/tasks.example.json
.vscode/launch.example.json
```

Copy or merge them into:

```text
.vscode/tasks.json
.vscode/launch.json
```

The supplied workflow performs these operations in order:

1. Configure `Spooler-Firmware/build/debug` with the AVR toolchain.
2. Build the Debug firmware ELF.
3. Configure the simulator Debug build with Qt.
4. Build `prusa_mmu_sim_gui`.
5. Start the Qt simulator with the Debug firmware ELF.
6. Wait until simavr reports that the GDB server is ready.
7. Start `/opt/local/bin/avr-gdb` through the VS Code C/C++ debugger.
8. Attach GDB to `localhost:1234`.

Select the launch configuration:

```text
Spooler Firmware in Qt Simulator
```

The GUI remains responsive while the firmware is stopped at a GDB breakpoint.
The FIFO is also still polled while the AVR is stopped. Commands that only
change simulated inputs can therefore be issued while debugging. Time-based
button taps still require simulated AVR cycles to advance before their timed
press/release phases complete.

## Architecture

```text
                         Qt main thread
                              |
                       queued signals
                              |
                              v
                    SimulationWorker thread
                              |
                 +------------+-------------+
                 |                          |
                 v                          v
        SimulatorController          ControlInterface
                 |                    FIFO commands
                 +------------+-------------+
                              |
                              v
                           MmuBoard
                              |
              +---------------+---------------+
              |               |               |
              v               v               v
           simavr          TMC2130        mechanics
              |
              v
        ATmega32U4 firmware
```

The GUI and FIFO deliberately share the same simulated board. This makes it
possible to operate the simulator interactively and still use shell scripts for
repeatable tests.

## Important source files

```text
src/SimulatorStatus.hh
src/SimulatorController.hh
src/SimulatorController.cpp
src/gui/SimulationWorker.hh
src/gui/SimulationWorker.cpp
src/gui/MainWindow.hh
src/gui/MainWindow.cpp
src/gui/MachineWidget.hh
src/gui/MachineWidget.cpp
src/gui/main_gui.cpp
```

`MachineWidget` performs the custom MMU-style drawing and animation.
`SimulationWorker` owns simavr execution and bridges it to the Qt event-driven
interface.


## v9 GUI initialization safety

The GUI controls remain disabled until the simulated ATmega32U4, firmware, GDB server, and control FIFO initialize successfully. The board and worker control methods also reject input when no valid simavr CPU exists. This prevents a button press from calling `avr_io_getirq()` with a null `avr_t *` after a failed initialization.

If the controls remain disabled, read the status message directly below the machine display or the terminal output; it will report whether the firmware ELF, simavr CPU, or FIFO failed to initialize.

## v10 run versus debug startup

The GUI now has two startup modes.

Normal manual use runs the firmware immediately:

```bash
./build/debug/prusa_mmu_sim_gui ../Spooler-Firmware/build/debug/firmware
```

VS Code/GDB use starts the AVR stopped so the debugger can attach before the
firmware executes:

```bash
./build/debug/prusa_mmu_sim_gui --wait-for-gdb \
  ../Spooler-Firmware/build/debug/firmware
```

The provided VS Code task uses `--wait-for-gdb` automatically. This fixes the
case where the GUI appeared initialized but button taps did nothing because the
AVR cycle counter was not advancing while the CPU was intentionally stopped.
