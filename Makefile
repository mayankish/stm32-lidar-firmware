# Makefile -- stm32-lidar-firmware
# Bare-metal build: no HAL, no CubeMX, no IDE project files.
# Toolchain: GNU Arm Embedded (arm-none-eabi-gcc). Tested against
# arm-none-eabi-gcc 12.2 (xPack / Arm GNU Toolchain "12.2.Rel1"); any
# 10.x-13.x release of the same toolchain should work unchanged.
#
# Usage:
#   make            build build/firmware.elf, .bin, .hex, print size
#   make flash      flash via st-flash (stlink-tools)
#   make flash-ocd   flash via openocd
#   make clean

TARGET     = firmware
BUILD_DIR  = build

# ---- vendored sources (git submodules -- see ../.gitmodules and README) ----
CMSIS_CORE_DIR   = Drivers/CMSIS
CMSIS_DEVICE_DIR = Drivers/CMSIS-Device-F4
FREERTOS_DIR     = Drivers/FreeRTOS-Kernel

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc -x assembler-with-cpp
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
GDB     = arm-none-eabi-gdb

MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

DEFS = -DSTM32F411xE -DUSE_FULL_LL_DRIVER

INCLUDES = \
  -IInc \
  -I$(CMSIS_CORE_DIR)/Include \
  -I$(CMSIS_DEVICE_DIR)/Include \
  -I$(FREERTOS_DIR)/include \
  -I$(FREERTOS_DIR)/portable/GCC/ARM_CM4F

CFLAGS  = $(MCU_FLAGS) $(DEFS) $(INCLUDES) -std=c11 -Wall -Wextra \
          -ffunction-sections -fdata-sections -fno-common -fno-builtin \
          -Og -g3 -MMD -MP
ASFLAGS = $(MCU_FLAGS) -g3
LDFLAGS = $(MCU_FLAGS) -specs=nosys.specs -specs=nano.specs \
          -Tlinker/STM32F411RETx_FLASH.ld \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
          -Wl,--gc-sections -nostartfiles

# ---- application + driver sources ----
C_SOURCES = \
  Src/main.c \
  Src/system_stm32f4xx.c \
  Src/syscalls.c \
  Src/gpio.c \
  Src/uart.c \
  Src/i2c.c \
  Src/vl53l0x.c \
  Src/stepper_task.c \
  Src/sensor_task.c \
  Src/telemetry_task.c \
  Src/health_task.c \
  Src/command_handler.c \
  Src/data_contract.c

ASM_SOURCES = startup/startup_stm32f411xe.s

# ---- FreeRTOS kernel sources (from the vendored submodule) ----
FREERTOS_SOURCES = \
  $(FREERTOS_DIR)/tasks.c \
  $(FREERTOS_DIR)/queue.c \
  $(FREERTOS_DIR)/list.c \
  $(FREERTOS_DIR)/timers.c \
  $(FREERTOS_DIR)/event_groups.c \
  $(FREERTOS_DIR)/stream_buffer.c \
  $(FREERTOS_DIR)/portable/GCC/ARM_CM4F/port.c \
  $(FREERTOS_DIR)/portable/MemMang/heap_4.c

OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(FREERTOS_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)) $(dir $(FREERTOS_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

.PHONY: all clean flash flash-ocd submodules-check

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

submodules-check:
	@test -f $(CMSIS_CORE_DIR)/Include/core_cm4.h || \
	  (echo "Missing $(CMSIS_CORE_DIR)/Include/core_cm4.h -- run: git submodule update --init --recursive" && exit 1)
	@test -f $(FREERTOS_DIR)/tasks.c || \
	  (echo "Missing $(FREERTOS_DIR)/tasks.c -- run: git submodule update --init --recursive" && exit 1)

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) submodules-check
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

# ---- flashing: documented both ways (ST-Link CLI and OpenOCD) ----
flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x08000000

flash-ocd: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	  -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

-include $(wildcard $(BUILD_DIR)/*.d)
