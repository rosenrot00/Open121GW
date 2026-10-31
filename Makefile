PROJECT := eevblog121gw_fw
BUILD_ROOT := build

CLANG ?= clang
HOST_CC ?= cc
RUST_LLD := $(shell find $(HOME)/.rustup/toolchains -path '*/bin/rust-lld' | head -1)

EEV_ENABLE_FAST_DIODE ?= 0
EEV_LCD_ONLY_TEST ?= 0
EEV_LCD_TEXT_TEST ?= 0
EEV_INPUT_TEST ?= 0
EEV_DIODE_FE_TEST ?= 0
# Historical name: this is the full custom P10 firmware path, not only a diode test.
EEV_DIODE_MODE_TEST ?= 1
EEV_UART_UPDATE_TRACE ?= 0
EEV_BLE_BAUD_SWEEP ?= 0
EEV_BLE_FIXED_38400 ?= 0
EEV_BLE_FIXED_19200 ?= 0
EEV_UART_PIN_TEST ?= 0
EEV_BLE_LINK_TEST ?= 0
EEV_TASK_FIRMWARE ?= 1
ENABLE_LTO ?= 0
BUILD_FLAVOR := $(if $(filter 1,$(EEV_TASK_FIRMWARE)),task_p10,$(if $(filter 1,$(EEV_BLE_LINK_TEST)),ble_link_test,$(if $(filter 1,$(EEV_UART_PIN_TEST)),uart_pin_test,$(if $(filter 1,$(EEV_DIODE_MODE_TEST)),diode_mode,$(if $(filter 1,$(EEV_DIODE_FE_TEST)),diode_fe_test,$(if $(filter 1,$(EEV_INPUT_TEST)),input_test,$(if $(filter 1,$(EEV_LCD_TEXT_TEST)),lcd_text,$(if $(filter 1,$(EEV_LCD_ONLY_TEST)),lcd_only,$(if $(filter 1,$(EEV_ENABLE_FAST_DIODE)),fast_diode,normal)))))))))
BUILD := $(BUILD_ROOT)/$(BUILD_FLAVOR)
LLD_DIR := $(BUILD_ROOT)/lld

CFLAGS := \
	-target armv7m-none-eabi \
	-mcpu=cortex-m3 \
	-mthumb \
	-std=c11 \
	-ffreestanding \
	-fno-builtin \
	-fdata-sections \
		-ffunction-sections \
		-fno-unwind-tables \
		-fno-asynchronous-unwind-tables \
		-Os \
	-Wall \
	-Wextra \
	-Werror \
	-I src \
	-DEEV_ENABLE_FAST_DIODE=$(EEV_ENABLE_FAST_DIODE) \
	-DEEV_LCD_ONLY_TEST=$(EEV_LCD_ONLY_TEST) \
	-DEEV_LCD_TEXT_TEST=$(EEV_LCD_TEXT_TEST) \
		-DEEV_INPUT_TEST=$(EEV_INPUT_TEST) \
		-DEEV_DIODE_FE_TEST=$(EEV_DIODE_FE_TEST) \
		-DEEV_DIODE_MODE_TEST=$(EEV_DIODE_MODE_TEST) \
		-DEEV_UART_UPDATE_TRACE=$(EEV_UART_UPDATE_TRACE) \
		-DEEV_BLE_BAUD_SWEEP=$(EEV_BLE_BAUD_SWEEP) \
		-DEEV_BLE_FIXED_38400=$(EEV_BLE_FIXED_38400) \
			-DEEV_BLE_FIXED_19200=$(EEV_BLE_FIXED_19200) \
			-DEEV_UART_PIN_TEST=$(EEV_UART_PIN_TEST) \
			-DEEV_BLE_LINK_TEST=$(EEV_BLE_LINK_TEST) \
			-DEEV_TASK_FIRMWARE=$(EEV_TASK_FIRMWARE)

LDFLAGS := \
	-target armv7m-none-eabi \
	-mcpu=cortex-m3 \
	-mthumb \
		-nostdlib \
		-fuse-ld=lld \
		-B$(LLD_DIR) \
	-Wl,-T,linker.ld \
	-Wl,--gc-sections \
		-Wl,-Map,$(BUILD)/$(PROJECT).map

ifeq ($(ENABLE_LTO),1)
CFLAGS += -flto
LDFLAGS += -flto
endif

SRCS := \
	src/startup.c \
	src/clock.c \
	src/gpio.c \
	src/analog.c \
	src/fault_diag.c \
	src/lcd.c \
	src/display_model.c \
	src/display_task.c \
	src/dual_secondary_task.c \
	src/input.c \
	src/input_task.c \
	src/lead_warning_task.c \
	src/measurement_controller.c \
	src/measurement_profile.c \
	src/mem_logging_task.c \
	src/rotary_mode_task.c \
	src/backlight.c \
	src/battery.c \
	src/apo_task.c \
	src/buzzer.c \
	src/calibration.c \
	src/sdio_sd.c \
	src/sd_fat.c \
	src/sd_card_task.c \
	src/status_task.c \
	src/frontend.c \
	src/temperature.c \
	src/bluetooth.c \
	src/bluetooth_app_task.c \
	src/bluetooth_meter_task.c \
	src/uart_update.c \
	src/aux_adc_task.c \
	src/p10_measure_task.c \
	src/task_sched.c \
	src/task_runtime.c \
	src/dmm.c \
	src/main.c

OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean test

all: $(BUILD)/$(PROJECT).bin

test: build/host_tests/test_bluetooth_app_task build/host_tests/test_task_sched \
		build/host_tests/test_measurement_controller build/host_tests/test_display_task \
		build/host_tests/test_sd_card_task build/host_tests/test_measurement_profile \
		build/host_tests/test_display_model build/host_tests/test_fault_diag
	python3 -m unittest discover -s tests -p 'test_*.py'
	build/host_tests/test_bluetooth_app_task
	build/host_tests/test_task_sched
	build/host_tests/test_measurement_controller
	build/host_tests/test_display_task
	build/host_tests/test_sd_card_task
	build/host_tests/test_measurement_profile
	build/host_tests/test_display_model
	build/host_tests/test_fault_diag

build/host_tests/test_bluetooth_app_task: tests/test_bluetooth_app_task.c \
		src/bluetooth_app_task.c src/bluetooth_app_task.h src/bluetooth.h src/uart_update.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I src \
		src/bluetooth_app_task.c tests/test_bluetooth_app_task.c -o $@

build/host_tests/test_task_sched: tests/test_task_sched.c \
		src/task_sched.c src/task_sched.h src/clock.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I src \
		src/task_sched.c tests/test_task_sched.c -o $@

build/host_tests/test_measurement_controller: tests/test_measurement_controller.c \
		src/measurement_controller.c src/measurement_controller.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I src \
		src/measurement_controller.c tests/test_measurement_controller.c -o $@

build/host_tests/test_display_task: tests/test_display_task.c \
		src/display_task.c src/display_task.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I src \
		src/display_task.c tests/test_display_task.c -o $@

build/host_tests/test_sd_card_task: tests/test_sd_card_task.c \
		src/sd_card_task.c src/sd_card_task.h src/sd_fat.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I src \
		src/sd_card_task.c tests/test_sd_card_task.c -o $@

build/host_tests/test_measurement_profile: tests/test_measurement_profile.c \
		src/measurement_profile.c src/measurement_profile.h src/dmm.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I src \
		src/measurement_profile.c tests/test_measurement_profile.c -o $@

build/host_tests/test_display_model: tests/test_display_model.c \
		src/display_model.c src/display_model.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I src \
		src/display_model.c tests/test_display_model.c -o $@

build/host_tests/test_fault_diag: tests/test_fault_diag.c \
		src/fault_diag.c src/fault_diag.h src/hw.h
	@mkdir -p build/host_tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -DEEV_FAULT_DIAG_HOST_TEST -I src \
		src/fault_diag.c tests/test_fault_diag.c -o $@

$(LLD_DIR)/ld.lld:
	@test -n "$(RUST_LLD)" || (echo "rust-lld not found; install Rust or provide a linker" >&2; exit 1)
	@mkdir -p $(LLD_DIR)
	@ln -sf "$(RUST_LLD)" $(LLD_DIR)/ld.lld

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/$(PROJECT).elf: $(OBJS) linker.ld $(LLD_DIR)/ld.lld
	$(CLANG) $(LDFLAGS) $(OBJS) -o $@

$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf tools/elf2bin.py
	python3 tools/elf2bin.py $< $@

clean:
	rm -rf $(BUILD_ROOT)
