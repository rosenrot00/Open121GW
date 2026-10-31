PROJECT := eevblog121gw_fw
BUILD_ROOT := build

CLANG ?= clang
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
ENABLE_LTO ?= 0
BUILD_FLAVOR := $(if $(filter 1,$(EEV_BLE_LINK_TEST)),ble_link_test,$(if $(filter 1,$(EEV_UART_PIN_TEST)),uart_pin_test,$(if $(filter 1,$(EEV_DIODE_MODE_TEST)),diode_mode,$(if $(filter 1,$(EEV_DIODE_FE_TEST)),diode_fe_test,$(if $(filter 1,$(EEV_INPUT_TEST)),input_test,$(if $(filter 1,$(EEV_LCD_TEXT_TEST)),lcd_text,$(if $(filter 1,$(EEV_LCD_ONLY_TEST)),lcd_only,$(if $(filter 1,$(EEV_ENABLE_FAST_DIODE)),fast_diode,normal))))))))
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
		-DEEV_BLE_LINK_TEST=$(EEV_BLE_LINK_TEST)

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
	src/lcd.c \
	src/input.c \
	src/backlight.c \
	src/battery.c \
	src/buzzer.c \
	src/calibration.c \
	src/sdio_sd.c \
	src/sd_fat.c \
	src/frontend.c \
	src/temperature.c \
	src/bluetooth.c \
	src/uart_update.c \
	src/dmm.c \
	src/main.c

OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean

all: $(BUILD)/$(PROJECT).bin

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
