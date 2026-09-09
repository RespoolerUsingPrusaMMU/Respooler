# Before You Compile

This document describes the software prerequisites and recommended development environment for building the Prusa MMU Rewinder project on macOS.

The project contains two different kinds of software:

- **Spooler-Firmware** — AVR firmware built for the **ATmega32U4** used by the Prusa MMU2/MMU3 controller.
- **Prusa-MMU-Simulator** — a native macOS Qt application that runs the firmware inside **simavr** and provides a graphical simulation of the rewinder hardware.

The project has primarily been developed and tested on **Apple Silicon Macs**. The paths shown below reflect that environment.

---

## 1. Recommended Directory Layout

The recommended top-level project layout is:

```text
Respooler/
├── .vscode/
│   ├── launch.json
│   └── tasks.json
├── simavr/
├── Spooler-Firmware/
└── Prusa-MMU-Simulator/
```

When using Visual Studio Code, open the top-level `Respooler` directory as the workspace rather than opening either subproject individually.

---

## 2. Xcode Command Line Tools

The native simulator build requires Apple's compiler and standard macOS development tools.

Install them with:

```bash
xcode-select --install
```

Verify that the compiler is available:

```bash
clang --version
```

---

## 3. MacPorts

MacPorts is recommended for most of the required development packages.

Install MacPorts from:

```text
https://www.macports.org/install.php
```

After installation, update the package database:

```bash
sudo port selfupdate
```

Make sure `/opt/local/bin` is in your shell path:

```bash
export PATH=/opt/local/bin:$PATH
```

It is recommended to add that line to:

```text
~/.zshrc
```

---

## 4. CMake

CMake is used to configure both the AVR firmware build and the native Qt simulator build.

Install it with:

```bash
sudo port install cmake
```

Verify:

```bash
cmake --version
```

---

## 5. Ninja

Ninja is the build system used by the supplied CMake build commands.

Install it with:

```bash
sudo port install ninja
```

Verify:

```bash
ninja --version
```

---

## 6. AVR GCC Toolchain

The rewinder firmware is compiled for the ATmega32U4 using the GNU AVR toolchain.

Install:

```bash
sudo port install avr-gcc avr-binutils avr-libc
```

Verify:

```bash
which avr-gcc
which avr-g++
which avr-objcopy
which avr-size
```

A typical MacPorts installation should report paths similar to:

```text
/opt/local/bin/avr-gcc
/opt/local/bin/avr-g++
/opt/local/bin/avr-objcopy
/opt/local/bin/avr-size
```

The firmware build uses the supplied CMake toolchain file:

```text
Spooler-Firmware/cmake/AvrGcc.cmake
```

---

## 7. Qt 6

The graphical Prusa MMU simulator is implemented using Qt Widgets.

Install the Qt 6 base package with:

```bash
sudo port install qt6-qtbase
```

The project currently expects the MacPorts Qt installation under:

```text
/opt/local/libexec/qt6
```

When configuring the simulator, this path is supplied to CMake with:

```bash
-DCMAKE_PREFIX_PATH=/opt/local/libexec/qt6
```

---

## 8. libelf

simavr uses `libelf` when loading AVR ELF firmware files.

Install with:

```bash
sudo port install libelf
```

The simulator CMake configuration searches common library locations, including:

```text
/opt/local/lib
/opt/homebrew/lib
```

On some systems, `libelf` may also be supplied by Homebrew.

---

## 9. simavr

The simulator uses the open-source **simavr** AVR emulator.

The simavr source tree should be located beside the other project directories:

```text
Respooler/
├── simavr/
├── Spooler-Firmware/
└── Prusa-MMU-Simulator/
```

If simavr is not already present, clone it into the top-level project directory:

```bash
cd Respooler
git clone https://github.com/buserror/simavr.git
```

Build simavr before building the Prusa MMU simulator:

```bash
cd simavr
make
```

The Prusa MMU Simulator CMake project expects to find simavr through the relative path:

```text
../simavr
```

---

## 10. AVR GDB

`avr-gdb` is required only if you want to debug the AVR firmware through Visual Studio Code or another GDB frontend.

On the current development system, `avr-gdb` is installed using Homebrew:

```bash
brew install avr-gdb
```

Verify:

```bash
which avr-gdb
avr-gdb --version
```

On an Apple Silicon Homebrew installation, the path is typically:

```text
/opt/homebrew/bin/avr-gdb
```

The VS Code `launch.json` should use the exact path returned by:

```bash
which avr-gdb
```

For example:

```json
"miDebuggerPath": "/opt/homebrew/bin/avr-gdb"
```

If `avr-gdb` is installed somewhere else, update `launch.json` accordingly.

---

## 11. Visual Studio Code

Visual Studio Code is optional for command-line builds, but it is recommended for firmware development and source-level debugging.

Install the Microsoft **C/C++** extension:

```text
ms-vscode.cpptools
```

This extension provides the `cppdbg` debugger used by the project's `launch.json`.

The recommended workspace layout is:

```text
Respooler/
├── .vscode/
│   ├── launch.json
│   └── tasks.json
├── simavr/
├── Spooler-Firmware/
└── Prusa-MMU-Simulator/
```

Open the top-level `Respooler` directory in VS Code.

---

## 12. Verify the Development Environment

Before attempting a build, verify the major tools:

```bash
clang --version
cmake --version
ninja --version

avr-gcc --version
avr-g++ --version
avr-objcopy --version

which avr-gcc
which avr-g++
which avr-gdb
```

For the current Apple Silicon development configuration, the important paths are approximately:

```text
Native compiler:
  /usr/bin/clang
  /usr/bin/clang++

AVR compiler:
  /opt/local/bin/avr-gcc
  /opt/local/bin/avr-g++

Qt 6:
  /opt/local/libexec/qt6

AVR GDB:
  /opt/homebrew/bin/avr-gdb
```

Your exact paths may differ depending on whether you use MacPorts, Homebrew, or another package-management setup.

---

# Building the Firmware

From the top-level repository:

```bash
cd Spooler-Firmware

cmake -S . -B build/debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug
```

The AVR firmware ELF is generated as:

```text
Spooler-Firmware/build/debug/firmware
```

For a release build:

```bash
cmake -S . -B build/release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/release
```

---

# Building the Qt Simulator

Make sure simavr has already been built, then:

```bash
cd Prusa-MMU-Simulator

cmake -S . -B build/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/local/libexec/qt6

cmake --build build/debug
```

The GUI simulator is generated as:

```text
Prusa-MMU-Simulator/build/debug/prusa_mmu_sim_gui
```

---

# Running the Simulator

For normal simulator operation:

```bash
./build/debug/prusa_mmu_sim_gui \
  ../Spooler-Firmware/build/debug/firmware
```

This starts the firmware immediately inside simavr and allows the GUI controls to interact with the simulated rewinder.

For GDB or Visual Studio Code debugging:

```bash
./build/debug/prusa_mmu_sim_gui \
  --gdb-port 1234 \
  --wait-for-gdb \
  ../Spooler-Firmware/build/debug/firmware
```

In this mode, the AVR starts stopped and waits for `avr-gdb` to connect on port `1234`.

---

# Common Build Notes

## Simulator Uses the Native macOS Compiler

The simulator is a native macOS application and should be built with AppleClang.

A correct simulator CMake configuration should identify a compiler similar to:

```text
AppleClang
```

It should **not** use:

```text
avr-gcc
avr-g++
cmake/AvrGcc.cmake
```

Those are only for the firmware build.

## Firmware Uses the AVR Compiler

The firmware build should use:

```text
/opt/local/bin/avr-gcc
/opt/local/bin/avr-g++
```

and the toolchain file:

```text
cmake/AvrGcc.cmake
```

## Clean Builds After Moving Directories

CMake caches absolute paths. If you rename or move one of the project directories, delete the corresponding build directory before reconfiguring:

```bash
rm -rf build/debug
```

Then rerun the appropriate `cmake -S ... -B ...` command.

---

# Quick Installation Summary

For a typical MacPorts-based setup:

```bash
xcode-select --install

sudo port selfupdate

sudo port install \
  cmake \
  ninja \
  avr-gcc \
  avr-binutils \
  avr-libc \
  qt6-qtbase \
  libelf
```

If AVR GDB is needed for debugging:

```bash
brew install avr-gdb
```

Then clone and build simavr:

```bash
git clone https://github.com/buserror/simavr.git
cd simavr
make
```

After these prerequisites are installed, the firmware and simulator can be built using the commands provided above.
