# Prusa MMU / Spooler Simulator

A board-level simulator for the standalone filament spooler firmware using
`libsimavr`. It runs the firmware ELF on an emulated ATmega32U4 while modeling
the external MMU-board hardware that the spooler firmware depends on.

The simulator is intended for VS Code/GDB debugging and state-machine testing.
It is a functional hardware model, not an electrical or mechanical finite-
element simulation.

## What is modeled

### ATmega32U4 and board I/O

- ATmega32U4 at 16 MHz by default.
- FINDA on PF6.
- Three-button resistor ladder on ADC5.
- Pulley/shuttle STEP PB4, CS PC6, StallGuard PF4.
- Selector/take-up STEP PD4, CS PD7, StallGuard PF1.
- Idler/brake STEP PD6, CS PB7, StallGuard PF0.
- Shared TMC2130 SPI bus.
- Cascaded 74HC595 shift registers on PB5, PC7, and PB6.
- All motor DIR/ENABLE shift-register outputs.
- All five red/green LED pairs.

### TMC2130 behavior

The TMC2130 model now matches the standalone spooler driver's initialization
requirements rather than only the original Prusa firmware behavior.

It models:

- five-byte pipelined SPI reads,
- register writes and stored register values,
- `IOIN` version `0x11` in bits 31:24,
- the required IOIN variant bit 6,
- live STEP, DIR, and enable observations,
- `GSTAT`,
- `DRV_STATUS`,
- `IHOLD_IRUN`, `CHOPCONF`, `COOLCONF`, `GCONF`, `TCOOLTHRS`, `TPWMTHRS`,
  `TPOWERDOWN`, and `PWMCONF` writes,
- missing-driver injection,
- invalid-identity injection,
- simulated short/driver error,
- undervoltage,
- over-temperature prewarning,
- over-temperature.

A normal driver therefore returns an `IOIN` value containing at least:

```text
0x11000040
```

plus live low-order STEP/DIR/enable bits. This is sufficient for the current
`Tmc2130::init()` identity check to succeed.

### Shuttle mechanics and homing

Automatic mechanics are enabled by default.

- Shuttle physical position starts at 1000 simulator steps.
- Inner hard stop defaults to 0 steps.
- Outer hard travel limit defaults to 5200 steps.
- Homing motion toward the inner stop automatically asserts active-low
  StallGuard when the shuttle reaches the hard stop.
- Reversing away from the hard stop automatically releases StallGuard.
- Physical position is clamped at the configured hard limits.
- Manual stall injection remains available for timeout and fault testing.

The automatic hard-stop behavior lets the current spooler firmware execute its
normal Homing -> HomeBackoff -> Ready sequence without manually issuing a stall
command.

### Motor motion

The TMC2130 is configured by the spooler for `CHOPCONF.DEDGE`, so every STEP pin
transition represents one microstep. The simulator therefore counts both rising
and falling STEP transitions when a motor is enabled and not stalled.

The simulator tracks:

- physical motor position,
- total step count,
- enable state,
- logical motion direction,
- StallGuard state.

The three original MMU axes are labeled by their spooler roles:

```text
Pulley   = shuttle / traverse
Selector = take-up spool
Idler    = source-spool brake
```

### FINDA and filament runout

The current spooler uses:

```text
FINDA_HIGH_MEANS_NO_FILAMENT = true
```

Therefore:

```text
PF6 LOW  = filament present
PF6 HIGH = no filament
```

The preferred simulator command is `filament present|absent`; the lower-level
`finda on|off` command remains available to control the raw PF6 level.

An automatic runout can also be scheduled after a specified number of future
take-up motor steps. This is useful for testing transition from `Winding` to
`OutOfFilament` without manually timing a command.

### Buttons

The ADC values match the current standalone spooler `Defaults.hh`:

```text
Right  = ADC count 25   (accepted range 0..50)
Middle = ADC count 90   (accepted range 80..100)
Left   = ADC count 170  (accepted range 160..180)
None   = ADC count 1023
```

This corrects the Left/Right mapping in the earlier simulator.

## Easier button simulation

For normal button testing, use a one-shot `tap` command. The simulator now generates a complete debounced release/press/release sequence using simulated AVR CPU cycles rather than host wall-clock time. By default it forces the button released for 50 ms, presses the selected ADC-ladder button for 100 ms, then holds the released state for another 50 ms. This guarantees the firmware can clear a previous `justPressed()` event before recognizing the next press, even when simavr runs faster or slower than real time under GDB.

```bash
echo 'tap left'   > /tmp/prusa-mmu-sim.cmd
echo 'tap middle' > /tmp/prusa-mmu-sim.cmd
echo 'tap right'  > /tmp/prusa-mmu-sim.cmd
```

An optional press duration can be supplied in milliseconds. The 50 ms pre-release and post-release debounce phases remain in place:

```bash
echo 'tap middle 250' > /tmp/prusa-mmu-sim.cmd
```

`press` is an alias for `tap`. For tests that require a button to remain pressed, use `hold` followed later by `release`:

```bash
echo 'hold right' > /tmp/prusa-mmu-sim.cmd
echo 'release'    > /tmp/prusa-mmu-sim.cmd
```

The original `button left|middle|right|release` command remains available for compatibility and still behaves as a manual press/release interface.

## Build

The simulator requires a built simavr source tree.

```sh
cmake -S . -B build \
  -DSIMAVR_ROOT=/Users/andrepruitt/PrusaMMUFirmware/simavr
cmake --build build
```

## Run

Run it against the standalone spooler ELF:

```sh
./build/prusa_mmu_sim \
  /Users/andrepruitt/PrusaMMUFirmware/Prusa-Spooler-Firmware/build/release/firmware
```

Default endpoints:

```text
GDB:     localhost:1234
Control: /tmp/prusa-mmu-sim.cmd
```

## Control commands

Send commands from another terminal:

```sh
echo 'status' > /tmp/prusa-mmu-sim.cmd
```

### Filament / FINDA

```sh
echo 'filament present' > /tmp/prusa-mmu-sim.cmd
echo 'filament absent'  > /tmp/prusa-mmu-sim.cmd

echo 'finda on'  > /tmp/prusa-mmu-sim.cmd   # raw PF6 HIGH
echo 'finda off' > /tmp/prusa-mmu-sim.cmd   # raw PF6 LOW
```

### Buttons

Hold a button:

```sh
echo 'tap left'   > /tmp/prusa-mmu-sim.cmd
echo 'tap middle' > /tmp/prusa-mmu-sim.cmd
echo 'tap right'  > /tmp/prusa-mmu-sim.cmd
```

Release it:

```sh
# One-shot tap commands release automatically.
# For a deliberate sustained press:
echo 'hold middle' > /tmp/prusa-mmu-sim.cmd
echo 'release'     > /tmp/prusa-mmu-sim.cmd
```

The firmware's own debounce timing remains active, so leave a simulated button
pressed long enough for the firmware to recognize it before releasing it.

### Mechanical model

```sh
echo 'mechanics auto'   > /tmp/prusa-mmu-sim.cmd
echo 'mechanics manual' > /tmp/prusa-mmu-sim.cmd

echo 'shuttle-position 1000' > /tmp/prusa-mmu-sim.cmd
echo 'shuttle-limits 0 5200' > /tmp/prusa-mmu-sim.cmd
```

Manual StallGuard injection:

```sh
echo 'stall shuttle'   > /tmp/prusa-mmu-sim.cmd
echo 'unstall shuttle' > /tmp/prusa-mmu-sim.cmd
```

`pulley`, `selector`, and `idler` are accepted as aliases for `shuttle`,
`takeup`, and `brake` respectively.

### Automatic runout

Trigger no-filament after another 10,000 take-up microsteps:

```sh
echo 'runout after 10000' > /tmp/prusa-mmu-sim.cmd
```

Disable it:

```sh
echo 'runout off' > /tmp/prusa-mmu-sim.cmd
```

### TMC2130 fault injection

Missing driver:

```sh
echo 'tmc-present shuttle off' > /tmp/prusa-mmu-sim.cmd
```

Bad IOIN identity/version:

```sh
echo 'tmc-id shuttle bad' > /tmp/prusa-mmu-sim.cmd
```

Restore normal identity:

```sh
echo 'tmc-id shuttle good' > /tmp/prusa-mmu-sim.cmd
```

Runtime fault/status injection:

```sh
echo 'driver-error takeup'            > /tmp/prusa-mmu-sim.cmd
echo 'driver-ok takeup'               > /tmp/prusa-mmu-sim.cmd
echo 'tmc-undervoltage brake on'      > /tmp/prusa-mmu-sim.cmd
echo 'tmc-prewarn shuttle on'         > /tmp/prusa-mmu-sim.cmd
echo 'tmc-overtemp shuttle on'        > /tmp/prusa-mmu-sim.cmd
```

Inspect a TMC register:

```sh
echo 'tmc-reg shuttle 0x04' > /tmp/prusa-mmu-sim.cmd   # IOIN
echo 'tmc-reg shuttle 0x6c' > /tmp/prusa-mmu-sim.cmd   # CHOPCONF
echo 'tmc-reg shuttle 0x10' > /tmp/prusa-mmu-sim.cmd   # IHOLD_IRUN
```

### Help

```sh
echo 'help' > /tmp/prusa-mmu-sim.cmd
```

## Suggested spooler test sequence

### 1. Driver initialization and automatic homing

Start the simulator with all defaults. Continue the firmware in GDB.

Expected behavior:

```text
Boot
  -> all three TMC IOIN checks succeed
  -> Homing
  -> shuttle physically moves from 1000 toward 0
  -> automatic StallGuard asserts at 0
  -> HomeBackoff
  -> shuttle moves outward by configured backoff
  -> firmware logical shuttle position becomes 0
  -> Ready
```

Use:

```sh
echo 'status' > /tmp/prusa-mmu-sim.cmd
```

to inspect motor enable/direction/positions, StallGuard, TMC identity, FINDA,
and LEDs.

### 2. Driver initialization failure

While the CPU is stopped before `Board::init()` executes:

```sh
echo 'tmc-id shuttle bad' > /tmp/prusa-mmu-sim.cmd
```

or:

```sh
echo 'tmc-present shuttle off' > /tmp/prusa-mmu-sim.cmd
```

Then continue execution. `Board::init()` should return false and the rewinder
should enter its driver-initialization error path.

### 3. Homing timeout

Disable automatic mechanics before homing:

```sh
echo 'mechanics manual' > /tmp/prusa-mmu-sim.cmd
echo 'unstall shuttle'  > /tmp/prusa-mmu-sim.cmd
```

The shuttle will never report its home stall, allowing the 15-second firmware
homing timeout to be tested.

### 4. Outer-limit adjustment

Once Ready, press and release either arrow. The shuttle should first move to its
saved outer limit. Subsequent Left/Right presses should jog the selected outer
limit. Middle should return the shuttle to logical home and start winding when
filament is present.

### 5. Winding

With filament present:

```sh
echo 'tap middle'  > /tmp/prusa-mmu-sim.cmd
# allow debounce
# One-shot tap commands release automatically.
# For a deliberate sustained press:
echo 'hold middle' > /tmp/prusa-mmu-sim.cmd
echo 'release'     > /tmp/prusa-mmu-sim.cmd
```

Expected behavior:

- take-up enabled and stepping,
- shuttle traversing between logical inner/outer limits,
- brake enabled but stationary,
- Left reduces speed level,
- Right increases speed level,
- Middle stops winding and returns to Ready behavior defined by firmware.

### 6. Filament runout

During winding:

```sh
echo 'runout after 5000' > /tmp/prusa-mmu-sim.cmd
```

After the requested take-up motion, PF6 is driven high. After the firmware FINDA
debounce interval, the spooler should stop take-up and shuttle motion, release
the brake, and enter `OutOfFilament`.

Restore filament and acknowledge:

```sh
echo 'filament present' > /tmp/prusa-mmu-sim.cmd
echo 'tap middle'    > /tmp/prusa-mmu-sim.cmd
# allow debounce
echo 'release'         > /tmp/prusa-mmu-sim.cmd
```

## GDB / VS Code

Keep the firmware debugger pointed at the standalone spooler ELF:

```json
"program": "${workspaceFolder}/build/release/firmware",
"MIMode": "gdb",
"miDebuggerPath": "/opt/homebrew/bin/avr-gdb",
"miDebuggerServerAddress": "localhost:1234"
```

A simulator pre-launch task can run the sibling simulator:

```json
{
  "label": "Start MMU Simulator",
  "type": "shell",
  "command": "pkill -x prusa_mmu_sim 2>/dev/null || true; nohup '${workspaceFolder}/../Prusa-MMU-Simulator/build/prusa_mmu_sim' '${workspaceFolder}/build/release/firmware' > /tmp/prusa-mmu-sim.log 2>&1 & sleep 2",
  "problemMatcher": []
}
```

## Scope and limitations

The simulator is designed to test the behavior used by the current spooler
firmware. It does not attempt to reproduce TMC2130 analog current regulation,
real motor inertia, filament elasticity, thermal rise, spool-radius-dependent
brake torque, or missed steps caused by marginal torque.

AVR EEPROM operations are handled by simavr during a simulator process. This
revision does not add a separate host-side EEPROM persistence file across
simulator process restarts. Therefore EEPROM save/load logic can be debugged in
one simulated run, but a true simulator-process power-cycle persistence test
still requires an EEPROM persistence extension.

## v4 command diagnostics

Simulator stdout/stderr are now explicitly unbuffered for both C stdio and C++
iostreams. Each received FIFO command is echoed as `[control] <command>` before
execution, making it easy to distinguish a FIFO/control-loop problem from a
firmware response problem.

When GDB has stopped the AVR at a breakpoint, the FIFO command is not processed
until execution is continued because `control.poll()` runs after `avr_run()`
returns.
