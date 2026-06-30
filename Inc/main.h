/* main.h -- shared system state: queues, semaphores, sweep config, and
 * fault counters that more than one task/module needs to see. */
#ifndef MAIN_H
#define MAIN_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include <stdint.h>

/* ---- task priorities (configMAX_PRIORITIES = 6, see FreeRTOSConfig.h) ---- */
#define PRIO_SENSOR     4
#define PRIO_STEPPER    3
#define PRIO_TELEMETRY  2
#define PRIO_COMMAND    2
#define PRIO_HEALTH     1

/* ---- inter-task messages ---- */

/* StepperTask -> SensorTask: "settled at this angle, take a reading."
 * Depth-1 queue: SensorTask is always waiting for the next one before
 * Stepper produces it, by construction of the sweep state machine. */
extern QueueHandle_t xAngleQueue; /* holds int32_t angle_cdeg */

/* Message kinds queued to TelemetryTask. */
typedef enum { TM_SAMPLE = 0, TM_SWEEP_COMPLETE = 1, TM_HEALTH = 2 } telemetry_msg_kind_t;

typedef struct {
    telemetry_msg_kind_t kind;
    int32_t  angle_cdeg;     /* TM_SAMPLE */
    int32_t  distance_mm;    /* TM_SAMPLE */
    uint8_t  sweep_dir;      /* TM_SWEEP_COMPLETE */
    uint16_t fault_flags;    /* TM_HEALTH */
    uint16_t battery_mv;     /* TM_HEALTH */
    uint32_t timestamp_ms;
} telemetry_msg_t;

extern QueueHandle_t xTelemetryQueue;

/* ---- sweep configuration, mutated by command_handler on
 * control_command(set_sweep_range), read by StepperTask. Guarded by
 * xSweepConfigMutex because it's written from CommandHandler's task
 * context and read from StepperTask's -- both can run on either core...
 * well, there's one core here, but they're still two separate tasks, so
 * the mutex matters under preemption. ---- */
typedef struct {
    int32_t min_angle_cdeg;
    int32_t max_angle_cdeg;
    uint8_t scanning; /* 0 = stopped (start_scan/stop_scan), 1 = running */
} sweep_config_t;

extern sweep_config_t g_sweep_config;
extern SemaphoreHandle_t xSweepConfigMutex;

/* ---- fault counters, single-producer/single-consumer plain uint32_t.
 * On Cortex-M4 a 32-bit aligned load/store is atomic with respect to a
 * single writer + single reader, which is all these are used for, so no
 * extra locking is added -- see README "Known limitations" for the
 * reasoning spelled out. ---- */
extern volatile uint32_t g_i2c_fault_count;

/* fault_flags bit assignments reported in health_status frames */
#define FAULT_FLAG_I2C            (1u << 0)
#define FAULT_FLAG_STEPPER_STALL  (1u << 1)  /* never set: no stall-detect wired, see README */
#define FAULT_FLAG_UART_RX_DROP   (1u << 2)

/* battery_mv is reported as a documented placeholder -- this project's pin
 * map does not allocate an ADC channel for battery sense, so this is not
 * wired to real hardware. See README "Testing" section. */
#define BATTERY_MV_PLACEHOLDER 0xFFFFu

uint32_t millis(void); /* wraps xTaskGetTickCount(), tick = 1ms */

#endif /* MAIN_H */
