# stm32-lidar-firmware

Bare-metal FreeRTOS firmware for an STM32 Nucleo-F411RE that drives a
stepper-mounted VL53L0X time-of-flight sensor through an oscillating sweep
and streams range samples over UART. This is **Project 1** of a small
multi-repo lidar-mapping robot project I've been building (firmware
here, plus an ESP32 radio bridge, an Android client, and a Python SLAM
dashboard in companion repos). See [`DATA_CONTRACT.md`](DATA_CONTRACT.md)
for the canonical wire-format spec this firmware implements.

## Overview & system role

This board is the "bot" side of the system: it owns the sensor and the
sweep mechanism, has no networking of its own, and talks to everything
else over a single UART link to an ESP32 (`esp32-raw-mac-radio/bot-radio`,
Project 2). It has no idea a network exists — it just emits `scan_sample`
/ `scan_complete` / `health_status` frames and accepts `control_command`
frames, exactly as defined in the data contract.

```
   VL53L0X (I2C1)
        |
   [STM32F411RE] -- USART1 (115200 8N1) --> ESP32 bot-radio --> (raw 802.11) --> ...
        |
   USART2 (debug only, ST-Link VCP)
```

No HAL, no CubeMX-generated code. Every peripheral (GPIO, USART, I2C,
NVIC, the clock tree) is driven directly through CMSIS register
definitions. The only "framework" present is a vendored, unmodified
FreeRTOS kernel.

## Task architecture

![FreeRTOS task architecture: 5 tasks (Stepper, Sensor, Telemetry, Health, CommandHandler) and the queues/mutex connecting them](docs/task_architecture.png)

Five FreeRTOS tasks, two depth-1 "handoff" queues (the deliberate
backpressure mechanism described above), one deeper telemetry queue, and
one mutex guarding the sweep-range config that's written by
`CommandHandlerTask` and read by `StepperTask`.

## Hardware BOM

| Qty | Part | Notes |
|---|---|---|
| 1 | STM32 Nucleo-F411RE | ST part `NUCLEO-F411RE`, on-board ST-Link/V2-1 |
| 1 | VL53L0X ToF breakout | e.g. Adafruit `3317` or Pololu `2490`/`2491`, I2C, 3.3V |
| 1 | Stepper motor, NEMA-17 class | any 200-step/rev (1.8°/step) bipolar stepper |
| 1 | A4988 or DRV8825 stepper driver breakout | STEP/DIR/ENABLE interface assumed identical between the two |
| 1 | Breadboard + jumper wires | |
| 1 | External motor supply (8-35V per driver datasheet) | **do not power the stepper coils from the Nucleo's 5V/3.3V rail** |

## Wiring diagram (ASCII)

```
                +-------------------------+
                |     Nucleo-F411RE       |
                |                         |
   VL53L0X -----| PB8 (I2C1_SCL)          |
   (SCL/SDA) ---| PB9 (I2C1_SDA)          |
   (VIN=3V3) ---| 3V3                     |
   (GND) -------| GND                     |
                |                         |
                | PA0 (STEP) -------------+----> A4988/DRV8825 STEP
                | PA1 (DIR)  -------------+----> A4988/DRV8825 DIR
                | PC1 (ENABLE) -----------+----> A4988/DRV8825 ENABLE
                |                         |
   ESP32 -------| PA9  (USART1_TX) ------>|  (ESP32 bot-radio UART RX)
   bot-radio <--| PA10 (USART1_RX) <------|  (ESP32 bot-radio UART TX)
                |                         |
   ST-Link -----| PA2  (USART2_TX, debug-only, VCP) |
   VCP    <-----| PA3  (USART2_RX, debug-only, VCP) |
                |                         |
                | PA5 (LD2, on-board LED) |
                +-------------------------+

   A4988/DRV8825 motor side: VMOT/GND -> external supply (NOT the Nucleo
   rail), 1A/1B/2A/2B -> stepper coil pairs, MS1=0/MS2=1/MS3=0 tied in
   hardware for 1/4 microstepping (see stepper_task.h comment for why).
```

USART2/PA2/PA3 is reserved for the ST-Link virtual COM port for debug
`printf` only (see `Src/syscalls.c`) — it is never repurposed for the
data-contract link, per this project's pin map.

## Toolchain setup

Tested against **arm-none-eabi-gcc 12.2** ("Arm GNU Toolchain 12.2.Rel1",
the xPack / Arm-maintained build). Any 10.x-13.x release of the same
toolchain family should work unchanged — nothing here depends on a
12.2-specific feature.

```sh
# Debian/Ubuntu
sudo apt install gcc-arm-none-eabi stlink-tools openocd make

# or grab the upstream Arm GNU Toolchain release directly:
# https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
```

Vendored dependencies are git submodules, pinned to these exact tags
(verified live against the upstream repos' GitHub releases/tags as of
the last update to this document):

| Submodule | Path | Upstream | Pinned tag |
|---|---|---|---|
| FreeRTOS-Kernel | `Drivers/FreeRTOS-Kernel` | `FreeRTOS/FreeRTOS-Kernel` | `V11.3.0` |
| CMSIS-Device-F4 | `Drivers/CMSIS-Device-F4` | `STMicroelectronics/cmsis_device_f4` | `v2.6.11` |
| CMSIS Core | `Drivers/CMSIS` | `STMicroelectronics/cmsis_core` | `v5.9.0` |

```sh
git submodule update --init --recursive
```

`make` checks for these (target `submodules-check`) and fails with a
clear message pointing at the command above if they're missing, rather
than failing deep inside a missing-header compile error.

## Build

```sh
make            # build/firmware.elf, .bin, .hex + size report
make clean
```

## Flash

Either of these works; pick whichever you already have installed.

```sh
make flash      # st-flash (stlink-tools), writes .bin at 0x08000000
make flash-ocd   # openocd, programs+verifies+resets via the .elf
```

## Pin map

| Function | Pin(s) | Notes |
|---|---|---|
| I2C1 SCL / SDA | PB8 / PB9 | VL53L0X, AF4, open-drain + external/board pull-ups |
| USART1 TX / RX | PA9 / PA10 | Link to ESP32 bot-radio, 115200 8N1, AF7 |
| USART2 TX / RX | PA2 / PA3 | ST-Link VCP, debug `printf` only — never repurposed |
| Stepper STEP / DIR / ENABLE | PA0 / PA1 / PC1 | To A4988/DRV8825 |
| Status LED (LD2) | PA5 | On-board Nucleo LED; fast blink = fault, slow heartbeat = healthy |

## Data contract summary

Every frame is a 14-byte header+payload struct (`sof`, `type`, `seq`,
10-byte `payload`) plus a trailing 2-byte big-endian CRC16/CCITT-FALSE,
16 bytes total on the wire. This firmware emits `scan_sample` (0x01),
`scan_complete` (0x02), `health_status` (0x03) and consumes
`control_command` (0x10), replying `control_ack` (0x11). See
[`Inc/data_contract.h`](Inc/data_contract.h) for the exact struct layouts
and [`DATA_CONTRACT.md`](DATA_CONTRACT.md) for the full prose spec,
including a documented correction to the original spec's "14 bytes
total" framing (the 14-byte figure is the pre-CRC struct; CRC is
additional).

**If telemetry looks garbled or every packet fails CRC on the receiving
end**, check this exact point first: the CRC must be CRC-16/CCITT-FALSE
(poly `0x1021`, init `0xFFFF`, no reflection, xorout `0x0000`), computed
over bytes 0-13, and transmitted **big-endian** — deliberately the
opposite byte order of the little-endian `seq` field. This mismatch is
the single most likely silent bug across all four language
implementations of this contract.

## File structure

```
stm32-lidar-firmware/
├── Inc/                  headers (drivers, tasks, data contract, FreeRTOSConfig.h)
├── Src/                  implementation (main, drivers, 5 tasks, data contract)
├── startup/               hand-written vector table + Reset_Handler
├── linker/                hand-written FLASH/RAM linker script
├── Drivers/               git submodules: CMSIS, CMSIS-Device-F4, FreeRTOS-Kernel
├── Makefile
├── .gitmodules / .gitignore / LICENSE
```

Source files, by responsibility:

| File | Responsibility |
|---|---|
| `Src/main.c` | Boot, peripheral init, task/queue/mutex creation, scheduler start |
| `Src/system_stm32f4xx.c` | HSI→PLL clock config to 84MHz, `SystemInit()` |
| `Src/gpio.c`, `Src/uart.c`, `Src/i2c.c` | Register-level peripheral drivers, no HAL |
| `Src/vl53l0x.c` | Minimal single-shot VL53L0X ranging sequence |
| `Src/stepper_task.c` | `StepperTask` — sweep state machine, STEP/DIR/ENABLE |
| `Src/sensor_task.c` | `SensorTask` — blocks on Stepper handoff, triggers ranging |
| `Src/telemetry_task.c` | `TelemetryTask` — drains telemetry queue, frames + sends |
| `Src/health_task.c` | `HealthTask` — fault monitoring, LD2, periodic `health_status` |
| `Src/command_handler.c` | Parses inbound `control_command`, replies `control_ack` |
| `Src/data_contract.c` | Wire framing, CRC16, encode/decode (shared logic, host-testable) |
| `Src/syscalls.c` | Retargets libc `_write` to USART2 for debug `printf` |

## Verification

This firmware has **not yet been flashed to physical hardware**
(no STM32 board, VL53L0X, stepper, or driver board attached
at this stage of development). What *was* verified:

- `Src/data_contract.c` / `Inc/data_contract.h` — the only hardware-independent
  module — was compiled and unit-tested against a plain host `gcc`
  (outside this firmware's normal `arm-none-eabi-gcc` toolchain), confirming:
  the CRC16 implementation matches the standard CRC-16/CCITT-FALSE
  catalogue check value (`0x29B1` for ASCII `"123456789"`); full
  pack→unpack round-trips preserve every field; and a single corrupted
  byte is correctly detected as a CRC failure.
- Every other file (drivers, tasks, `main.c`, startup/linker) is written
  exactly as it would be for the real Nucleo-F411RE + VL53L0X + A4988
  hardware described above, following RM0383 register definitions and
  the VL53L0X/A4988 datasheets, but it has **not** been built with
  `arm-none-eabi-gcc` or run on a board as part of producing this repo.

If you have the hardware: `git submodule update --init --recursive`,
`make`, then `make flash`, and watch `health_status` frames / LD2 arrive
on the ESP32 bot-radio's UART within ~2 seconds of boot (the firmware
starts scanning immediately, no `start_scan` command required).

## Known limitations

- **No SPAD calibration**: the VL53L0X driver skips ST's full
  factory-calibration sequence and relies on default SPAD/timing config.
  Functional, but may be less accurate than ST's official API.
- **Blocking I2C**: `i2c1_write`/`i2c1_write_then_read` busy-wait rather
  than using interrupts/DMA. Acceptable because I2C only ever runs inside
  `SensorTask`, which has nothing else to do while waiting.
- **Calibrated busy-wait delays** (`delay_us`/`delay_ms` in
  `stepper_task.c`) are not cycle-exact — fine for stepper pulse timing
  (microsecond-to-millisecond tolerances), not suitable for precision PWM.
- **No stall-detect wiring**: `FAULT_FLAG_STEPPER_STALL` is defined but
  never set — no stall-detect signal is wired from the driver board in
  this revision.
- **`battery_mv` is a documented placeholder** (`0xFFFF`): this project's pin
  map doesn't allocate an ADC channel for battery sensing.
- **No retransmission**: a `control_command` frame that fails CRC is
  silently dropped at the framing layer in `command_handler.c` — there is
  no ack-timeout/retry mechanism in v1.
- HSI+PLL clock source (not HSE) — simpler, fully self-contained bring-up,
  at the cost of slightly worse absolute clock accuracy than a crystal
  reference; irrelevant at UART/I2C timing tolerances used here.

## License

MIT — see [`LICENSE`](LICENSE).
