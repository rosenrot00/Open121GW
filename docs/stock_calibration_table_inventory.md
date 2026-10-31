# Stock calibration/table inventory

This tracks stock EEVblog 121GW calibration tables and whether the custom
firmware uses the same source data. Runtime calibration values live in STM32L1
data EEPROM at `0x08080000`; the firmware should read them there instead of
copying per-device values into code.

| Stock item | Stock address/offset | Custom mapping | Status |
| --- | ---: | --- | --- |
| Mode calibration base offsets | `0x08028EE8` | `src/calibration.c:k_cal_mode_base_offsets` | Matched from IDA. |
| Normal zero/gain/range blocks | EEPROM `base + range * 0x10` | `calibration_stock_value_offset()` and `p10_load_calibration()` | Runtime-loaded from device calibration. |
| Low-Z calibration block | EEPROM VDC/VAC base `+ 0x30` | `calibration_stock_value_offset(mode 0, ..., lowz_alt_path)` | Stock-compatible selection. |
| V AC alt-bank calibration | EEPROM mode 1 block when stock subpath is active | `calibration_stock_value_offset(mode 2, flags bit2)` and runtime `g_voltage_alt_primary_path` | Implemented from `cal_load_mode_range_coefficients`. |
| V/mV alternate measurement block | EEPROM `0x02D0` zero, `0x02D4` gain | `calibration_stock_value_offset(mode 2/4, flags bit0)` | Available for service/debug; runtime only uses it when that stock path is modeled. |
| Threshold words | EEPROM `base + 8 + range * 0x10`, AC current forced to range 1 | `calibration_stock_threshold_offset()` and `p10_load_stock_thresholds()` | Stock-compatible read and clamp. |
| Ohms range 6 extra compensation | EEPROM `0x0178` | `p10_load_calibration()` extra word | Implemented. |
| Frequency reference | EEPROM `0x0104` | `dmm_p10_freq_reference_hz()` | Implemented with stock fallback `49152050`. |
| CAP gain scaling | EEPROM normal gain with scale `/100000`, range 4 option `/10000` | `p10_load_calibration()` | Implemented. |
| Dual-current zero/gain pairs | EEPROM four words in the current block | `p10_load_calibration()` | Implemented. |
| Single-current extra zero markers | EEPROM `0x04D0..0x04F8` | `p10_load_calibration()` | Implemented. |
| Temperature offset | EEPROM `0x02E0` | `temperature.c` / UART `UTEMP` | Implemented. |
| Temperature Type-K table | Stock rodata Type-K table | `src/stock_typek_table.inc` | Implemented. |
| Battery percentage table | Stock rodata battery ADC table | `src/battery.c` | Implemented. |
| Voltage/current secondary correction tables | Stock rodata correction axes/tables | `src/dmm.c` and `src/stock_voltage_alt_tables.inc` | Implemented for known display paths. |

UART service helpers:

- `UCALM <mode> <range> [flags]` dumps stock-selected zero/gain/extra fields
  and threshold words.
- `UCALMW <mode> <range> <field> <flags> <u32>` writes one stock-selected
  32-bit calibration field, skipping EEPROM writes when the value is unchanged.

`flags` bits for `UCALM/UCALMW`:

- bit 0: stock `g_measure_alt_path_b7`, selects EEPROM `0x02D0/0x02D4` for
  mode 2/4 field 0/1.
- bit 1: V AC alt bank active.
- bit 2: V AC alt subpath active, selects the mode 1 calibration block.
- bit 3: Low-Z alternate AC path.
