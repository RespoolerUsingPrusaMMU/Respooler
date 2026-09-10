# Before You Compile on Ubuntu Linux

This document describes the software prerequisites and recommended development environment for building the Prusa MMU Rewinder project on Ubuntu Linux.

NOTE: These instruction are for ARM64 computers. If we you need to install on an Intel chip you will need to change to Intel versions of the installed software.

The project contains two different kinds of software:

- **Spooler-Firmware** — AVR firmware built for the **ATmega32U4** used by the Prusa MMU2/MMU3 controller.
- **Prusa-MMU-Simulator** — a native Linux Qt application that runs the firmware inside **simavr** and provides a graphical simulation of the rewinder hardware.

The instructions below assume a reasonably current Ubuntu release and use Ubuntu's standard `apt` package manager.

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

## 2. Update Ubuntu Packages

Before installing the development tools, update the package database:

```bash
sudo apt update
sudo apt upgrade
```

The `upgrade` step is optional, but it is generally a good idea on a new development machine.

---

## 3. Basic Build Tools

Install the standard GNU compiler toolchain and common build utilities:

```bash
sudo apt install build-essential
```

This provides tools such as:

```text
gcc
g++
make
```

Verify:

```bash
gcc --version
g++ --version
make --version
```

---

## 4. CMake

CMake is used to configure both the AVR firmware build and the native Qt simulator build.

Install:

```bash
sudo apt install cmake
```

Verify:

```bash
cmake --version
```

---

## 5. Ninja

Ninja is the build system used by the supplied CMake build commands.

Install:

```bash
sudo apt install ninja-build
```

Verify:

```bash
ninja --version
```

---
## Install Git
```bash
sudo apt install git
```

---
## Install the firmware

```bash
 git clone https://github.com/RespoolerUsingPrusaMMU/Respooler.git
```

---

## 6. AVR GCC Toolchain

The rewinder firmware is compiled for the ATmega32U4 using the GNU AVR cross-compilation toolchain.

Install:

```bash
sudo apt install gcc-avr avr-libc binutils-avr
```

Verify:

```bash
which avr-gcc
which avr-g++
which avr-objcopy
which avr-size
```

Typical Ubuntu paths are:

```text
/usr/bin/avr-gcc
/usr/bin/avr-g++
/usr/bin/avr-objcopy
/usr/bin/avr-size
```

The firmware build uses the supplied CMake toolchain file:

```text
Spooler-Firmware/cmake/AvrGcc.cmake
```

If that toolchain file contains hard-coded macOS paths such as:

```text
/opt/local/bin/avr-gcc
```

update it so it either uses the Ubuntu paths:

```text
/usr/bin/avr-gcc
/usr/bin/avr-g++
```

or preferably finds the tools from the system `PATH`.

---

## 7. Qt 6

The graphical Prusa MMU simulator is implemented using Qt Widgets.

Install the Qt 6 development packages:

```bash
sudo apt install qt6-base-dev qt6-base-dev-tools
```

It is also useful to install:

```bash
sudo apt install qt6-tools-dev
```

Verify that Qt 6 is available to CMake:

```bash
cmake --find-package   -DNAME=Qt6   -DCOMPILER_ID=GNU   -DLANGUAGE=CXX   -DMODE=EXIST
```

Unlike the macOS build, Ubuntu normally does not require an explicit `CMAKE_PREFIX_PATH` for Qt when Qt was installed through `apt`.

---

## 8. libelf

simavr uses `libelf` for loading AVR ELF firmware files.

Install the development package:

```bash
sudo apt install libelf-dev
```

It is also useful to have `pkg-config` installed:

```bash
sudo apt install pkg-config
```

Verify:

```bash
pkg-config --modversion libelf
```

If a version number is displayed, `libelf` is available to the build system.

---
## 9. Glut

```bash
sudo apt install freeglut3-dev
```


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

Install the dependencies typically needed to build simavr:

```bash
sudo apt install build-essential libelf-dev pkg-config
```

Then build it:

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

Install:

```bash
sudo apt install gdb-avr
```

Verify:

```bash
which avr-gdb
avr-gdb --version
```

The expected path on Ubuntu is normally:

```text
/usr/bin/avr-gdb
```

The VS Code `launch.json` should therefore normally contain:

```json
"miDebuggerPath": "/usr/bin/avr-gdb"
```

Always verify the actual path with:

```bash
which avr-gdb
```

---

## 11. Visual Studio Code

Visual Studio Code is optional for command-line builds, but it is recommended for firmware development and source-level debugging.

Install Mucrosoft VSCode if its not aready installed

```bash
wget -qO /tmp/code.deb https://update.code.visualstudio.com/latest/linux-deb-x64/stable
sudo apt install /tmp/code.deb
```

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

If your existing `.vscode/launch.json` was created on macOS, change:

```json
"miDebuggerPath": "/opt/homebrew/bin/avr-gdb"
```

to:

```json
"miDebuggerPath": "/usr/bin/avr-gdb"
```

The rest of the debugger configuration can generally remain the same if the project directory layout is unchanged.

---

## 12. Verify the Development Environment

Before attempting a build, verify the major tools:

```bash
gcc --version
g++ --version
cmake --version
ninja --version

avr-gcc --version
avr-g++ --version
avr-objcopy --version
avr-gdb --version

which avr-gcc
which avr-g++
which avr-gdb
```

Typical Ubuntu paths are:

```text
Native compiler:
  /usr/bin/gcc
  /usr/bin/g++

AVR compiler:
  /usr/bin/avr-gcc
  /usr/bin/avr-g++

AVR GDB:
  /usr/bin/avr-gdb
```

Qt and libelf should normally be discovered automatically by CMake when installed through `apt`.

---

# Quick Installation Command

For a typical Ubuntu development machine, most prerequisites can be installed with:

```bash
sudo apt update

sudo apt install   build-essential   cmake   ninja-build   gcc-avr   avr-libc   binutils-avr   gdb-avr   qt6-base-dev   qt6-base-dev-tools   qt6-tools-dev   libelf-dev   pkg-config   git
```

After this command completes, clone and build simavr if it is not already part of your project tree:

```bash
cd Respooler
git clone https://github.com/buserror/simavr.git

cd simavr
make
```

---

# Building the Firmware

From the top-level repository:

```bash
cd Spooler-Firmware

cmake -S . -B build/debug -G Ninja   -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake   -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug
```

The AVR firmware ELF should be generated as:

```text
Spooler-Firmware/build/debug/firmware
```

For a release build:

```bash
cmake -S . -B build/release -G Ninja   -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake   -DCMAKE_BUILD_TYPE=Release

cmake --build build/release
```

---

# Building the Qt Simulator

Make sure simavr has already been built, then:

```bash
cd Prusa-MMU-Simulator

cmake -S . -B build/debug -G Ninja   -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug
```

On Ubuntu, Qt installed through `apt` should normally be found automatically, so the macOS-specific option:

```text
-DCMAKE_PREFIX_PATH=/opt/local/libexec/qt6
```

should not be required.

The GUI simulator should be generated as:

```text
Prusa-MMU-Simulator/build/debug/prusa_mmu_sim_gui
```

---

# Running the Simulator

For normal simulator operation:

```bash
./build/debug/prusa_mmu_sim_gui   ../Spooler-Firmware/build/debug/firmware
```

This starts the firmware immediately inside simavr and allows the GUI controls to interact with the simulated rewinder.

For GDB or Visual Studio Code debugging:

```bash
./build/debug/prusa_mmu_sim_gui   --gdb-port 1234   --wait-for-gdb   ../Spooler-Firmware/build/debug/firmware
```

In this mode, the simulated AVR starts stopped and waits for `avr-gdb` to connect on port `1234`.

---

# Common Ubuntu Build Notes

## Firmware and Simulator Use Different Compilers

The two projects are intentionally built with different compilers.

The firmware uses the AVR cross compiler:

```text
avr-gcc
avr-g++
```

The simulator uses the native Linux compiler:

```text
gcc
g++
```

Do not configure the Qt simulator with:

```text
cmake/AvrGcc.cmake
```

That toolchain file is only for the firmware.

---

## Remove macOS-Specific Paths

If the project was copied from a Mac development system, check the CMake and VS Code configuration files for paths such as:

```text
/opt/local/bin
/opt/local/lib
/opt/local/libexec/qt6
/opt/homebrew/bin
/opt/homebrew/lib
```

Those paths are specific to MacPorts or Homebrew and should normally not be used on Ubuntu.

Typical Ubuntu equivalents are under:

```text
/usr/bin
/usr/lib
/usr/include
```

Whenever possible, allow CMake and the system `PATH` to locate the tools rather than hard-coding platform-specific paths.

---

## Clean Builds When Switching Operating Systems

Do not reuse a `build` directory created on macOS.

CMake build directories contain platform-specific paths, compiler information, and generated build files.

After copying the source tree to Ubuntu, remove old build directories:

```bash
rm -rf Spooler-Firmware/build
rm -rf Prusa-MMU-Simulator/build
```

Then configure both projects again on Ubuntu.

---

## Qt Display Requirements

The simulator is a graphical Qt application and requires a working Linux desktop display.

If you are running Ubuntu Desktop normally, no special configuration should be needed.

If running through SSH, a container, WSL, or another headless environment, additional X11 or Wayland configuration may be required before the GUI can be displayed.

---

# Summary

A typical Ubuntu setup consists of:

```text
GNU C/C++ compiler      -> native simulator build
AVR GCC                 -> ATmega32U4 firmware build
CMake                   -> project configuration
Ninja                   -> build system
Qt 6                    -> simulator GUI
libelf                  -> simavr ELF support
simavr                  -> AVR CPU/peripheral simulation
avr-gdb                 -> firmware debugging
Visual Studio Code      -> optional development/debugging environment
```

Once these prerequisites are installed, both the AVR firmware and the native Qt simulator can be built using the commands above.
