# Spooler Firmware GoogleTest Suite

This directory contains native macOS GoogleTest tests for the production
`src/app/Rewinder.cc` state machine.

The AVR hardware classes cannot execute directly as a normal macOS process, so
`tests/fakes/` supplies deterministic host-side replacements for:

- `hardware::Board`
- buttons
- FINDA
- LEDs
- shuttle/take-up/brake axes
- AVR EEPROM
- debug UART output

The production `Rewinder.cc`, `Rewinder.hh`, `RewinderState.hh`, and
`Defaults.hh` are used directly.  The fake include directory is placed before
`inc/` only for the unit-test target.

## Install GoogleTest on macOS

Using MacPorts:

```bash
sudo port selfupdate
sudo port install googletest
```

If CMake does not automatically find the MacPorts package, configure with:

```bash
-DCMAKE_PREFIX_PATH=/opt/local
```

## Build and run

From `Spooler-Firmware`:

```bash
rm -rf build/tests

cmake -S tests -B build/tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/local

cmake --build build/tests

ctest --test-dir build/tests --output-on-failure
```

Or run the executable directly:

```bash
./build/tests/spooler_rewinder_tests
```

## Coverage

The suite validates:

- construction and accessors
- driver initialization success/failure
- homing, StallGuard qualification, backoff, and homing timeout
- short and five-second center-button gestures
- Ready -> Winding
- Winding -> Paused -> Winding resume
- filament loss and direct winding recovery after filament restoration
- long-hold re-home
- speed-level limits and winding-rate calculations
- shuttle edge reversal
- outer-limit geometry and EEPROM persistence
- Ready -> AdjustOuterLimit
- outer-limit jogging, timeout, save, home return, and Ready transition
- brake and stop behavior
- every ErrorCode value, including the currently reserved
  `ShuttleUnexpectedStall`
- top-level input/LED servicing
- debug status output

`ShuttleUnexpectedStall` is currently reserved in production code and has no
automatic detection path.  The test therefore invokes the internal `fail()`
method to confirm that this error is latched and that all winding motion is
stopped safely.

The tests expose `Rewinder` private methods only in the test translation unit
using `#define private public`.  This avoids adding test-only methods or friend
classes to the production firmware interface.
