# Task Firmware Method

This fork is for turning the current stock-matching custom firmware into a more
structured firmware without losing the working updater and debug surface.

The default build flavor in this fork is `task_p10`. It still uses the proven
P10 firmware path for now; the separate flavor only prevents us from mixing build
artifacts with the older `diode_mode` output.

## Current Invariants

- SRAM updater and UART flashing stay available before each functional change.
- Flash writes remain limited to the main firmware range; bootloader pages are not
  touched.
- Debug commands used for comparison stay available:
  - `UAPP` for app state and rotary/mode/range.
  - `ULCD`, `ULCDR`, and `ULCDT` for LCD shadow and raw segment tests.
  - `SCAP` and `UCAPDUMP` for CAP/front-end counters.
  - `UTASK` for confirming that the task fork is running and that the scheduler
    table is initialized.
  - Existing input, ADC, SD, and updater commands in `uart_update.c`.
- P10 rotary testing uses the physical rotary switch. Software rotary forcing is
  not treated as equivalent because the switch also changes the jack/mux path.
- A change is accepted only when stock and task firmware agree at the behavior
  layer: front-end register state, range state, LCD shadow, and visible mode
  symbols. Numeric jitter alone is not a bug if calibration/range state matches.

## Why Cooperative Tasks

The STM32L1 and the measurement front-end do not need an RTOS for this firmware.
Preemptive threads would make the currently understood timing harder to compare
against stock. The first target is a cooperative scheduler: each task runs quickly,
uses explicit periods, and leaves interrupts for UART/SysTick/front-end events.

The scheduler is intentionally small and static:

- no heap allocation;
- fixed task table;
- millisecond timestamps from `clock_millis()`;
- no string lookup or libc dependency;
- tasks can be enabled/disabled by handle.

`UTASK` prints the scheduler heartbeat and per-domain state. `UTASKS` reports
period, run count, lateness, skipped runs, overruns, and runtime maxima for every
registered task.

## Current Task Split

- `input_task` and `rotary_mode_task`: physical input sampling, debounce, and
  mode transitions.
- `p10_measure_task`, `measurement_controller`, and `measurement_profile`:
  front-end events, explicit search/settle/precision phases, range decisions,
  and mode-specific stock profiles.
- `display_task`, `display_model`, and `dual_secondary_task`: independent LCD
  cadence, displayed-value selection, and secondary display state.
- `aux_adc_task`: asynchronous temperature and battery ADC sequencing.
- `bluetooth_app_task` and `bluetooth_meter_task`: command parsing and periodic
  stock-style meter frames.
- `mem_logging_task` and `sd_card_task`: sampling cadence plus
  measurement-boundary-gated card writes.
- `lead_warning_task`, `status_task`, and `apo_task`: UI/safety state outside the
  measurement path.

`main.c` still owns top-level orchestration and stock behavior that spans
multiple domains. Extraction is accepted only when it removes duplicated state
or blocking work without changing the observed front-end sequence.

## Source Snapshots

`main` contains the task firmware. `legacy-stocklike` preserves the earlier
implementation. Each branch starts from an English source snapshot.

## Compare Loop

For every P10 mode:

1. Flash stock diagnostic firmware.
2. Set the physical rotary to P10.
3. Cycle MODE once through the whole group to normalize persisted mode state.
4. Capture at 100 ms for the first second, then 500 ms for the remaining window.
5. Record `UAPP`, `SCAP` when CAP is active, `ULCD`, and relevant front-end bytes.
6. Flash this task firmware and repeat the same physical steps.
7. Compare state fields before changing code.

The first 300 ms after a mode transition are part of the test because stock often
shows range/blanking behavior there.

Use `tools/capture_p10_physical.py` for this first group. It requires the rotary
to be physically in P10 and only simulates MODE button clicks, so it keeps the
jack/mux path equivalent to manual operation.
