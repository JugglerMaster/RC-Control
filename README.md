# RC-Control

Firmware for Arduino Pro Mini (5V / 16MHz ATmega328P) RC cars, read from hobby RC receivers. The repo covers two motor driver setups:

- **FNG / TB6612** — the primary project (v1 and v2), with a median-of-3 dirty-data filter and retuned throttle scaling.
- **L293D** — a separate 2023 car build with different pin mappings and speed clamping.

## Overview

- Reads a standard hobby RC receiver throttle channel pulse (and a separate gear channel) using an interrupt-based pulse-width timer (`EnableInterrupt`).
- Maps the throttle pulse to a 0–255 PWM speed, clamped at the extremes so out-of-range readings snap to full speed instead of dropping out.
- Applies a **median-of-3 dirty-data filter** so one-off corrupt pulse readings (noise/glitches that land inside a valid window) are discarded without causing motor surges, while still responding instantly to a genuine fast trigger throw.
- Lights respond to the gear channel and to throttle direction.

## Versions

| Folder                              | Version | Description                                                        |
|-------------------------------------|---------|--------------------------------------------------------------------|
| `RC_Car_FNG_v2/`                    | **v2**  | Current FNG build: median-of-3 dirty-data filter, retuned throttle for a new car and receiver. |
| `RC_Car_FNG_v1/`                    | **v1**  | Original 2018 FNG build, heavily modified from the original author's code. |
| `RC_Car_L293D_New_Control_2023/`    |         | Separate 2023 build for a car using an L293D motor driver.        |

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

### L293D build (`RC_Car_L293D_New_Control_2023/`)

A separate 2023 car using an L293D motor driver with two directional PWM pins instead of the FNG's IN1/IN2 + single PWM. Throttle ranges from the original receiver:

- **Backward:** pulse `999–1440 µs` → speed `255 − (pulse − 1000)/1.96`, floor at 50
- **Neutral:** pulse `1440–1560 µs` → coast
- **Forward:** pulse `1560–2100 µs` → speed `(pulse − 1500)/1.96`, snaps to 255 above 210

| Arduino pin | Function                         |
|-------------|----------------------------------|
| A1          | Throttle channel input           |
| A2          | Gear channel input               |
| A6          | Battery voltage sense            |
| 3           | Direction B (motor)              |
| 4           | Battery low LED                  |
| 5           | Direction A (motor)              |
| 6           | Rear light                       |
| 9           | Front left light                 |
| 10          | Front right light                |

## Flashing

The easiest way is to open `RC_Car_FNG_v2/RC_Car_FNG_v2.ino` in the Arduino IDE, select **Arduino Pro or Pro Mini** as the board, **ATmega328P (5V, 16MHz)** as the processor, and hit Upload.

In our case the IDE couldn't sync with the Pro Mini over the CP2102 USB adapter (the bootloader was fine but the adapter had a USB timing issue with optiboot). We used `arduino-cli` to compile and an ISP programmer (Nano-as-ISP) to flash instead.

Compile:

```
arduino-cli compile --fqbn arduino:avr:pro:cpu=16MHzatmega328 RC_Car_FNG_v2
```

Flash over ISP:

```
avrdude -C avrdude.conf -p atmega328p -c avrisp -P COM9 -b 57600 -e -U flash:w:RC_Car_FNG_v2.ino.hex:i
```

> Use `-e` (chip erase) before writing and confirm the `bytes of flash verified` line — a write without erase can silently leave stale firmware on the chip.

## License

This project is licensed under the [MIT License](LICENSE).

Receiver-reading code originally from [RCArduino Blog](http://rcarduino.blogspot.com/2012/01/how-to-read-rc-receiver-with.html), heavily modified.
