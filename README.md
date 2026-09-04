# RC-Control

Firmware for an Arduino Pro Mini (5V / 16MHz ATmega328P) RC car, read from a hobby RC receiver and driving a TB6612-style motor driver (FNG chip) with dual-speed control and gear-activated lighting.

## Overview

- Reads a standard hobby RC receiver throttle channel pulse (and a separate gear channel) using an interrupt-based pulse-width timer (`EnableInterrupt`).
- Maps the throttle pulse to a 0–255 PWM speed, clamped at the extremes so out-of-range readings snap to full speed instead of dropping out.
- Applies a **median-of-3 dirty-data filter** so one-off corrupt pulse readings (noise/glitches that land inside a valid window) are discarded without causing motor surges, while still responding instantly to a genuine fast trigger throw.
- Lights respond to the gear channel and to throttle direction.

## Versions

The `RC_Car_FNG` folder contains two builds of the sketch:

| File                  | Version | Description                                                       |
|-----------------------|---------|-------------------------------------------------------------------|
| `RC_Car_FNG.ino`      | **v2**  | Current working build: median-of-3 dirty-data filter, retuned throttle ranges/scaling for the new receiver. |
| `RC_Car_FNG_v1.ino`   | **v1**  | Original pre-tuning code kept as a reference/fallback.            |

## Pin wiring

| Arduino pin | Function                              |
|-------------|---------------------------------------|
| 3           | Throttle channel input (PPM pulse)    |
| 2           | Gear channel input (PPM pulse)        |
| 4           | Motor driver standby (STBY)           |
| 7           | Motor driver IN2                      |
| 8           | Motor driver IN1                      |
| 9           | Front light PWM                       |
| 10          | Motor driver PWM (speed)              |
| 5           | Bottom light                          |
| 6           | Rear light                            |
| A3          | Battery voltage sense                 |

## Throttle scaling

- **Backward:** pulse `1060–1439 µs` → speed `(1468 − pulse)/1.55`
- **Neutral:** pulse `1440–1499 µs` → coast
- **Forward:** pulse `1500–1900 µs` → speed `(pulse − 1468)/1.63`

Both branches clamp the computed 0–255 speed to its boundaries, so any pulse beyond the range holds full speed rather than stalling. The `1.96`-style divisor (`travel/255`) keeps the full throw mapped to exactly 255.

## Flashing

Compile with `arduino-cli`:

```
arduino-cli compile --fqbn arduino:avr:pro:cpu=16MHzatmega328 RC_Car_FNG
```

The Pro Mini in this project has no usable serial bootloader sync on the CP2102 adapter, so flashing is done over ISP using a Nano-as-ISP programmer:

```
avrdude -C avrdude.conf -p atmega328p -c avrisp -P COM9 -b 57600 -e -U flash:w:RC_Car_FNG.ino.hex:i
```

> Use `-e` (chip erase) before writing and confirm the `bytes of flash verified` line — a write without erase can silently leave stale firmware on the chip.

## License

Original receiver-reading code from [RCArduino Blog](http://rcarduino.blogspot.com/2012/01/how-to-read-rc-receiver-with.html).
