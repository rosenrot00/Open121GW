# Open121GW

Reverse engineering project for the EEVblog 121GW multimeter.

UART provides the feedback loop for testing, debugging and programming the
firmware.

[Download firmware](https://github.com/rosenrot00/Open121GW/releases/tag/development-2026-07-23)

[EEVBlog Discussion](https://www.eevblog.com/forum/blog/eevblog-1783-can-ai-reverse-engineer-the-121gw-multimeter-firmware/)

## Implemented Functionality

- Resistance, capacitance, continuity and diode measurements.
- Selectable 3 V / 15 V diode operation and continuity thresholds of 30 / 300 ohms,
  with above-threshold and below-threshold beeping.
- Low-Z, DC / AC / AC+DC voltage, millivolt and temperature measurements.
- Frequency, period, duty cycle, and current / voltage measurement modes,
  including uA, mA, A and mVA/VA.
- Automatic and manual ranging, REL, HOLD / AutoHOLD, MIN / MAX / AVG and
  AC-voltage peak capture.
- Device calibration data, secondary readings, LCD units and icons, and bargraph.
- Setup menu, backlight control, battery monitoring, lead warnings and automatic
  power-off.
- MEM data logging to SD card in CSV format.

## Added Functionality And Improvements

- A cooperative task scheduler separates input, measurements, display updates,
  auxiliary ADC sampling, logging and communication.
- Faster resistance autoranging uses separate search, settling and precision
  phases, restoring precision settings once the range has been acquired.
- Fast continuity and diode beeping: an early raw-sample indication is confirmed
  or cancelled by the calibrated measurement, before display averaging.
- Continuity retains `dN30`, `dN300`, `UP30` and `UP300` threshold modes. For
  example, a 100-ohm resistor is silent in `dN30` and audible in `dN300`.
- Diode beeping uses voltage thresholds rather than a fixed resistance limit:
  below 0.1 V in 3 V mode and below 1 V in 15 V mode.
- UART diagnostics expose measurement state, HY3131 counters, LCD shadow data,
  individual LCD segment tests, button simulation and scheduler timing.
- A SRAM-based UART firmware updater supports CRC verification, selectable
  transfer speeds and flash writes restricted to the application area.
- Nonblocking UART diagnostics and queued SD writes reduce interference with
  measurement processing.
- Change-aware settings persistence avoids unnecessary EEPROM writes.

Bluetooth command handling and measurement frame generation are implemented.
End-to-end BLE connectivity remains unverified.

The download contains the latest archived task binary. Its final complete
measurement comparison remains open.

## License

Licensed under the GNU General Public License version 3 only (`GPL-3.0-only`).
See [LICENSE](LICENSE) for the full terms.
