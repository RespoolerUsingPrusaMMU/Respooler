# Prusa Spooler Firmware Configuration Guide

This guide describes how to select, measure, and tune the values in `namespace spooler::config` in `inc/config/Defaults.hh`. The values fall into four categories: mechanical calibration, operating behavior, electrical polarity/current, and timing/user-interface settings. Mechanical values should be established from measurements of the actual rewinder. Electrical values should be verified cautiously on the real hardware. Timing values should generally be changed only when there is a clear reason.

## General calibration principles

1. Change one related group of parameters at a time.
2. Establish geometry and motor scaling before tuning speeds.
3. Tune homing before unattended winding.
4. Start motor currents and winding speeds conservatively and increase only as needed.
5. Verify all direction and sensor-polarity constants at low speed before enabling automatic operation.
6. Record the final measured values and the method used to obtain them.

A useful calibration sequence is:

1. Verify motor directions and FINDA polarity.
2. Calibrate `SHUTTLE_STEPS_PER_MM`.
3. Verify `TAKEUP_MOTOR_STEPS_PER_REVOLUTION`.
4. Set spool geometry and edge limits.
5. Tune homing speed, backoff, and StallGuard sample count.
6. Tune outer-limit adjustment behavior.
7. Tune winding RPM levels and winding pitch.
8. Tune brake current for adequate filament tension.
9. Verify button thresholds and debounce timing.
10. Leave scheduler timing unchanged unless the timer implementation is intentionally redesigned.

---

## Filament and spool geometry

### `FILAMENT_DIAMETER_MM`

Current value:

```cpp
constexpr float FILAMENT_DIAMETER_MM = 1.75F;
```

**Purpose**  
Represents the nominal filament diameter.

**Principle**  
For standard FDM filament this is normally 1.75 mm. The actual filament may vary slightly, but this setting should normally represent the nominal filament family rather than every small measured variation.

**How to select it**  
Use the filament manufacturer's nominal diameter. If the machine is later extended to calculate spool capacity, layer buildup, or changing spool radius, this value becomes more important.

**How to adjust it**  
Change it only when using a different nominal filament size, such as 2.85 mm.

**Current code note**  
In the current v8 firmware this constant is defined but is not yet used by the winding calculations. It is retained as a geometry parameter for future calculations.

---

### `WINDING_PITCH_MM`

Current value:

```cpp
constexpr float WINDING_PITCH_MM = 1.80F;
```

**Purpose**  
Defines how far the shuttle moves laterally for each complete revolution of the take-up spool.

The firmware uses:

```text
shuttle speed = take-up revolutions/second
                x WINDING_PITCH_MM
                x SHUTTLE_STEPS_PER_MM
```

**Principle**  
For level winding, the pitch should be approximately one filament diameter. A pitch slightly larger than the filament diameter helps prevent adjacent turns from overlapping or climbing over one another.

For 1.75 mm filament, 1.80 mm is a reasonable starting value.

**How to tune it**

- If adjacent turns overlap, crowd, or ride on top of one another, increase the pitch slightly.
- If visible gaps appear between adjacent turns, decrease the pitch.
- Make small changes, typically 0.02 to 0.05 mm at a time.

A practical tuning range for 1.75 mm filament is approximately 1.70 to 1.90 mm, depending on filament stiffness, guide geometry, and spool surface.

---

### `SPOOL_WINDING_WIDTH_MM`

Current value:

```cpp
constexpr float SPOOL_WINDING_WIDTH_MM = 60.0F;
```

**Purpose**  
Provides the default winding width used when no valid operator-selected outer limit is present in EEPROM.

The default usable outer limit is calculated as:

```text
usable width = SPOOL_WINDING_WIDTH_MM - SHUTTLE_EDGE_MARGIN_MM
```

**Principle**  
This should represent the physical distance between the usable inner and outer spool boundaries before subtracting the safety margin.

**How to measure it**

1. Measure the axial distance between the inside faces of the spool flanges.
2. Use the portion of that width that the filament guide can safely reach.
3. Enter that physical width here.

Because the firmware now allows the operator to adjust and save the outer limit, this value mainly serves as a safe initial/default value.

---

### `SHUTTLE_EDGE_MARGIN_MM`

Current value:

```cpp
constexpr float SHUTTLE_EDGE_MARGIN_MM = 1.5F;
```

**Purpose**  
Keeps the filament guide away from the outer flange wall.

**Principle**  
The guide should reverse before the filament is forced hard against the flange. The margin accounts for guide width, filament bending, mechanical play, and positioning error.

**How to tune it**

- Increase it if filament rubs or piles against the flange.
- Decrease it if too much usable spool width is being left empty.
- Make changes in small increments, such as 0.25 mm.

Do not make the value smaller than the total mechanical uncertainty of the shuttle system.

---

## Motor and motion scaling

### `SHUTTLE_STEPS_PER_MM`

Current value:

```cpp
constexpr float SHUTTLE_STEPS_PER_MM =
  (200.0F * 8.0F / 19.147274F);
```

This is approximately 83.56 steps/mm.

**Purpose**  
Converts commanded shuttle distance in millimeters to motor microsteps.

**Principle**  
The general relationship is:

```text
steps/mm = motor full steps/revolution
           x microsteps/full step
           / linear travel per revolution
```

The current expression assumes:

- 200 full steps/revolution
- 8x microstepping
- 19.147274 mm of shuttle travel per motor revolution

**Recommended calibration procedure**

1. Mark or measure a known shuttle starting position.
2. Command a relatively long move, for example 50 mm.
3. Measure the actual travel with calipers.
4. Calculate:

```text
new steps/mm = old steps/mm x commanded distance / measured distance
```

Example:

```text
old steps/mm        = 83.56
commanded distance  = 50.00 mm
measured distance   = 49.20 mm

new steps/mm = 83.56 x 50.00 / 49.20
             = 84.92 steps/mm
```

Repeat the measurement in both directions. If the result differs significantly by direction, investigate backlash or mechanical compliance rather than averaging a large error into this constant.

---

### `TAKEUP_MOTOR_STEPS_PER_REVOLUTION`

Current value:

```cpp
constexpr float TAKEUP_MOTOR_STEPS_PER_REVOLUTION = 200.0F * 8.0F;
```

Current result: 1600 microsteps/revolution.

**Purpose**  
Converts take-up spool RPM into motor step rate.

**Principle**  
For a directly driven 200-step motor at 8x microstepping:

```text
steps/revolution = 200 x 8 = 1600
```

If gearing is added:

```text
steps/spool revolution = motor steps/revolution x gear ratio
```

where the gear ratio is motor revolutions per spool revolution.

**How to verify it**

Command a known number of motor steps and mark the spool. Verify that 1600 commanded microsteps produce one full spool revolution. If not, include the actual gearbox or pulley ratio.

---

## Homing configuration

### `SHUTTLE_HOME_SPEED_MM_PER_SEC`

Current value:

```cpp
constexpr float SHUTTLE_HOME_SPEED_MM_PER_SEC = 8.0F;
```

**Purpose**  
Sets shuttle speed while searching for the inner hard stop using StallGuard.

**Principle**  
StallGuard needs enough motor load and motion to detect the hard stop reliably, but the shuttle should not impact the stop aggressively.

**How to tune it**

- Start low, around 5 to 8 mm/s.
- Increase if motion is unnecessarily slow and stall detection remains reliable.
- Decrease if the shuttle hits the hard stop too violently, rebounds, or stalls inconsistently.

The best value is the lowest speed that still gives repeatable StallGuard detection with adequate margin.

---

### `SHUTTLE_HOME_BACKOFF_MM`

Current value:

```cpp
constexpr float SHUTTLE_HOME_BACKOFF_MM = 2.0F;
```

**Purpose**  
Moves the shuttle away from the physical hard stop after homing. The firmware then defines this backed-off position as logical position zero.

**Principle**  
Normal winding should never continuously operate against the hard stop.

**How to tune it**

- Increase it if the shuttle can still touch or load the hard stop during normal inward reversal.
- Decrease it if unnecessary spool width is being lost.

A useful value is large enough to clear the end stop plus backlash and mechanical flex, with a small safety margin.

---

### `SHUTTLE_HOME_TIMEOUT_MS`

Current value:

```cpp
constexpr uint16_t SHUTTLE_HOME_TIMEOUT_MS = 15000U;
```

**Purpose**  
Prevents indefinite homing motion if a stall is never detected.

**Principle**  
The timeout should be comfortably longer than the maximum legitimate travel time from the farthest possible shuttle position to the home stop.

Calculate a starting value from:

```text
maximum travel time = maximum travel distance / homing speed
```

Then add margin, typically 50 to 100 percent.

Example for 60 mm of travel at 8 mm/s:

```text
60 / 8 = 7.5 seconds
```

A 15 second timeout therefore provides roughly 2x margin.

**Important**  
This is a `uint16_t` millisecond value. Keep it below 65,536 ms unless the timer representation is redesigned.

---

### `SHUTTLE_STALL_SAMPLES_REQUIRED`

Current value:

```cpp
constexpr uint8_t SHUTTLE_STALL_SAMPLES_REQUIRED = 4U;
```

**Purpose**  
Requires multiple consecutive StallGuard-active observations before accepting the hard stop as home.

**Principle**  
This rejects short electrical or mechanical glitches.

**How to tune it**

- Increase it if occasional false home detections occur.
- Decrease it if valid hard-stop detection is delayed excessively.

The meaning of this value depends directly on how frequently `stallActive()` is sampled. Four samples are useful only if the sampling interval is controlled and understood. If samples are later taken once every 1 ms, a value of 4 corresponds to approximately 4 ms of continuous stall indication.

---

## Adjustable outer-limit configuration

### `OUTER_LIMIT_JOG_MM`

Current value:

```cpp
constexpr float OUTER_LIMIT_JOG_MM = 0.50F;
```

**Purpose**  
Sets the distance moved for each Left or Right adjustment command.

**Principle**  
The jog should be small enough for accurate placement but large enough that adjustment does not require excessive button presses.

**How to tune it**

- Use 0.25 mm for fine adjustment.
- Use 0.5 mm as a good general-purpose value.
- Use 1.0 mm if coarse adjustment is preferred.

---

### `OUTER_LIMIT_ADJUST_SPEED_MM_PER_SEC`

Current value:

```cpp
constexpr float OUTER_LIMIT_ADJUST_SPEED_MM_PER_SEC = 8.0F;
```

**Purpose**  
Sets shuttle speed while moving to or jogging the saved outer limit.

**Principle**  
Adjustment motion should be easy for the operator to observe and should not move quickly enough to create a hazardous flange collision if the configured limit is wrong.

**How to tune it**

Start around the homing speed. Increase only after verifying that the operator can reliably stop and observe the shuttle position.

---

### `MINIMUM_SPOOL_WINDING_WIDTH_MM`

Current value:

```cpp
constexpr float MINIMUM_SPOOL_WINDING_WIDTH_MM = 10.0F;
```

**Purpose**  
Prevents the operator from moving the outer winding edge effectively on top of the inner edge.

The actual minimum outer shuttle position is:

```text
(MINIMUM_SPOOL_WINDING_WIDTH_MM - SHUTTLE_EDGE_MARGIN_MM)
    x SHUTTLE_STEPS_PER_MM
```

**How to select it**  
Choose the narrowest spool that the machine is intended to support. It should still leave enough traverse distance for stable level winding.

---

### `MAXIMUM_SPOOL_WINDING_WIDTH_MM`

Current value:

```cpp
constexpr float MAXIMUM_SPOOL_WINDING_WIDTH_MM = 60.0F;
```

**Purpose**  
Provides a software safety limit that prevents the operator from commanding the shuttle beyond the known mechanical range.

**Principle**  
This should be based on the machine's safe mechanical travel, not merely the widest spool expected to be used.

**Recommended setup**

1. Home the shuttle.
2. Manually determine the maximum safe outward shuttle position.
3. Measure the distance from logical zero.
4. Subtract an additional safety allowance.
5. Set `MAXIMUM_SPOOL_WINDING_WIDTH_MM` so the calculated outer position remains inside that safe travel.

Never increase this value simply because a wider spool is installed unless the shuttle can physically travel there safely.

---

### `OUTER_LIMIT_TIMEOUT_MS`

Current value:

```cpp
constexpr uint16_t OUTER_LIMIT_TIMEOUT_MS = 60000U;
```

**Purpose**  
Returns the shuttle to the home/start position after one minute without an adjustment command.

**How to tune it**  
This is primarily a user-interface preference.

- Shorter values return the shuttle sooner but may interrupt slow adjustment.
- Longer values leave the shuttle at the outer position longer.

Because it is a `uint16_t` millisecond value, 60,000 ms is already close to the 65,535 ms maximum. Use a wider type if a timeout greater than approximately 65 seconds is desired.

---

### `OUTER_LIMIT_EEPROM_MAGIC`

Current value:

```cpp
constexpr uint16_t OUTER_LIMIT_EEPROM_MAGIC = 0xA55AU;
```

**Purpose**  
Identifies whether EEPROM contains a valid saved outer-limit record.

**Principle**  
This is not a calibration value.

**When to change it**  
Normally never. Change it intentionally when the EEPROM data format or interpretation changes and old saved data should be invalidated.

---

## Motor direction conventions

### `SHUTTLE_DRIVER_DIR_ON`
### `TAKEUP_DRIVER_DIR_ON`
### `BRAKE_DRIVER_DIR_ON`
### `TAKEUP_WINDING_DIRECTION_POSITIVE`

Current values:

```cpp
constexpr bool SHUTTLE_DRIVER_DIR_ON = false;
constexpr bool TAKEUP_DRIVER_DIR_ON = true;
constexpr bool BRAKE_DRIVER_DIR_ON = true;
constexpr bool TAKEUP_WINDING_DIRECTION_POSITIVE = true;
```

**Purpose**  
Maps logical directions used by the rewinder to the actual electrical DIR polarity required by each TMC2130 channel.

**Principle**  
These are installation-specific polarity settings.

**Verification procedure**

1. Run at very low speed.
2. Verify shuttle positive direction moves outward, away from logical zero.
3. Verify the take-up spool turns in the intended winding direction.
4. If a direction is reversed, change only the corresponding polarity constant.
5. Re-test homing immediately after changing the shuttle direction polarity.

Do not compensate for incorrect motor direction by changing unrelated signs or edge calculations elsewhere in the application.

---

## Winding speeds

### `SPEED_LEVEL_COUNT`

Current value:

```cpp
constexpr uint8_t SPEED_LEVEL_COUNT = 5U;
```

**Purpose**  
Defines the number of entries in `TAKEUP_RPM`.

**Rule**  
It must always match the number of RPM values in the array.

---

### `DEFAULT_SPEED_LEVEL`

Current value:

```cpp
constexpr uint8_t DEFAULT_SPEED_LEVEL = 2U;
```

**Purpose**  
Selects the zero-based RPM entry used at startup.

With the current array:

```text
index 0 ->  8 RPM
index 1 -> 12 RPM
index 2 -> 18 RPM   <- default
index 3 -> 25 RPM
index 4 -> 35 RPM
```

**How to select it**  
Use a moderate speed that is reliable with a typical spool rather than the fastest available speed.

---

### `TAKEUP_RPM[]`

Current values:

```cpp
constexpr float TAKEUP_RPM[SPEED_LEVEL_COUNT] =
{
  8.0F,
  12.0F,
  18.0F,
  25.0F,
  35.0F
};
```

**Purpose**  
Defines the user-selectable take-up spool speed levels.

**Principle**  
Increasing RPM increases both take-up rotation and shuttle traverse proportionally, so winding pitch remains unchanged. However, filament linear speed increases with spool radius:

```text
filament linear speed = 2 x pi x spool radius x RPM / 60
```

Therefore the same RPM produces a higher filament speed as the take-up spool fills.

**How to tune the RPM table**

1. Begin with a nearly empty take-up spool.
2. Verify the lowest level winds cleanly.
3. Test each higher level.
4. Repeat with a partially full spool, where filament linear velocity is higher.
5. Reduce upper RPM levels if tension becomes excessive, the source spool cannot feed smoothly, or winding quality deteriorates.

The table does not need equal spacing. It is often better to provide closer spacing in the useful operating region.

---

## Brake current

### `BRAKE_RUN_CURRENT`
### `BRAKE_HOLD_CURRENT`

Current values:

```cpp
constexpr uint8_t BRAKE_RUN_CURRENT = 6U;
constexpr uint8_t BRAKE_HOLD_CURRENT = 6U;
```

**Purpose**  
Controls the TMC2130 current applied to the stationary source-spool motor used as a brake.

**Principle**  
More motor current generally produces more holding/braking torque, but also more motor and driver heating. Because the source spool radius decreases during unwinding, a fixed brake torque does not produce constant filament tension:

```text
tension = brake torque / source spool radius
```

Thus filament tension tends to increase as the source spool becomes smaller if brake torque remains fixed.

**Tuning procedure**

1. Start with low current.
2. Wind at the lowest take-up speed.
3. Increase brake current until the filament remains controlled and the source spool does not overrun.
4. Verify operation with both a full source spool and a nearly empty source spool.
5. Check motor and TMC2130 temperature after extended operation.
6. Use the lowest current that provides reliable tension.

If tension varies too much over the source spool diameter range, the better long-term solution is variable braking based on spool radius or a mechanical dancer/tension system rather than simply increasing this constant.

---

## Button input configuration

### `BUTTONS_ADC_CHANNEL`

Current value:

```cpp
constexpr uint8_t BUTTONS_ADC_CHANNEL = 5U;
```

**Purpose**  
Selects the ADC channel used by the original MMU resistor-ladder buttons.

**When to change it**  
Only if the board wiring changes.

---

### `RIGHT_BUTTON_MAX`
### `MIDDLE_BUTTON_MIN`
### `MIDDLE_BUTTON_MAX`
### `LEFT_BUTTON_MIN`
### `LEFT_BUTTON_MAX`

Current values:

```cpp
constexpr uint16_t RIGHT_BUTTON_MAX = 50U;
constexpr uint16_t MIDDLE_BUTTON_MIN = 80U;
constexpr uint16_t MIDDLE_BUTTON_MAX = 100U;
constexpr uint16_t LEFT_BUTTON_MIN = 160U;
constexpr uint16_t LEFT_BUTTON_MAX = 180U;
```

**Purpose**  
Decodes the resistor-ladder ADC reading into Right, Middle, Left, or no-button states.

**Principle**  
Thresholds should be centered around the actual measured ADC values with enough unused space between ranges to tolerate resistor tolerance, supply variation, ADC error, and noise.

**Recommended calibration procedure**

1. Add temporary diagnostic output or use a debugger to read the raw ADC value.
2. Record at least 20 readings for each button.
3. Record readings with no button pressed.
4. Determine the minimum and maximum observed value for each button.
5. Add a reasonable margin around each cluster while keeping ranges separated.

Example:

```text
Right readings:   21 to 27
Middle readings:  88 to 92
Left readings:   168 to 172
```

Suitable windows might be:

```text
Right:    <= 40
Middle:   80 to 100
Left:    160 to 180
```

Avoid making the ranges so wide that noise or simultaneous-button resistor combinations can be mistaken for a valid single button.

**Important current limitation**  
The present decoder returns only one button state. A simultaneous Left+Right press cannot be represented unless its ADC value is separately measured and a dedicated combined state is added.

---

### `BUTTON_DEBOUNCE_MS`

Current value:

```cpp
constexpr uint16_t BUTTON_DEBOUNCE_MS = 30U;
```

**Purpose**  
Requires a decoded button state to remain stable before the application accepts the change.

**How to tune it**

- Increase if one physical press occasionally produces multiple events.
- Decrease if button response feels sluggish.

Typical mechanical-button debounce values are approximately 10 to 50 ms. The current 30 ms is a reasonable starting point.

---

## FINDA configuration

### `FINDA_DEBOUNCE_MS`

Current value:

```cpp
constexpr uint16_t FINDA_DEBOUNCE_MS = 80U;
```

**Purpose**  
Requires the FINDA filament state to remain stable before the firmware declares a change.

**Principle**  
This should suppress brief mechanical movement or electrical noise without delaying genuine filament-loss detection too much.

**How to tune it**

- Increase if filament vibration produces false transitions.
- Decrease if filament-loss response is too slow.

Because a false loss stops winding, reliability is generally more important than shaving a few tens of milliseconds from the response.

---

### `FINDA_HIGH_MEANS_NO_FILAMENT`

Current value:

```cpp
constexpr bool FINDA_HIGH_MEANS_NO_FILAMENT = true;
```

**Purpose**  
Defines the electrical polarity of the FINDA input.

**Verification procedure**

1. Read the FINDA input with no filament present.
2. Insert filament and read it again.
3. Confirm which electrical state corresponds to no filament.
4. Set the constant accordingly.

This should be verified before testing automatic winding because an inverted setting can cause incorrect out-of-filament behavior.

---

## Scheduler and display timing

### `STEP_SCHEDULER_HZ`

Current value:

```cpp
constexpr uint32_t STEP_SCHEDULER_HZ = 20000UL;
```

**Purpose**  
Defines the frequency of the step-generation scheduler.

**Current architectural constraint**  
`MotionController.cc` contains a compile-time assertion requiring this value to be exactly 20 kHz. The Timer1 implementation is configured around that frequency.

**Recommendation**  
Do not treat this as a normal tuning parameter. Leave it at 20,000 Hz unless the Timer1 configuration and scheduler mathematics are intentionally redesigned together.

---

### `LED_BLINK_PERIOD_MS`

Current value:

```cpp
constexpr uint16_t LED_BLINK_PERIOD_MS = 500U;
```

**Purpose**  
Controls the complete LED blink cycle used for status/error indication.

**How to tune it**  
This is a visual preference. A 500 ms period corresponds to two complete blink cycles per second. Increase it for slower blinking or decrease it for faster blinking.

Avoid extremely small values that make the LED appear continuously dim rather than visibly blinking.

---

## Recommended commissioning values and order

The current defaults are appropriate as initial engineering values, but they should be commissioned in this order:

| Parameter group | Establish by | Main risk if wrong |
|---|---|---|
| Direction constants | Low-speed observation | Motion in the wrong direction |
| FINDA polarity | Direct sensor test | False filament-present/lost state |
| `SHUTTLE_STEPS_PER_MM` | Measured commanded travel | Incorrect spool width and pitch |
| `TAKEUP_MOTOR_STEPS_PER_REVOLUTION` | One-revolution test | Incorrect RPM and traverse rate |
| Spool width / margins | Mechanical measurement | Flange collision or unused spool area |
| Homing settings | Repeated home tests | Hard impacts or false home |
| Outer-limit settings | Operator adjustment test | Unsafe or awkward adjustment |
| RPM table | Winding tests at multiple spool radii | Excess speed/tension |
| Winding pitch | Observe adjacent turns | Gaps or overlapping turns |
| Brake current | Tension and temperature testing | Slack, excessive tension, overheating |
| Button thresholds | Raw ADC measurements | Incorrect button decoding |
| Debounce values | Repeated switch/sensor tests | False or sluggish events |
| Scheduler frequency | Normally leave fixed | Broken step timing |

## Final validation checklist

Before unattended operation, verify all of the following:

- Homing succeeds repeatedly from several starting positions.
- The shuttle always backs away from the hard stop and establishes logical zero correctly.
- Positive shuttle motion is outward.
- The saved outer limit never exceeds safe mechanical travel.
- A corrupted or blank EEPROM falls back to a safe default outer limit.
- Left and Right adjustment moves occur in the expected directions.
- The 60-second inactivity timeout returns the shuttle to zero.
- Pressing Start while adjusting returns the shuttle to zero before winding begins.
- The shuttle reverses cleanly at both the inner and outer limits.
- Filament lays down next to the previous turn without excessive overlap or gaps.
- The source spool does not freewheel or produce excessive tension.
- Brake motor and TMC2130 temperatures remain acceptable during extended operation.
- FINDA reliably stops winding when filament is lost.
- Each button produces exactly one intended event per press.
- All winding speed levels are safe with both nearly empty and nearly full take-up spools.
