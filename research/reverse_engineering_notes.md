# EEVBlog2_05 Firmware Reverse Engineering Notes

Target files:

- Firmware binary: `/Users/taarimi7/Downloads/EEVBlog2_05.bin`
- IDA database: `/Users/taarimi7/Downloads/EEVBlog2_05.bin.i64`
- Custom firmware bring-up project: `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw`

Address notes:

- IDA code segment: `0x08006000..0x080294e8`
- IDA VA to firmware-file offset: `file_offset = VA - 0x08006000`
- Calibration flash is accessed at `0x08080000` and is not inside the 141 KB firmware image.

## Custom Firmware Bring-Up Status

- A first C firmware base exists in `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw`.
- It links as an app image at `0x08006000` and leaves the factory boot/update area at `0x08000000..0x08005FFF` untouched.
- The vector table mirrors the stock initial MSP value `0x20001240`.
- Implemented so far: reset/runtime startup, 32 MHz HSE/PLL clock setup, LSE/RTC LCD clock-domain setup, 1 ms SysTick, read-only calibration access at `0x08080000`, minimal LCD blank/init, buzzer PE7, frontend GPIOG bit-bang primitives, recovered diode frontend table, and USART2 Bluetooth frame shell at 19200 baud.
- Normal bring-up build with LSE LCD blank init and blocking boot beep: `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw/build/normal/eevblog121gw_fw.bin`, size `2572` bytes, SHA-256 `d0356b657ee6494c1d48ce4d671825e0a1c126a261f84aff7664789a7c422c76`.
- LCD-only LSE blank test with blocking boot beep and forced buzzer-off loop: `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw/build/lcd_only/eevblog121gw_fw.bin`, size `1932` bytes, SHA-256 `bca7d554b84272430bddbcdf1fa26af365eb7da7b1171577a1ab6f1f50362dc9`.
- LCD text renderer test (`HELLO` on positions 0..4): `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw/build/lcd_text/eevblog121gw_fw.bin`, size `2828` bytes, SHA-256 `db38cfe3f2cd26f4d66f1b2f08618288ad23a40f009d16a9f3ad845ee1e01cbf`.
- Input scan test (`P-xx` rotary one-hot on upper display; lower display shows `J--`/`JA-`/`J-B`/`JAB`, or `B-xx` while PH8..PH15 buttons are active): `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw/build/input_test/eevblog121gw_fw.bin`, size `3310` bytes, SHA-256 `3d8ba00ee9358f1006ef0d78f04ec0bb7156c3cfad7572375623cc2637204fdd`.
- Hardware result: input test shows `JAB` and expected rotary/button states on the meter, so LCD text, rotary GPIOH0..7, jack sense GPIOE10/11 and upper GPIOH8..15 button matrix reads are confirmed at bring-up level.
- Diode frontend status test: `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw/build/diode_fe_test/eevblog121gw_fw.bin`, size `4198` bytes, SHA-256 `0cc160e1a3eb4bddeb3c6aaa12bb8761dc409ba2ec6d6f7698f8ff93f9025b18`. It configures the recovered diode frontend table only in rotary position `P-10`, shows `DIOD`/`S-xx`, and emits a short beep only if the raw decoded sample already matches the diode threshold range.
- P-10 switch-position build: `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw/build/diode_mode/eevblog121gw_fw.bin`, size `8356` bytes, SHA-256 `109c664909b9a44990dd1056af5502b45dd841b0d9f411bd53b38f5eb049d19d`. It now starts like stock in mode `9` (`OHMS`) with range `6`; MODE (`PH8`, key mask `0x01`) cycles the original P-10 sequence `9 -> 10 -> 11 -> 12 -> 9`. The primary display path is stock-oriented: no custom mode text in the upper field, blank while settling, `OL` for overload, recovered decimal positions, P-10 mode/unit/scale annunciators, original Ohm smoothing, high-Ohm correction tables, continuity hysteresis options, stable autorange voting/blanking, and stock-like calibration reads for Ohms, continuity, diode, and capacitance. The fast diode beep remains separated from the display path for now.
- This is not yet a complete replacement DMM firmware. Missing pieces include LCD segment map/init, rotary/buttons, power management, jack warning, full mode/range state machine, calibrated scaling, display formatting, and full Bluetooth payload semantics.

## High-Level Architecture

- `Reset_handler` (`0x8029174`) calls `system_clock_reset_and_vector_init`, then `runtime_entry_after_reset`.
- `runtime_entry_after_reset` (`0x802910C`) runs static initializers and enters `firmware_main_loop`.
- `firmware_main_loop` (`0x8024B40`) initializes board, display, DMM runtime state and then services flags set by interrupts.
- `TIM2_periodic_scheduler_irq` (`0x802428C`) is the main periodic scheduler interrupt. It sets tick flags such as `0x200080D0`, `0x200080D1`, `0x200080D3`, etc.
- `TIM3_buzzer_duration_irq_handler` (`0x8024722`) owns buzzer duration and pulse-pattern timing.
- `USART_or_serial_irq_handler` (`0x80247D8`) handles a serial peripheral at `0x40004400`, receiving frames beginning with `0xF4` or `0xF8`.

## System Clock / Timing

- `system_clock_config_hse_pll_32mhz` (`0x8025FC2`) is the actual clock setup called by `system_clock_reset_and_vector_init`.
- The firmware enables HSE and waits for HSERDY, then writes RCC_CFGR bits `0x00910000` before enabling PLL and selecting PLL as SYSCLK.
- `rcc_get_clocks_freq` (`0x801FE2A`) decodes the clock tree with constants `HSI=16 MHz`, `HSE=8 MHz`, `rcc_pll_mul_factor_table={3,4,6,8,12,16,24,32,48}` and divider `((RCC_CFGR & 0x00C00000) >> 22) + 1`.
- Therefore `0x00910000` decodes as `HSE 8 MHz * 12 / 3 = 32 MHz`.
- AHB, APB1 and APB2 prescalers are left at divide-by-1, so `SYSCLK=HCLK=PCLK1=PCLK2=32 MHz`.
- Flash/PWR are configured for high-speed operation: FLASH_ACR bits `0x1|0x2|0x4` are set and PWR_CR is written with `0x0800`.
- `TIM2_scheduler_init` (`0x801C486`) uses PSC `0x0104` and ARR `0x00A0`. With APB1 at 32 MHz, its update rate is about `761.7 Hz` (`~1.31 ms` tick).
- `TIM3_buzzer_timer_init` (`0x801C4D4`) uses PSC `0x0104` and ARR `0x000F`, about `7663 Hz`.
- `bt_uart_init_19200` (`0x801C390`) relies on `rcc_get_clocks_freq`; at PCLK1 `32 MHz`, USART BRR for 19200 baud is computed to about `0x0683`.
- The firmware is already running at the normal top clock for an STM32L1-class part. Making the meter respond faster should target measurement averaging, frontend conversion timing and UI delays, not CPU overclocking.

## Board / Peripheral Init

- `board_low_level_init` (`0x801BE76`) handles PWR/flash status, peripheral clocks, GPIO setup, external/display init, TIM2 and TIM3 setup.
- `gpio_board_pin_init` (`0x801C056`) configures most GPIOA-H pins and their default BSRR/BRR states.
- `TIM2_scheduler_init` (`0x801C486`) configures TIM2 at `0x40000000`.
- `TIM3_buzzer_timer_init` (`0x801C4D4`) configures TIM3 at `0x40000400`.
- `USART2_frontend_serial_init` (`0x801C390`) configures `0x40004400` at 19200 baud and enables interrupt mask `0x525`.
- `display_lcd_controller_init` (`0x801B450`) initializes the LCD/display controller path.
- `rcc_ahb1_peripheral_clock_cmd` (`0x801FF80`) toggles `RCC_AHB1ENR` bits at `0x4002381C`; used for GPIO clocks.
- `rcc_apb1_peripheral_clock_cmd` (`0x801FFBC`) toggles `RCC_APB1ENR` bits at `0x40023824`; used for LCD/TIM/USART clocks.
- `gpio_apply_pin_config_mask` (`0x801BD48`) applies MODER/OTYPER/OSPEEDR/PUPDR settings to a masked set of GPIO pins.
- `gpio_set_pin_alternate_function` (`0x801BE04`) writes one GPIO AFRL/AFRH alternate-function nibble.

## Frontend Measurement Path

The STM32 internal ADC block is not obviously used. Raw measurement values are read from an external frontend/ADC-like device over bit-banged GPIOG.

- `measurement_poll_frontend_ready` (`0x8006124`) checks GPIOG IDR bit 3 and polls the frontend when ready.
- `frontend_read_status_and_dispatch` (`0x800614E`) reads 52 bytes into `0x200077D0`, reads status register 30, and dispatches measurement handlers when status bit 2 says a new sample is available.
- `frontend_gpio_read_registers` (`0x800C0C0`) reads frontend registers through GPIOG bit-banging.
- `frontend_gpio_write_registers` (`0x800C03C`) writes frontend registers through GPIOG bit-banging.
- `frontend_gpio_shift_read_byte` (`0x800C138`) reads 8 bits using GPIOG clock/data lines.
- `frontend_gpio_shift_write_byte` (`0x800C16C`) writes 8 bits using GPIOG clock/data lines.
- `frontend_decode_signed24_sample` (`0x80062F8`) builds a signed 24-bit value from `0x200077D0..0x200077D2`.
- `measurement_accumulate_average` (`0x8006342`) accumulates samples and emits a valid averaged value at `0x20007DD4`.
- `measurement_apply_offset_and_gain` (`0x8006418`) applies zero/gain calibration or calibration-collection paths.

Frontend status/event handling after each 52-byte read:

- `0x2000807F = rx[0x1E] & rx[0x1F]`.
- `0x2000807D = rx[0x20]`; this is the shadow of frontend register `0x20`.
- `0x2000807E = rx[0x29]`; this is used for an auxiliary event path.
- If `0x2000807F & 0x04`, `dmm_measurement_mode_dispatch` runs the normal measurement path.
- If `0x2000807F & 0x10`, `frontend_status_bit4_event_handler` (`0x8008208`) decodes another raw value and updates per-mode status hooks.
- If `0x2000807E & 0x04` and voltage alternate bank is not active, `frontend_aux_channel_status_event` (`0x800A7FC`) decodes auxiliary 24-bit values from `rx[0x0E..0x13]`, toggles frontend register `0x29` bit 2, and updates alternate/calibration data for modes 2/4.
- If `0x2000807F & 0x01`, `frontend_status_bit0_range_event` (`0x800AA00`) runs range/control update routines and sets UI/update flags.

Likely GPIOG bit roles:

- PG0: clock pulse (`0x40021818/1`, `0x4002181A/1`)
- PG2: data out / command bit (`0x40021818/4`, `0x4002181A/4`)
- PG3: data ready / data input (`0x40021810 & 8`)
- PG1: chip select or transaction framing (`0x40021818/2`, `0x4002181A/2`)

## Frontend Configuration

- `frontend_configure_for_mode_range` (`0x800B974`) is the mode/range reconfiguration wrapper.
- Reconfiguration sequence:
  - `frontend_build_and_write_mode_config` (`0x800B994`) builds 20 bytes and writes them to frontend registers `0x20..0x33`.
  - `frontend_write_mode_control_reg1e` (`0x800BEAC`) writes two bytes to frontend register `0x1E`; the second byte is a mode profile (`1`, `4`, `5`, or `17` observed).
  - `frontend_restart_conversion_timing` (`0x800BF8C`) temporarily clears bits in frontend register `0x20`, writes three bytes at register `0x1B`, reads them back, then sets register `0x20` bit 1.
- `g_frontend_timing_word` (`0x2000001C`) is written into frontend register `0x1B` by `frontend_restart_conversion_timing`.
- `sub_800ED64` periodically toggles `g_frontend_timing_word` between `0x8000` and `0xE000` for selected voltage/resistance/current-like modes, then re-runs `frontend_configure_for_mode_range`.
- `g_frontend_reg20_shadow` (`0x2000807D`) stores byte 0 of the current 20-byte frontend config and is reused when clearing/setting register `0x20` bits.

Known 20-byte frontend config tables:

- `frontend_cfg_table_primary_voltage` (`0x80288A0`): 5 rows before the next known table; used by mode 1, mode 2 alternate cases, and some extended voltage-like modes.
- `frontend_cfg_table_secondary_voltage` (`0x80289C8`): first 4 rows are used as 20-byte frontend configs; following bytes include other constants.
- `frontend_cfg_table_alt_voltage` (`0x8028904`): first 4 rows are voltage alternate configs; following bytes include other constants.
- `frontend_cfg_table_mode9` (`0x80285BC`): at least rows 0..6 are used; mode 9 also has a special `range == 6` bit clear in register image byte 10 when `0x20007FE0` is set.
- `frontend_cfg_table_mode12_option` (`0x80286D0`): 6 rows.
- `frontend_cfg_table_mode12_default` (`0x8028748`): first 6 rows are configs; bytes after that decode as code/other constants before `0x80288A0`.
- `frontend_cfg_table_mode3_or_5` (`0x8028F1C`): 2 rows.
- `frontend_cfg_table_resistance_default` (`0x8028F44`): 2 rows.
- `frontend_cfg_table_resistance_alt` (`0x8028F6C`): 2 rows.
- `frontend_cfg_table_mode11_diode` (`0x8028F94`): 2 identical rows; this is the diode frontend setup.
- `frontend_cfg_table_modes6_8` (`0x8028FBC`): first 2 rows are configs; following bytes include code/other constants.
- `frontend_cfg_table_mode10` (`0x8029150`): 1 row before reset/runtime code.
- `frontend_cfg_table_current_temp_shared` (`0x8028D1C`): first 9 rows look like frontend configs; later rows are other constants.

## Measurement Mode Dispatch

- `g_dmm_mode` is at `0x2000803C`.
- `g_dmm_range` is at `0x2000803D`.
- `dmm_measurement_mode_dispatch` (`0x80061E4`) selects mode handlers.
- Modes `13..15` use `dmm_dual_bank_modes13_15_handler` (`0x8008B8C`).
- Modes `22..24` use `dmm_dual_bank_modes22_24_handler` (`0x80084E0`).
- The dual-bank handlers alternate `0x200080C4` between two frontend configurations, collect two measured values, support calibration collection for both, then combine/post-process them.
- Mode `0x0B` is diode mode and calls `dmm_diode_mode_update_and_beep` (`0x8007E84`).
- Diode beep condition: averaged sample ready, converted value `< 1000` and `>= -999`, then `buzzer_on_for_ticks_force(500)`.
- First-beep delay is dominated by `measurement_accumulate_average`: diode falls through to default average count `16`.

Mode groups from rotary-position restore functions:

- Rotary position `0x02` / voltage group: modes `1`, `2`.
- Rotary position `0x04` / resistance group: modes `3`, `4`, `5`.
- Rotary position `0x08` / uA current group: modes `6`, `7`, `8`.
- Rotary position `0x10` / continuity-diode group: modes `9`, `10`, `11`, `12`.
- Rotary position `0x20` / temperature group: modes `24`, `15`.
- Rotary position `0x40` / mA current group: modes `17`, `16`, `13`, `22`.
- Rotary position `0x80` / A current group: canonical modes `19`, `18`; persisted modes `20`, `21` are folded back to `18`/`19` during restore.
- Mode `0` is used by the dispatcher but is not one of the direct rotary-position defaults found so far; it selects either the mode-2-style path or mode-1-style path depending on `0x20008098`.

## Rotary / UI

- `rotary_switch_sample_gpioh_active_low` (`0x8014A14`) reads GPIOH IDR (`0x40021C10`) and builds an active-low one-hot position mask.
- `rotary_switch_debounce_update` (`0x801496E`) accepts a new rotary position after 5 stable samples and validates one-hot.
- `rotary_switch_apply_position_mode` (`0x80143EE`) maps the stable rotary position to `g_dmm_mode`.
- Rotary position `0x10` maps to modes `9, 10, 11, 12`; mode `11` is diode.
- `mode_cycle_secondary_function` (`0x8014500`) cycles submodes. For the diode group: `9 -> 10 -> 11 -> 12 -> 9`.
- Front-panel key mask `0x01` (GPIOH8 active-low) is MODE; short press dispatches to `mode_cycle_secondary_function` through `ui_handle_key_event` (`0x8014C3C`).
- Front-panel key mask `0x10` is RANGE in normal mode handling; in diode mode the stock firmware repurposes it for a diode option page, but in the current custom P-10 test build it cycles OHMS/CAP range manually.
- `persist_last_mode_for_rotary_position` (`0x8011118`) stores the last submode for each physical switch position into calibration flash.
- `range_button_handle_manual_autorange` (`0x8014796`) handles range button/manual range changes.
- `input_jack_detect_and_lead_warning_service` (`0x801417C`) debounces GPIOH IDR bits 10/11 as a jack/lead-detect state. It can switch paired current/temp modes between their jack-dependent variants, resets measurement state when such a switch happens, and sets `g_lead_warning_active` (`0x20008076`) when the lead combination is invalid for the active mode.
- `set_lead_warning_active` (`0x80143E4`) sets `g_lead_warning_active`. Main-display overflow/open renderers then show `" LEAD"` instead of the normal message.

## Display / LCD

The display uses the STM32 LCD controller, not a separate display serial protocol. The firmware keeps a software shadow buffer and commits it into LCD RAM.

- `display_lcd_controller_init` (`0x801B450`) configures the LCD GPIO segment pins, enables the APB1 LCD clock, configures LCD CR/FCR fields, applies contrast, waits for LCD status flags, optionally clears the shadow buffer, then commits LCD RAM.
- `lcd_gpio_segment_pins_init` (`0x801B4D6`) configures the GPIO pins used as LCD segment/common outputs.
- `lcd_configure_clock_duty_bias` (`0x80203B4`) writes STM32 LCD clock divider/prescaler and duty/bias/mux fields.
- `lcd_set_controller_enable` (`0x80203FA`) writes LCD_CR bit 0 through bit-band alias `0x42048000`.
- `lcd_set_mux_segment_enable` (`0x802041E`) writes LCD_CR bit 7 through bit-band alias `0x4204801C`.
- `lcd_wait_fcr_sync_or_timeout` (`0x8020402`) waits for LCD_SR bit `0x20` with a timeout.
- `lcd_set_pulse_on_duration_bits` (`0x8020426`), `lcd_set_dead_time_bits` (`0x802043E`), `lcd_write_contrast_bits` (`0x8020472`) and `lcd_set_blink_config_bits` (`0x8020456`) wrap LCD_FCR bitfields.
- `lcd_set_contrast_level` (`0x801B708`) maps saved/display contrast levels `10..17` to LCD contrast bits `0x0000..0x1C00`.
- `lcd_apply_saved_contrast_setting` (`0x801B77A`) reapplies `g_lcd_contrast_setting` (`0x20000237`).

LCD RAM shadow mapping:

- `g_lcd_shadow_ram0..7` are at `0x20007790..0x200077AC`.
- `lcd_clear_shadow_buffer` (`0x801A81C`) clears 16 dwords from `0x20007790` and marks `g_lcd_dirty_flag`.
- `lcd_commit_shadow_buffer` (`0x801B304`) copies shadow words 0..7 to LCD RAM registers `0x40002414..0x40002430`, then calls `lcd_request_display_update` (`0x802048A`).
- `lcd_commit_partial_shadow_buffer` (`0x801B396`) updates a masked subset of LCD RAM.

Character and numeric rendering:

- `lcd_char_to_segment_mask` (`0x801AFFC`) maps ASCII to compact segment masks. It uses `lcd_digit_segment_masks` (`0x8029184`), `lcd_uppercase_segment_masks` (`0x80290B8`) and `lcd_lowercase_segment_masks` (`0x80290D4`); explicit specials are space=`0`, `-`=`0x04`, `=`=`0x82`, `0xFF`=`0xBF`.
- `lcd_draw_char_at_position` (`0x801A950`) expands one ASCII character into one of ten LCD character positions by distributing segment mask bits into the LCD shadow words.
- `lcd_write_5chars` (`0x8019AE0`) writes five characters either to positions `0..4` or `5..9`.
- `display_render_numeric_value` (`0x8017100`) sets sign/decimal state, splits the absolute value into five decimal digits, normalizes leading zeros to spaces, writes the 5-character value, and records `g_last_rendered_value`.
- `decimal_split_5digits` (`0x801B090`) splits an unsigned value into five decimal digits at `0x20007DB0..0x20007DC0`.
- `lcd_normalize_digits_to_ascii` (`0x801B130`) converts those digits to ASCII with leading-space suppression based on decimal position.
- Known message tables include `lcd_msg_ofl_open_lead_dashes_table` (`0x8029270`: `" OFL "`, `" OPEN"`, `" LEAD"`, `"-----"`), `lcd_msg_blank_apo_bon_table` (`0x80291B0`: blank, `APOoN`, `APOoF`, buzzer/backlight/APO status text), plus `lcd_msg_lead_dashes_calbin_table` and `lcd_msg_dashes_calbin_table`.

Decimal points, units and display gating:

- `lcd_set_main_decimal_position` (`0x801717E`) and `lcd_set_secondary_decimal_position` (`0x8017FB4`) toggle decimal point bits in the LCD shadow words.
- `lcd_update_unit_scale_annunciator` (`0x801912C`) derives display unit/scale annunciators from `g_dmm_mode` and `g_dmm_range`.
- `lcd_set_unit_scale_index_bits` (`0x8019268`) implements the normal three scale states by toggling `g_lcd_shadow_ram6` bit `0x80` and `g_lcd_shadow_ram2` bit `0x800`; it also updates `g_display_unit_scale_index` (`0x2000804D`). `lcd_update_unit_scale_annunciator(a1 == 1)` forces a fourth special state using `g_lcd_shadow_ram6` bit `0x100`.
- `display_should_show_auto_range` (`0x8019DA2`) decides whether AUTO range is active for display/serial status.
- `display_get_rel_hold_annunciator_state` (`0x801A028`) returns `0=none`, `1=relative`, `2=hold`; `lcd_update_rel_hold_annunciators` (`0x8019FA2`) maps that to `g_lcd_shadow_ram2/4` bit `0x10000`.
- `display_set_blank_holdoff_for_mode` (`0x800DB48`) sets `g_display_blank_holdoff_ticks` (`0x20000238`) after mode/range changes.
- `display_autorange_overrange_update` (`0x800DE24`) handles blank holdoff, sign/overrange flags and autorange changes.
- `display_minmax_capture_update` (`0x800E604`) manages min/max capture display values.
- `display_relative_delta_apply` (`0x800E594`) captures/subtracts relative-mode baseline values.

Display service pipeline:

- `display_service_pending_updates` (`0x8016D34`) is the main-loop display service. It is gated by `g_lcd_update_suppressed` (`0x20007FE4`) and consumes pending flags, then commits LCD shadow RAM when `g_lcd_dirty_flag` is set.
- `g_secondary_display_ready` (`0x2000805E`) triggers `display_service_secondary_ready` (`0x8018B14`).
- `g_measurement_ready_for_display` (`0x2000805D`) or `g_main_display_refresh_requested` (`0x2000805F`) triggers `display_render_main_measurement_or_status` (`0x8016E08`).
- `g_lcd_mode_annunciators_refresh_requested` (`0x20008060`) triggers `lcd_refresh_mode_annunciators_current` (`0x8017222`).
- `g_lcd_range_annunciators_refresh_requested` (`0x20008061`) triggers `lcd_refresh_range_dependent_annunciators` (`0x801776E`).
- `g_lcd_mode_bits_refresh_requested` (`0x2000804C`) redraws the mode/unit annunciator cluster via `lcd_set_mode_annunciators` (`0x80172A8`).
- `g_secondary_page_annunciators_refresh_requested` (`0x20008063`) or a changed `g_display_page_state` (`0x20008037`) triggers `lcd_refresh_secondary_page_annunciators` (`0x801A440`).
- `g_secondary_page_refresh_requested` (`0x20008062`) triggers `display_render_secondary_page` (`0x801797C`).

Main display status priority:

- `display_render_main_measurement_or_status` (`0x8016E08`) first clears the two main display pending flags, then gives priority to lead/open/overflow and calibration/status messages before rendering `g_measure_value` (`0x20007D64`).
- `lcd_show_overflow_message` (`0x80199B4`) writes `" OFL "` unless `g_lead_warning_active` is set for the main display, in which case it writes `" LEAD"`.
- `lcd_show_open_message` (`0x8019A22`) writes `" OPEN"` unless `g_lead_warning_active` is set for the main display.
- `lcd_show_lead_message` (`0x8019A74`) writes `" LEAD"`.
- `lcd_show_dashes_message` (`0x8019A86`) writes `"-----"`.
- `lcd_show_blank_message` (`0x8019A98`) writes five spaces and clears the matching sign indicator.

Mode/unit annunciator cluster:

- `lcd_clear_mode_unit_annunciators` (`0x8019B48`) clears the main mode/unit bit cluster before `lcd_set_mode_annunciators` rebuilds it.
- `lcd_set_mode_annunciators` (`0x80172A8`) maps `g_dmm_mode`, `g_dmm_range` and mode-0 alternate flags to LCD shadow bits. Confirmed examples:
  - Modes `1/2`: `g_lcd_shadow_ram4 |= 0x400`.
  - Modes `3/4`: `g_lcd_shadow_ram2 |= 0x400`, `g_lcd_shadow_ram4 |= 0x400`.
  - Mode `6`: range-dependent bits via `lcd_update_mode6_range_annunciators`.
  - Mode `7`: `g_lcd_shadow_ram6 |= 0x1`.
  - Mode `8`: `g_lcd_shadow_ram0 |= 0x20000`.
  - Mode `9`: range-dependent bits via `lcd_update_mode9_range_annunciators`.
  - Mode `10`: `g_lcd_shadow_ram4 |= 0x80000000`, `g_lcd_shadow_ram2 |= 0x100`.
  - Mode `11` diode: `g_lcd_shadow_ram6 |= 0x80000000`, `g_lcd_shadow_ram4 |= 0x400`.
  - Mode `12`: range-dependent bits via `lcd_update_mode12_range_annunciators`.
  - Current/dual-bank families `13..24` share bits in `g_lcd_shadow_ram0/2/4/6` plus bank bits in `g_lcd_shadow_ram1` or `g_lcd_shadow_ram5`.
- `lcd_update_mode9_range_annunciators` (`0x80177F2`) and `lcd_update_mode6_range_annunciators` (`0x8017858`) use range thresholds 2 and 5 to select the range/unit bits.
- `lcd_update_mode12_range_annunciators` (`0x80177AC`) switches between two range/unit bit groups at range `>= 2`.
- `lcd_update_temp_unit_range_bit` (`0x8017700`) updates the temperature range/unit bit for modes `15/24`.

Bargraph and secondary display:

- `display_service_secondary_ready` (`0x8018B14`) consumes `g_secondary_display_ready`; modes `5, 6, 7, 8, 12` skip the normal bargraph path.
- `lcd_refresh_bargraph_mode` (`0x8018F14`) chooses the bargraph scaling mode from current mode/range.
- `lcd_update_bargraph_value_and_sign` (`0x8018FA8`) computes a 0..25 bargraph count from `abs(g_display_value)` (`0x20007D68`), updates bargraph sign bits, and selects scale annunciators. The divisor is `400` in one scale and `2000` in the other; overrange caps at 25, diode mode caps at 15.
- `lcd_update_bargraph_segments` (`0x8018B80`) sets/clears the physical bargraph segment bits from that 0..25 count and marks the LCD dirty.
- `lcd_clear_bargraph_segments_from_index` (`0x8019408`) clears a run of bargraph segment bits and is also used while clearing secondary unit annunciators.
- `display_render_secondary_page` (`0x801797C`) dispatches the secondary/status page renderer by `g_display_page_state`.
- Identified secondary/status renderers include `display_show_battery_percent_page` (`0x801839A`, `BATxx`), `display_show_apo_onoff_page` (`0x8018460`, `APOoN/APOoF`), `display_show_buzzer_onoff_page` (`0x8018490`, `b-On/b-OFF`), `display_show_lcd_contrast_page` (`0x8018410`, `Lcd-X`), `display_show_burden_message` (`0x8018356`, `burdn`), `display_show_temperature_secondary` (`0x8018A00`, numeric plus C/F), `display_show_diode_option_page` (`0x80186E0`, `"   15"` or `"    3"`), and `display_show_dn_up_threshold_page` (`0x801871C`, `DN/UP 30/300`).
- `display_show_calibration_step_main_prompt` (`0x8019698`) and `display_show_calibration_result_main_message` (`0x8019738`) render main-display calibration prompts/results such as `OFF-E`, `G-END`, `400-E`, and `5KEND`.

Known fixed 5-character messages at `0x80291A8..0x8029290`:

- `"-oFF-"`, blank, `APOoN`, `APOoF`, `b-On `, `b-OFF`, `bdOn `, `bdOFF`.
- `"   15"`, `"    3"`, `DN 30`, `UP 30`, `DN300`, `UP300`.
- `OFF-E`, `G-END`, `400-E`, `5KEND`, `SdCAL`, `SdSAv`, `SdSEt`, `SAvEd`, `-Err-`.
- `" CAL1"`, `" CAL "`, `" OFL "`, `" OPEN"`, `" LEAD"`, `"-----"`, followed by filename text `cal.bin`.

## Buzzer

- `buzzer_on_for_ticks_force` (`0x800EED0`) sets buzzer active flag, countdown and GPIOE bit 7 on.
- `TIM3_buzzer_duration_irq_handler` (`0x8024722`) decrements countdown and turns GPIOE bit 7 off.
- Buzzer GPIO appears to use GPIOE BSRR/BRR bit `0x80`.

## Bluetooth / Serial External Logging Interface

USART at `0x40004400` is configured as a 19200 baud serial interface on GPIOG pins 5/6 with AF7. This is the inferred Bluetooth transport path: the binary has no literal `Bluetooth`/`BLE` strings, but this UART is UI-toggleable, has its own LCD annunciator, periodically sends measurement frames, and accepts external command frames. For our replacement firmware, this path must remain protocol-compatible.

- `bt_uart_init_19200` (`0x801C390`) configures GPIOG pins 5/6, enables the APB1 USART clock bit `0x20000`, initializes baud `19200`, enables USART, and enables RXNE interrupt code `0x25`.
- `bt_uart_disable` (`0x801C954`) disables RX interrupt `0x25`, disables the UART, and gates the APB1 USART clock.
- `bt_uart_apply_enabled_state` (`0x800C51A`) initializes or disables this USART based on `g_bt_streaming_enabled` (`0x2000802B`) and updates the LCD Bluetooth/streaming annunciator.
- `lcd_set_bt_streaming_annunciator` (`0x8019D2C`) mirrors `g_bt_streaming_enabled` to `g_lcd_shadow_ram0` bit `0x80000000`.
- A UI command with argument `0x80` in `ui_handle_button_action` (`0x801622C`) toggles `g_bt_streaming_enabled`, clears RX command state, applies the UART state, resets `g_bt_sample_history_index`, and beeps.
- `TIM2_periodic_scheduler_irq` (`0x802428C`) starts Bluetooth TX every 50 scheduler ticks when `g_bt_streaming_enabled` is set: it resets `g_bt_tx_index`, sends frame byte 0, then enables TXE interrupt code `0x27`.
- `firmware_main_loop` (`0x8024B40`) builds the next frame when `g_bt_frame_build_due` (`0x2000802C`) is set and processes completed RX frames when `g_bt_rx_frame_ready` (`0x20008030`) is set.
- `bt_uart_irq_handler` (`0x80247D8`) sends bytes `1..0x12` on TXE and receives `0xF4`/`0xF8` command frames on RXNE.

TX frame:

- `bt_uart_build_measurement_frame_f2` (`0x800C5DA`) builds `g_bt_tx_frame_f2` (`0x20007AE0`), a 19-byte frame.
- Byte `0` is `0xF2`.
- Bytes `1..4` are BCD-packed auxiliary numeric fields from `bt_uart_pack_bcd_aux_fields` (`0x800C53A`).
- Bytes `5..8` carry mode/range/sign/magnitude bits derived from `g_dmm_mode`, `g_dmm_range` and `g_last_rendered_value`.
- Byte `9` is filled by a display-page/status helper selected by `g_display_page_state`.
- Byte `0x0D` carries unit scale, secondary-unit family, display sign, and special unit-scale state.
- Byte `0x0E` carries the computed bargraph count (`g_lcd_bargraph_count`/`0x20008041`).
- Bytes `0x0F..0x11` carry mode-family, autorange, streaming enabled, min/max, hold/relative and other annunciator/status bits.
- Byte `0x12` is XOR of bytes `0..0x11`.

RX frames:

- The UART IRQ accepts frames starting with `0xF4` length 5 and `0xF8` length 15 into `g_bt_rx_frame` (`0x20007B80`).
- `bt_uart_process_rx_command_frame` (`0x800C332`) runs from the main loop when `g_bt_rx_frame_ready` is set.
- `0xF8` command format: six bytes encoded as ASCII-hex pairs at offsets `1,3,5,7,9,11`; byte at offset `13` is XOR of those six decoded bytes. Valid frames are echoed and applied by `bt_uart_apply_f8_frontend_command_bytes` (`0x801016C`).
- `bt_uart_apply_f8_frontend_command_bytes` (`0x801016C`) applies those six decoded bytes to frontend/control slots `2,4,6,10,12,14`; this is a control/configuration path, not just passive logging.
- `0xF4` command format: one ASCII-hex byte at offset `1`; decoded byte must equal the decoded byte at offset `3`. Valid frames are echoed and stored in `g_bt_f4_command_ready` (`0x20008069`) and `g_bt_f4_command_byte` (`0x2000806D`).
- `bt_uart_decode_hex_byte_from_rx_ascii` (`0x800C470`) decodes two ASCII hex chars from the staged RX frame.
- No stock Bluetooth firmware-update command has been found in the app image. The RX ISR only stages `0xF4` and `0xF8` frames, and the app's flash write xrefs are limited to calibration/data-EEPROM writes through `cal_flash_write_byte` (`0x80120F0`) at `0x08080000 + offset`. The SD firmware updater appears to live in the separate boot/update area, not in this app image.

## Calibration Flash

- Calibration base: `0x08080000`.
- `cal_flash_read_byte` (`0x801204C`) reads one byte from `0x08080000 + offset`.
- `cal_flash_write_byte` (`0x80120F0`) writes one byte to `0x08080000 + offset`.
- `cal_flash_write_byte_retry10` (`0x8012148`) retries writes up to 10 times.
- `cal_flash_read_byte_u8_wrapper` (`0x801203A`) and `cal_flash_write_byte_retry_wrapper` (`0x80120D0`) are the wrappers used by higher-level calibration/settings code.
- `cal_flash_read_u32_be`, `cal_flash_read_s16_be`, `cal_flash_write_u32_be`, `cal_flash_write_u16_be` implement big-endian multi-byte calibration values.
- `cal_mode_base_offsets` (`0x8028EE8`) maps DMM mode to calibration block base.
- Normal calibration block: `base = cal_mode_base_offsets[g_dmm_mode] + 16 * g_dmm_range`
  - `+0`: zero offset
  - `+4`: gain factor
  - `+8`, `+12`: additional per-mode values
- Diode mode (`mode 11`) base offset is `0x0180`; per-range blocks are at `0x08080000 + 0x0180 + 16 * range`.

Calibration SD-card import/export:

- `filename_cal_bin` (`0x8029290`) is the fixed filename `"cal.bin"`.
- `startup_selftest_and_cal_entry_check` (`0x800EA3A`) also handles the startup SD calibration workflow. It displays `SdCAL`, then either `SdSAv` for export or `SdSEt` for import, and finishes with `SAvEd` or `-Err-`.
- `g_sd_cal_workflow_state` (`0x200080CE`) controls the SD workflow states; `g_sd_cal_export_requested` (`0x20008088`) selects export and `g_sd_cal_import_requested` (`0x20008089`) selects import.
- User trigger for calibration export: with SD card inserted, hold `MEM`, power on, release `MEM`, press `UP`, then press `MODE`. This maps in firmware to `g_sd_cal_workflow_state = 1`, then key `0x40` sets `g_sd_cal_export_requested = 1` and state `2`, then key `0x01` confirms state `3` and calls `cal_export_to_cal_bin_and_verify`.
- User trigger for calibration import: hold `MEM`, power on, release `MEM`, press `DOWN`, then press `MODE`. This maps to key `0x80` setting `g_sd_cal_import_requested = 1`, then key `0x01` confirms import.
- `cal_export_to_cal_bin_and_verify` (`0x801CD14`) reads `0x4B0` (`1200`) bytes from calibration flash into `g_cal_bin_transfer_buffer` (`0x20001240`), writes them to `cal.bin`, closes/reopens the file, reads it back as `512 + 512 + 176` bytes, and compares every byte.
- `cal_import_from_cal_bin_and_verify` (`0x801CF06`) opens `cal.bin`, reads the same `1200` bytes into RAM, writes them byte-for-byte into calibration flash, then rereads and verifies all bytes.
- FatFs-style calls are now identified in this path: `fatfs_mount` (`0x8021B80`), `fatfs_open` (`0x8021BCC`), `fatfs_read` (`0x8021D72`), `fatfs_write` (`0x8021F60`) and `fatfs_close` (`0x802224C`).
- Observed return/error codes in this workflow: `0` success, `1` card/init issue, `4` open/create failure, `7` write/short-write, `8` read/short-read, `9` verify mismatch.

Minimal SD calibration export-only patch:

- Created patched image `EEVBlog2_05_cal_export_only_sd.bin`. SHA-256: `79a8dd4a3d32814dba3d5ca07d815ff5f8e92fd4d14e128067318fa6806ac7bc`.
- Original SHA-256: `8f885f12f567c2cccd9862d9fe26f127e2a674c9afbce9f7a38d441b3f396722`.
- This image changes only 4 bytes at `0x801CF06`: `70 B5 AD F5` -> `04 20 70 47` (`movs r0,#4; bx lr`).
- Effect: SD calibration export remains original and writes/verifies `cal.bin` with exactly `0x4B0` bytes from `0x08080000`; SD calibration import returns error immediately and cannot write calibration data.
- Binary diff: only file offsets `0x16F06..0x16F09` differ from the original app image. The filename remains `cal.bin`.

SD full-NVM dump patch:

- Direct flash-buffer image `EEVBlog2_05_full_flash_eeprom_dump_sd.bin` produced `-Err-` on hardware and left a zero-byte `FW.BIN`; removed from the workspace. It likely failed because it passed flash memory directly to `fatfs_write` in one large write.
- Chunked image `EEVBlog2_05_full_flash_eeprom_dump_sd_chunked.bin` also produced `-Err-` quickly and was removed from the workspace. This suggests either the bootloader/update area starting at `0x08000000` is not safely readable from the app, or another first-block constraint is being hit.
- Created no-bootloader chunked image `EEVBlog2_05_dump_appflash_eeprom_no_boot_sd.bin`. SHA-256: `7c7371b0a9ac95dc52abf9a2e414854743906ff99dd199144bad4e7fa0c5ac33`.
- Diagnostic image `EEVBlog2_05_diag_app4k_sd.bin` dumps only `0x1000` bytes from known app flash `0x08006000`; SHA-256: `f541154742bacd360e31dce942c8c4f6e6873a7ee2106c5471b66a5ba375f1c3`.
- Diagnostic image `EEVBlog2_05_diag_boot4k_sd.bin` dumps only `0x1000` bytes from bootloader/update area `0x08000000`; SHA-256: `cfc3b29aa38885231b479757e446f85abe72308c65b7fe6de3b87863558f5d23`.
- Original SHA-256: `8f885f12f567c2cccd9862d9fe26f127e2a674c9afbce9f7a38d441b3f396722`.
- The patch reuses the existing SD calibration export trigger but writes `fw.bin` instead of `cal.bin`.
- The no-bootloader image copies each `0x200`-byte block to original calibration transfer buffer `0x20001240` and calls `fatfs_write` with that RAM buffer.
- `fw.bin` layout for the no-bootloader image after a successful export:
  - offset `0x00000..0x59FFF`: app flash area read from `0x08006000..0x0805FFFF`.
  - offset `0x5A000..0x5CFFF`: data EEPROM read from `0x08080000..0x08082FFF` (12 KiB, includes calibration area).
  - expected total file size: `0x5D000` bytes (`380928` decimal).
- Binary diff from original app image: 141 changed bytes, all in the SD export/import area and filename slot.
- Patched addresses:
  - `0x801CD74`: after file create/open, success path skips the original `0x4B0`-byte cal write and branches to the replacement chunked dump writer.
  - `0x801CDA8`: initial write error path branches to close/cleanup.
  - `0x801CDB0`: old read-back verifier replaced with a loop that writes app flash blocks plus optional data-EEPROM blocks, each `0x200` bytes, via RAM buffer `0x20001240`.
  - `0x801CF06`: import path returns error immediately, preventing this dump firmware from writing `fw.bin` back into calibration EEPROM.
  - `0x8029290`: filename changed from `cal.bin` to `fw.bin`.

Custom P10 mode reimplementation notes:

- Current build artifact: `/Users/taarimi7/Downloads/ida_pro/eevblog121gw_fw/build/diode_mode/eevblog121gw_fw.bin`.
- 2026-04-29 CAP build size: `8586` bytes. SHA-256: `22dc420da2e672120c7747c799b03bd5c8804c0b19d522062f481853d555b90a`.
- 2026-04-29 Bluetooth diagnostic build size: `9254` bytes. SHA-256: `39923da2019a14f8cedcd844b578318ba462c3fc80a95012eae846e94259eec3`. In P10 test mode it initializes USART2/Bluetooth by default, sends `BTOK\r\n` at startup, emits the existing `0xF2` heartbeat once per second, echoes each received byte, and briefly shows the latest RX byte as `b-XX` on the upper LCD field.
- 2026-04-29 Bluetooth-enable-pin build size: `9310` bytes. SHA-256: `fc276d5d501e44297d80d4316491dd055f0d15486f2662fb33c0e38863ee8ee8`. It additionally configures GPIOG15 as output and drives it high before USART2 init, matching stock `bt_uart_init_19200`; stock `bt_uart_disable` drives GPIOG15 low.
- 2026-04-29 diode RANGE-option build size: `9602` bytes. SHA-256: `106f7a1e40c31f32e0338e3d5d1f52d24f8169a788a70f3304e7470aa96961e0`. RANGE in diode mode now follows stock behavior: it toggles the diode `3`/`15` option, briefly renders `"    3"` or `"   15"` on the upper LCD field, and uses the matching diode decimal/scaling path. It does not cycle an actual diode range, because stock `ui_handle_key_event` calls the diode option handler for mode 11 instead of `range_button_handle_manual_autorange`.
- 2026-04-29 diode high-voltage GPIO build size: `9794` bytes. SHA-256: `a75315df7d60b78fbbf2e83fde6f81d630b5cd066dab2cfc39171be8c55714ba`. The `3`/`15` diode option now also drives the stock hardware selector GPIOs recovered from `sub_800FD30`: `3 V` = PE15 high / PG6 low, `15 V` = PE15 low / PG6 high. PG6 is shared with Bluetooth RX in this custom build, so diode voltage selection can interfere with receiving Bluetooth bytes while this path owns the pin.
- 2026-04-30 persistent diode option display build size: `9758` bytes. SHA-256: `1c7dfe952fcc5142aaf04bd1f32b816a7bfae3906ddd660a80a63d00c5e6f739`. The diode `3`/`15` secondary display is now persistent while mode 11 is selected, and sets the recovered small secondary `V` annunciator bit (`g_lcd_shadow_ram2 |= 0x20000`, from stock `lcd_refresh_secondary_page_annunciators` page 10/mode 11 path).

## Stock UART CAP Diagnostic Patch

- Created patched stock 2.05 image `EEVBlog2_05_stock_uart_capdiag_updater.bin`.
- SHA-256: `3380d72c8082a0800045222cefeaedc08cab7bc6b3d268c4d3030669b4ac7637`.
- The file is the original 2.05 app plus an appended payload at `0x080294E8`; patched image ends at `0x0802A5F8`, still inside app flash (`0x08006000..0x0805FFFF`).
- Hooked stock `frontend_status_bit0_cap_event_dispatch` call at `0x0800AA78` to `hook_cap_compute`, which calls the original `cap_compute_display_value_from_counters()` and emits one `SCAP E ...` UART line after each CAP bit0 event.
- Hooked stock `measurement_poll_frontend_ready` call at `0x08006148` to `hook_cap_timeout`, which calls the original `cap_timeout_autorange_handler()` and emits `SCAP T ...` when a pending CAP timeout was handled.
- Hooked stock `bt_uart_irq_handler` receive byte call at `0x08024830` to `hook_usart_read_data`, so ASCII UART commands can coexist with the stock F4/F8 BLE command path.
- CAP diagnostic line fields: mode/range/status/reg1e/reg1f/reg20/reg29, CAP count/ref/wrap/elapsed, frontend timing word, raw/display value, zero offset, gain, timeout ticks/counter, and key flags.
- UART commands added to the patched stock firmware:
  - `U?` or `UINFO`: identify diagnostic patch and updater support.
  - `USTOPDBG` / `USTARTDBG`: disable/enable automatic `SCAP` debug output.
  - `USIM <size_hex> <crc32_hex>`: enter SRAM updater in simulation mode.
  - `UFBEGIN <size_hex> <crc32_hex>`: enter SRAM updater in flash mode.
- SRAM updater is copied to `0x20002000` before flash operations. It only erases/programs app flash starting at `0x08006000` and refuses images above `0x0005A000` bytes, so the bootloader area `0x08000000..0x08005FFF` is not written by this path.
- 2026-05-01 follow-up fix: default CAP debug is disabled, the diagnostic SRAM magic was bumped so soft reset also reinitializes debug off, and diagnostic UART sends are guarded/non-blocking when USART2 is disabled by the stock firmware. New `EEVBlog2_05_stock_uart_capdiag_updater.bin` image size `0x24718`, SHA-256 `c22ff0e46ed0f00841e870259d5a70bf85d42b772b67a6e01be3ad3566b6d5c4`. This prevents the CAP hook from freezing the LCD when Bluetooth/USART2 is off.
- 2026-05-01 CAP freeze isolation build: `EEVBlog2_05_stock_uart_updater_nocap.bin` removes both CAP hooks at `0x0800AA78` and `0x08006148`; only the USART2 RX hook at `0x08024830` remains. Image size `0x24718`, SHA-256 `d6366e3af3347a190f85d246343cb3d5002b957928f396420243d827afd4e381`. This is intended to prove whether the freeze is caused by the CAP instrumentation itself.
- 2026-05-01 direct-start UART stock diagnostic build: `EEVBlog2_05_stock_uart_direct_start.bin` keeps Stock CAP untouched, patches the reset vector at `0x08006004` to run `SystemInit`, initialize USART2 directly at `19200` while holding BLE enable `PG15` low, and then enter the stock runtime at `0x0802919D`. Both Bluetooth apply branches at `0x0800C528`/`0x0800C52E` are patched to reapply the direct UART/BLE-off state, and the USART2 RX hook remains at `0x08024830`. CAP debug is on-demand through `UCAPDUMP`; no CAP event/timeout hooks are installed. Image size `0x24858`, SHA-256 `0ffb5b8bb1c3daac447e6ced7ce8f4a64b7236737977981fe40102e9f788b8a4`.
- 2026-05-01 direct-start UART hello/CAP-trigger build: `EEVBlog2_05_stock_uart_direct_start.bin` also hooks only the stock CAP timeout call at `0x08006148` to call the original timeout handler, reassert direct USART2 at 19200, hold BLE enable `PG15` low, and periodically emit `HALLO stockuart 19200 ble=0`. It still does not hook CAP event computation. Both stock BLE apply branches at `0x0800C528`/`0x0800C52E` and the additional direct `bt_uart_disable` callsite at `0x0801C608` are patched to the BLE-off UART routine, so enabling Bluetooth in the stock UI should leave the BLE module disabled while keeping the direct UART path active. Image size `0x2496c`, SHA-256 `48193764ff52a7e434208810d0433364f3f8c3b735d5e701e6691d15368c6433`.
- 2026-05-01 direct-start UART normal-measurement-trigger build: after the CAP-trigger build froze in CAP, the CAP timeout hook was removed again. `EEVBlog2_05_stock_uart_direct_start.bin` now hooks only the normal measurement dispatcher call at `0x080061AA`; it calls stock `dmm_measurement_mode_dispatch`, skips mode 12/CAP explicitly, then reasserts USART2 at 19200 with BLE enable low and periodically emits `HALLO stockuart 19200 ble=0`. Startup and BLE apply/disable hooks also emit one hello line. Image size `0x249cc`, SHA-256 `3aaee6bff9102e9a935564a17aaefe02d959c6e65bdaceba425b5f1be99815be`.
- 2026-05-01 direct-start UART passive recovery build: after the normal-measurement-trigger build froze completely, all automatic UART sends and the measurement dispatcher hook were removed. The build now only initializes direct USART2/BLE-off at reset, patches BLE apply/disable callsites to keep `PG15` low while preserving direct UART init, and keeps the USART2 RX hook for explicit commands if the UART path is actually alive. Image size `0x249a8`, SHA-256 `5f8efccecf3bbf9ec694ee28f0b90fb8cf16eea6bb392132d6ede382246e9143`.
- 2026-05-01 direct-start UART RX-disabled recovery build: after sending `U?` froze the previous passive build, the USART2 RX hook at `0x08024830` was removed and direct UART init no longer enables RXNE interrupt/NVIC USART2. This keeps `PG15` low and leaves TX/RX pins configured, but incoming host bytes cannot enter our diagnostic parser or interrupt path. Image size `0x249a8`, SHA-256 `41354c2fb0787e2f86fa7e48327f4d81d9601863f7d02ecfcc8d745875a21143`.
- 2026-05-01 stock-TX BLE-low test build: `EEVBlog2_05_stock_uart_stocktx_blelow.bin` removes reset/direct/RX hooks entirely. It only patches the stock Bluetooth enable call at `0x0800C528` to call the original `bt_uart_init_19200` and then force `PG15` low, plus patches disable callsites `0x0800C52E`/`0x0801C608` to call original disable and force `PG15` low. This should let the stock firmware generate its own 19-byte frames when Bluetooth streaming is enabled, while the BLE112 remains disabled and the external UART adapter can listen on STM32 `PD5` TX. Image size `0x249e0`, SHA-256 `36a7e7c3da391677793c2cf4f60bd99e54fba3843ab87ece116f07e5d1a38ff3`.
- 2026-05-01 stock-TX safe diagnostic-frame build: `EEVBlog2_05_stock_uart_stocktx_blelow.bin` keeps the BLE-low stock UART init/disable hooks and additionally wraps the stock main-loop frame build call at `0x08024C0C`. The wrapper calls the original frame builder, forces `PG15` low, and every 16th frame replaces the 19-byte stock TX buffer with a checksummed `F2 DBG0...` diagnostic frame containing mode, range, frontend status/event bytes and display/range flags. No UART send is done directly from hooks/interrupts, and no RX path is patched. Image size `0x24ae4`, SHA-256 `7cddf21ce92db2ec1e2bf2e1d763dcf56dd53f940e6f99f75c33bfe812c89414`.
- 2026-05-01 SRAM-updater-start build: `EEVBlog2_05_stock_uart_sram_updater_start.bin` keeps the BLE-low stock UART init/disable hooks, restores a reduced USART2 read hook at `0x08024830`, and hooks the stock RX processing call at `0x08024D56`. The read hook no longer replies or jumps from interrupt context; it only recognizes `USIM <size> <crc>` or `UFBEGIN <size> <crc>`, stores size/CRC/mode in diagnostic SRAM, and sets `g_bt_rx_frame_ready`. The main-loop RX hook then enters the existing SRAM updater, which disables interrupts and handles all replies/streaming from RAM. Image size `0x24918`, SHA-256 `85bd659dd6471d9563d0f2d20c3335e7498f0ddf4b0c8143dcd33782a93056c5`.
- 2026-04-30 diode open `.OFL` build size: `9910` bytes. SHA-256: `94a5a18fb2cdad254dac92ed6952a5ea4c97663f9d7408aa7cd658fe447565be`. In diode mode, overrange/open lead now renders the main display as `.OFL` by writing `" OFL "` and setting the leading main decimal marker.
- 2026-04-30 Ohm autorange/status-annunciator build size: `10722` bytes. SHA-256: `40e8467ed7721eed0a864ec4eacf3c84b1d6310b4875a3a86b4b4f390871178a`. Ohm autorange now follows stock `display_autorange_overrange_update`: high threshold is `threshold + threshold/10`, or `threshold + threshold/12` while overrange-latched, low threshold is `threshold/10`, range changes happen immediately after the short stock-like blank holdoff, and the range decision uses the calibrated instantaneous measurement before display smoothing/compensation. The earlier non-stock hard `50000` range-6 open threshold was removed. Ohm overrange is rendered as stock-style `" OFL "` with the current Ohm decimal position, while diode open still uses the requested `.OFL` rendering. The P10 display now also sets recovered `AUTO` (`g_lcd_shadow_ram7 |= 1`) when autoranging, `APO` (`g_lcd_shadow_ram5 |= 0x80`) by default, and shows a small secondary Celsius temperature readout from the stock ADC1 channel-4 NTC path.
- 2026-04-30 P10 bargraph build size: `11162` bytes. SHA-256: `8538fede5818a2bcd3c6fb43ca981052b19c77dc1e7d05029490cceb972f98c2`. The LCD driver now renders the recovered 25-segment bargraph bit sequence from `lcd_update_bargraph_segments` (`0x8018B80`). For P10 Ohm/continuity/diode it computes the stock-like count as `abs(display_value) / 2000`, forces 25 on overrange, and clamps diode mode to 15 segments like `lcd_update_bargraph_value_and_sign` (`0x8018FA8`). CAP mode keeps the bargraph off, matching the original secondary/bargraph skip behavior for mode 12.
- 2026-04-30 P10 bargraph hold build size: `11198` bytes. SHA-256: `bd97b0b6bf731a65c4c6037a8826490e2a6fb6458b346bd6d1a550944ac75f67`. The bargraph/range display now has independent sample state instead of being tied to the main numeric `have_value` flag. During Ohm autorange transitions the main number may still blank briefly like stock, but the recovered range/bargraph segment display is kept alive from the current/last instantaneous sample so it no longer disappears just because the range changed.
- 2026-04-30 CAP frontend-event ACK build size: `11222` bytes. SHA-256: `97c282b33be092be11f10ef11ccf9811eea03624879b7c6c8959b1921f56cc53`. `frontend_read_status` now mirrors stock `frontend_read_status_and_dispatch` by writing one zero byte to frontend register `0x1E` immediately after reading the 52-byte status block. This clears stale event bits before the next conversion. The CAP decoder also no longer restarts conversion when the reference counter at status bytes `0x18..0x1A` is zero, avoiding a loop where an uncleared bit-0 event could repeatedly restart CAP before a valid numeric value is available.
- 2026-04-30 Ohm open-range build size: `11390` bytes. SHA-256: `b6997f3a27b77781cec4240b4a83defffabb9bab399b09af9a4aae957149843e`. Ohm averaging now matches `measurement_accumulate_average` (`0x8006342`): range 5 uses one sample and all other Ohm ranges use eight samples. The highest Ohm range now treats the nominal `50000` threshold as the open-lead/OFL cutoff instead of adding the autorange hysteresis margin, so open leads should render stock-style `" OFL "` instead of lingering as MOhm numeric values near the top of range 6.
- 2026-04-30 Ohm/CAP autorange-timeout build size: `11679` bytes. SHA-256: `b391d642b34cf7faf5cab1e4f3380860a134a29be6de3f9a8da414b5fcd8edff`. Ohm/CAP autorange now uses the already conditioned display value for range decisions, matching the original flow where `display_autorange_overrange_update` runs after the mode-specific measurement smoothing/compensation. CAP also now implements the recovered timeout path from `sub_800B540`/`sub_800EC28`/`TIM2_periodic_scheduler_irq`: if no CAP sample arrives before the stock-like 200 ms timeout (300 ms on CAP range 5), autorange jumps from ranges 0/1 to range 2, then from ranges 2..4 to range 5, or reports overrange at the top/manual range.
- 2026-04-30 P10 rotary-entry reset build size: `11771` bytes. SHA-256: `afbe12772864597380cfd5a415de2f3455b5a2df7c03df9e652f1c59641b5de3`. Entering P10 via the rotary switch now explicitly resets the local P10 state to Ohms, default Ohm range, autorange enabled, and clears stale display/bargraph state. This fixes the case where Ohm autorange only worked after first cycling through diode/CAP with the MODE key, because the MODE path reset autorange while direct rotary entry could preserve stale manual/range state.
- 2026-04-30 P10 open-lead text variants build size: `11875` bytes. SHA-256: `67b47277b34b2f080baa332468ce03fd73d84662fbb29bec3d88c8639c927958`. Open/overrange text is now mode-specific in P10: Ohm renders `0.FL`, continuity renders `0F.L`, and diode renders `.0FL`.
- 2026-04-30 MODE long-press backlight build size: `12335` bytes. SHA-256: `6da7eceb35753a9045953868f6dd9b3afa4a7bfa8eb9f2902a8fed13386abceb`. Backlight is controlled on recovered PE8, active high (`ui_handle_button_action` case 1 in stock toggles `0x40021018/1A` bit `0x100`). A long MODE press (about 700 ms) toggles it without also cycling P10 mode; short MODE now cycles mode on release. The setting is persisted as one byte at data-EEPROM offset `0x04F0`, outside the original `cal.bin` export/import region `0x0000..0x04AF`, using `0xA5` for on and `0x5A` for off.
- 2026-04-30 REL offset build size: `12507` bytes. SHA-256: `fc35e85af656b2a724b4706d8146e4ea80cffaf45c165c7bac42df7ba87b04bf`. REL key mask `0x20` now toggles relative mode in P10. On activation it captures the current displayed value as baseline, freezes autorange to keep the numeric scale stable, and displays subsequent values as `current - baseline`; on deactivation it restores the previous auto/manual range state. The recovered REL annunciator is also set while active: LCD RAM2 and RAM4 bit `0x10000`.
- CAP mode (`mode 12`) does not use the normal status `0x04` signed-24 sample path. It uses frontend status bit `0x01`, decodes little-endian 24-bit counters from status bytes `0x15..0x17` and `0x18..0x1A`, computes `count/ref` or `100*count/ref` depending on range, restarts conversion timing, then applies calibration and dynamic smoothing.
- Original CAP smoothing deadband is `value/10` for range 5 and `value/50` for other ranges, clamped to at least 20, with a 10-sample smoothing window. Ranges 2..4 clamp smoothed values below 21 to zero.
- Original low CAP ranges (`0` and `1`) do not have a generic small-value display clamp. They subtract the stored calibration zero offset and only become zero when `raw <= zero_offset`; therefore an open-lead residual above zero points to frontend/analog state or calibration, not a display threshold.
- Stock board GPIO init puts `PA4`, `PA5`, `PG11`, `PG13`, and `PG14` into analog mode. The custom firmware already mirrored all except `PG14`; `PG14` is now mirrored too. Other stock GPIO init outputs such as `PB0/PB1`, `PD8..PD15`, and `PE0..PE2` are later overwritten by the LCD controller in stock, so they should not be re-applied after our LCD init.
- Original CAP icon/range annunciator helper `lcd_update_mode12_range_annunciators` sets LCD shadow bit `0x200` in RAM4 always, with RAM0/RAM2 `0x200` selected by range `<2` vs `>=2`.

## Still Missing For A Clean Reimplementation

- Exact MCU part and clock tree confirmation from hardware markings or full vector/register map.
- Clock setup is mapped and currently decodes to 32 MHz, but exact hardware marking is still needed before assuming any overclock margin.
- Full external frontend/ADC protocol: register map, command meanings, status bits, and electrical pin mapping.
- LCD glass physical segment labels are still incomplete, but the STM32 LCD controller path, shadow buffer, character rendering, main mode/unit bits, status messages, secondary-page dispatcher and bargraph logic are now mapped.
- Complete mode-name mapping for all `g_dmm_mode` values.
- Complete range-count table source and range labels.
- Bluetooth/serial protocol details still need semantic labels for every status bit, but UART pins/baud, frame lengths, checksums, TX scheduling, LCD toggle, and RX command decoding are mapped.
- Remaining display work: exact printed labels for every LCD shadow bit and semantic labels for some secondary settings (`bd`, DN/UP threshold options, several calibration/status page variables).
- Safe flashing/update flow, including bootloader behavior and preserving calibration sector.
