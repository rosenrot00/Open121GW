# EEVblog 121GW Manual Notes

Source: `/Users/taarimi7/Downloads/EEVblog-121GW-Manual.pdf`
Manual revision: 3 March 2025

These notes are a quick implementation reference for UI behavior. Use the
binary/IDA as the authority for low-level register behavior.

## Controls

- `RANGE`: changes function range; in diode mode toggles 3 V / 15 V.
- `REL`: relative measurement; also setup UP key.
- `1ms PEAK`: setup DOWN key; long press toggles Bluetooth data transfer.
- `MODE`: cycles red secondary modes on the dial; long press toggles backlight.
- `SETUP`: opens setup menu and cycles items; long press edits/saves selected item.

## P10 Resistance Switch Position

MODE cycles:

1. Resistance
2. Continuity tester
3. Diode tester
4. Capacitance measurement

Resistance ranges/spec entries:

- 50 ohm, 0.001 ohm resolution
- 500 ohm, 0.01 ohm
- 5 kohm, 0.1 ohm
- 50 kohm, 1 ohm
- 500 kohm, 10 ohm
- 5 Mohm, 0.1 kohm
- 50 Mohm, 1 kohm

## Continuity / Cable Break

Continuity mode is selectable through setup while in continuity:

- `dN 30`: beep for continuity below 30 ohm.
- `dN 300`: beep for continuity below 300 ohm.
- `UP 30`: cable-break mode, beep/open indication above 30 ohm.
- `UP 300`: cable-break mode, above 300 ohm.

Setup flow:

1. Press `SETUP` until `UP xxx` or `dN xxx` appears on the secondary display.
2. Hold `SETUP` until entry flashes.
3. Use UP/DOWN to toggle.
4. Hold `SETUP` until beep to store.

## Capacitance

- Enter by pressing `MODE` three times from resistance.
- Manual says REL can zero residual lead/meter capacitance.
- Open/no capacitor behavior should be checked against stock firmware; manual
  gives ranges but not display text.

Capacitance ranges/spec entries:

- 10 nF, 0.01 nF resolution
- 100 nF, 0.1 nF
- 1 uF, 1 nF
- 10 uF, 10 nF
- 100 uF, 100 nF
- 9999 uF, 1 uF

## Diode

- Diode mode supports 3 V and 15 V voltage modes.
- `RANGE` toggles 3 V / 15 V after diode mode is selected.
- Secondary display shows maximum diode voltage after toggle.
- Test current/spec: 3 V nominal 1.4 mA, 15 V nominal 7 mA.
- Current is limited by nominal 2.2 kohm total PTC + resistor; not constant current.

## REL

- Press `REL` to store the currently displayed value as relative zero.
- Subsequent display is measured value minus stored value.
- Manual says it can be used in all functions, including ohms and capacitance.

## Setup Menu

Order:

1. Temperature
2. Battery Voltage
3. Auto Power Off (`APo`)
4. Buzzer On/Off (`b-OFF` / `b-ON`)
5. LCD Contrast (`LCD X`)
6. Year
7. Month-Day
8. Hour-Minute
9. Multimeter ID
10. Logging Interval (`In x`)
11. Function-dependent settings

General edit flow:

1. Press `SETUP` to navigate.
2. Hold `SETUP` until display flashes.
3. Use UP/DOWN.
4. Hold `SETUP` until no longer flashing / beep to save.

Battery page displays `bAt`; low battery icon at approximately 4.2 V.
Logging interval range is 200 ms to 999 s; `0` means maximum ADC rate, about
200 ms.

## Bluetooth

- Hardware module: BLE112-A Class 2.
- Long press `1ms PEAK` until `BT` is displayed to enable Bluetooth.
- Long press again until `BT` disappears to disable.
- Meter continues normal operation while display data is transmitted.

## Firmware Update

- SD-card firmware update file is `EEVblog.bin`.
- Procedure: power off, insert SD with firmware, hold the documented button
  pair, power on, `IAP` appears, confirm update, meter restarts after complete.
- Manual notes bootloader should be robust, but power/SD removal during update
  should still be avoided.
