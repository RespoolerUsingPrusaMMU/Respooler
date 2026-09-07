# Prusa Spooler Firmware

Firmware for a precision filament rewinder built from a Prusa MMU controller board, its three TMC2130 stepper channels, buttons, LEDs, and FINDA sensor.

## Mechanical mapping

| Original MMU axis | Rewinder function |
|---|---|
| Pulley | Traverse/shuttle |
| Selector | Take-up spool |
| Idler | Supply-spool brake/tension |

The middle button starts/stops winding. While winding, left decreases speed and right increases speed. While stopped, pressing left and right together re-homes the shuttle against the fixed inner edge using StallGuard.

FINDA is interpreted for the rewinder mechanism: the steel ball being close to FINDA means there is **no filament**. This behavior is configurable.

## Directory layout

Expected layout:

```text
/Users/andrepruitt/PrusaMMUFirmware/
├── Prusa-Spooler-Firmware/
└── Prusa-MMU-Simulator/

/Users/andrepruitt/simavr/
```

The spooler source itself uses the following layout:

```text
Prusa-Spooler-Firmware/
├── inc/
│   ├── app/
│   ├── config/
│   ├── hardware/
│   └── motion/
├── src/
│   ├── app/
│   ├── hardware/
│   └── motion/
├── cmake/
├── .vscode/
└── CMakeLists.txt
```

## Coding standards

The spooler-owned C++ code follows these conventions:

- C++17, with GNU extensions enabled to remain compatible with the Prusa MMU HAL and its user-defined unit literals.
- Header files use `.hh` and are stored under `inc/`.
- Source files use `.cc` and are stored under `src/`.
- Include guards are used instead of `#pragma once`.
- Two-space indentation.
- Opening braces are placed on the following line.
- No space is used between `if`, `while`, `for`, and the opening parenthesis.
- Member variables begin with `m`.
- Local variables begin with `t`.
- Function parameters begin with `a`.
- Fixed-width integer types are taken from the AVR-compatible `<stdint.h>` header and are used directly in the global namespace.
- Numeric literals use explicit suffixes where appropriate.
- `auto` is not used.
- Loop indices use descriptive temporary names such as `tIndex`, not `i`.
- Functions with return values use one return statement at the bottom of the function.
- Public interfaces and files use Doxygen-style documentation.

The project is compiled as GNU C++17 (`gnu++17`) so the imported Prusa HAL can use its C++ templates and user-defined literals such as `_mm`, `_mm_s`, and `_deg`.

The imported Prusa HAL remains in its original coding style. It is treated as third-party code by CMake so warnings originating solely from that external source do not obscure spooler warnings.

## Important configuration

Edit `inc/config/Defaults.hh` before operating real hardware. In particular verify:

- `SPOOL_WINDING_WIDTH_MM`
- `SHUTTLE_STEPS_PER_MM`
- `TAKEUP_MOTOR_STEPS_PER_REVOLUTION`
- motor direction constants
- `BRAKE_RUN_CURRENT` and `BRAKE_HOLD_CURRENT`
- winding RPM levels
- FINDA polarity

The default shuttle conversion is the original MMU pulley conversion and may not match the rewinder shuttle mechanics.

## State machine

```text
Power on
   |
   v
HOMING -- StallGuard --> HOME BACKOFF --> READY
                                      \
                                       center + filament
                                             |
                                             v
                                          WINDING
                                             |
                      center ----------------+---- no filament
                         |                          |
                         v                          v
                       READY                 OUT OF FILAMENT

Any initialization/homing failure --> ERROR
```

### Startup homing

1. Pulley/shuttle motor moves toward the fixed inner edge.
2. StallGuard must remain active for several samples.
3. Shuttle position is set to zero.
4. Shuttle backs away by `SHUTTLE_HOME_BACKOFF_MM`.
5. The backed-off location becomes winding position 0.

A timeout prevents endless motion if StallGuard fails.

## Precision winding relationship

The shuttle advance is derived from take-up spool rotation rather than being independently tuned:

```text
shuttle mm/sec = takeup revolutions/sec * WINDING_PITCH_MM
```

and therefore:

```text
shuttle steps/sec =
    takeup steps/sec
    / TAKEUP_MOTOR_STEPS_PER_REVOLUTION
    * WINDING_PITCH_MM
    * SHUTTLE_STEPS_PER_MM
```

This keeps winding pitch constant when the user changes speed.

## Build

Use the toolchain file finds `avr-gcc` on `PATH`.

Example using the Prusa bundled compiler:

```bash
cd /Users/andrepruitt/PrusaMMUFirmware/Prusa-Spooler-Firmware

cmake -S . -B build/release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/release
```

Outputs:

```text
build/release/firmware
build/release/firmware.hex
build/release/firmware.asm
build/release/firmware.map
```

The extensionless `firmware` file is the ELF image containing debug symbols.

## VS Code / simavr

The supplied `.vscode/tasks.json` performs:

1. Configure firmware.
2. Build firmware.
3. Build `Prusa-MMU-Simulator`.
4. Start the simulator with this project's ELF.
5. `launch.json` starts `/opt/homebrew/bin/avr-gdb` and connects to port 1234.

The current MMU simulator can be reused because the rewinder still uses the same controller board and pins. It should next be extended with rewinder mechanics: shuttle travel/end stop, take-up spool rotation, and FINDA behavior.

## User controls

### Ready

- **Short Middle press:** start winding if filament is present.
- **Hold Middle for 5 seconds:** run a complete StallGuard shuttle re-home cycle.
- **Left or Right:** enter outer-limit adjustment mode.

### Winding

- **Short Middle press:** pause winding.  The shuttle position and current traverse direction are retained.
- **Hold Middle for 5 seconds:** stop winding and run a complete StallGuard shuttle re-home cycle.
- **Left:** decrease speed one level.
- **Right:** increase speed one level.

The five green LEDs form a speed-level bar while winding.

### Paused

- **Short Middle press:** resume winding from the exact shuttle position and traverse direction at which winding was paused.
- **Hold Middle for 5 seconds:** discard the paused traverse position and run a complete StallGuard shuttle re-home cycle.

The current green speed bar blinks while paused.

### FINDA

If filament disappears during winding:

- take-up motor stops,
- shuttle stops,
- brake is released,
- red LEDs blink,
- state becomes `OutOfFilament`.

After filament is restored, press the middle button once to acknowledge the condition.  The rewinder enters `Paused` so the traverse position and direction are preserved.  Press Middle again to resume winding from that point.

## Brake caution

The idler motor is held stationary and its TMC2130 holding current provides drag on the supply spool. Start with low current. The same brake torque produces increasing filament tension as the supply spool radius decreases. A dancer arm or tension sensor would be a future improvement if constant tension is required.

## Local TMC2130 wrapper

The rewinder intentionally does not include or link Prusa `hal/tmc2130.h` or
`hal/tmc2130.cpp`. That Prusa header pulls in the complete MMU configuration and
physical-unit system (`config.h`, `axis.h`, and `unit.h`). The rewinder instead
contains its own `hardware/Tmc2130.hh/.cc`, implementing only the TMC2130
features required by this spooler.

## Local MMU hardware abstraction

The rewinder also does not link Prusa MMU HAL object files. It provides its own
`hardware/MmuHal.hh/.cc` implementation for the board functions it uses: GPIO,
SPI, ADC, the cascaded 74HC595 shift registers, and the MMU board pin mapping.
This keeps the application independent of the original Prusa firmware HAL and
avoids the C/C++ linkage and namespace differences encountered when the two
firmware projects were mixed.

The Prusa firmware source tree may still be referenced by the documented build
configuration because its bundled AVR GCC toolchain is convenient for building
the ATmega32U4 firmware. Using that compiler toolchain does not make the Prusa
firmware source part of this project's linked executable.


## v5 AVR register qualifier correction

The local MMU HAL preserves the AVR `volatile` qualifier on all memory-mapped GPIO and SPI register pointers. Hardware pin objects are `static const` rather than `constexpr` because their initialization uses register-address reinterpret casts that are not required to be constant expressions by the AVR GCC 7.3 toolchain.

## Adjustable outer winding limit

The spooler now has a dedicated `AdjustOuterLimit` application state.  From
`Ready`, pressing either arrow moves the shuttle to the last stored outer
winding limit without changing that limit.  Once the shuttle arrives:

- Right moves the outer limit outward by `OUTER_LIMIT_JOG_MM`.
- Left moves the outer limit inward by `OUTER_LIMIT_JOG_MM`.
- The limit is clamped between `MINIMUM_SPOOL_WINDING_WIDTH_MM` and
  `MAXIMUM_SPOOL_WINDING_WIDTH_MM`.
- After `OUTER_LIMIT_TIMEOUT_MS` without another adjustment, the selected limit
  is saved and the shuttle returns to logical position zero.
- Pressing the center/start button saves the limit, returns the shuttle to
  logical position zero, and starts winding after the return completes.

The selected outer limit is stored in AVR EEPROM when the adjustment session
ends.  `eeprom_update_word()` is used so unchanged values do not cause redundant
EEPROM programming.  Winding edge reversal uses the selected limit rather than
the fixed `SPOOL_WINDING_WIDTH_MM` value.

The initial maximum limit remains the existing 60 mm spool-width assumption.
Increase `MAXIMUM_SPOOL_WINDING_WIDTH_MM` only after verifying the shuttle has
sufficient mechanical travel.

## License and Prusa relationship

Copyright (C) 2026 Andre Pruitt. This project is licensed under the GNU General
Public License, version 3, or (at your option) any later version
(GPL-3.0-or-later). See `LICENSE`. Commercial use, including selling hardware
with this firmware installed, is permitted subject to the GPL's distribution,
source-code, notice, and installation-information requirements where applicable.

This project targets hardware derived from or compatible with the Original Prusa
MMU2S/MMU3 controller architecture and was developed with reference to the open
source Prusa MMU firmware and hardware behavior. The current spooler firmware
uses its own application, TMC2130 wrapper, and MMU hardware-abstraction code; it
does not link Prusa firmware HAL object files.

`Original Prusa`, `Original Prusa MMU2S`, and `Original Prusa MMU3` are names and
trademarks associated with Prusa Research a.s. This project is not presented as
an official Prusa Research product. The GPL license covering software source code
does not grant rights to Prusa trademarks, logos, graphics, industrial design,
or other separately licensed assets. Anyone producing a commercial product
should use their own branding and design assets unless they have separate rights
to use Prusa material.

See `NOTICE.md` for a concise statement of attribution, third-party relationship,
commercial-use implications, and trademark considerations.
