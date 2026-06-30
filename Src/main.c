/*
 * main.c -- entry point: bring up peripherals, create the 5 FreeRTOS
 * tasks + their shared queues/mutex, start the scheduler.
 *
 * Task set:
 *   StepperTask        - owns the sweep state machine + STEP/DIR/ENABLE
 *   SensorTask         - blocks on Stepper's angle handoff, reads VL53L0X
 *   TelemetryTask      - drains the telemetry queue, frames + sends on UART1
 *   HealthTask         - lowest priority, fault monitoring + LD2 + periodic health_status
 *   CommandHandlerTask - parses control_command from USART1 RX, sends control_ack
 */
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "main.h"
#include "uart.h"
#include "i2c.h"
#include "stepper_task.h"
#include "sensor_task.h"
#include "telemetry_task.h"
#include "health_task.h"
#include "command_handler.h"

/* ---- definitions for the externs declared in main.h ---- */
QueueHandle_t xAngleQueue;
QueueHandle_t xTelemetryQueue;
sweep_config_t g_sweep_config;
SemaphoreHandle_t xSweepConfigMutex;
volatile uint32_t g_i2c_fault_count;

/* UART1<->ESP32 link baud rate. MUST match esp32-raw-mac-radio/bot-radio's
 * UART config exactly -- documented in both projects' READMEs and in
 * DATA_CONTRACT.md. 115200 8N1 is plenty for a handful of 16-byte frames
 * per second and is the most common default on both an STM32 USART and an
 * ESP-IDF UART driver, so it was picked as the path of least surprise. */
#define UART1_BAUD 115200u
/* UART2 is debug-only (ST-Link VCP); baud here only needs to match
 * whatever serial terminal the developer opens locally. */
#define UART2_BAUD 115200u

uint32_t millis(void) {
    return (uint32_t)(xTaskGetTickCount() * (1000u / configTICK_RATE_HZ));
}

/* Depth-1 "handoff" queues: each is intentionally sized to 1, NOT to save
 * RAM, but as the backpressure mechanism described throughout this
 * project -- a producer blocks on xQueueSend until the consumer has
 * drained the previous item, so the system self-paces to the VL53L0X's
 * conversion time rather than letting samples pile up. The telemetry
 * queue is deeper (8) since TelemetryTask's UART1 write is fast relative
 * to its three producers and a brief burst (e.g. a sweep-reversal
 * scan_complete landing right after a sample) shouldn't have to block
 * StepperTask or HealthTask. */
#define ANGLE_QUEUE_LEN     1
#define TELEMETRY_QUEUE_LEN 8

static void init_shared_state(void) {
    xAngleQueue = xQueueCreate(ANGLE_QUEUE_LEN, sizeof(int32_t));
    xTelemetryQueue = xQueueCreate(TELEMETRY_QUEUE_LEN, sizeof(telemetry_msg_t));
    xSweepConfigMutex = xSemaphoreCreateMutex();
    configASSERT(xAngleQueue != NULL);
    configASSERT(xTelemetryQueue != NULL);
    configASSERT(xSweepConfigMutex != NULL);

    g_sweep_config.min_angle_cdeg = DEFAULT_MIN_ANGLE_CDEG;
    g_sweep_config.max_angle_cdeg = DEFAULT_MAX_ANGLE_CDEG;
    g_sweep_config.scanning = 1; /* scan immediately on boot -- no explicit
                                    start_scan needed to see telemetry on
                                    a freshly flashed board; a dashboard
                                    can still stop_scan/start_scan later. */
    g_i2c_fault_count = 0;
}

int main(void) {
    /* SystemInit() (clock to 84MHz) already ran from Reset_Handler before
     * we got here, per the standard CMSIS startup contract. */

    uart2_init(UART2_BAUD); /* debug VCP first, so any early fault is visible */
    uart1_init(UART1_BAUD); /* ESP32 link */
    i2c1_init();

    init_shared_state();

    xTaskCreate(vStepperTask,        "Stepper", configMINIMAL_STACK_SIZE * 2, NULL, PRIO_STEPPER,    NULL);
    xTaskCreate(vSensorTask,         "Sensor",  configMINIMAL_STACK_SIZE * 2, NULL, PRIO_SENSOR,     NULL);
    xTaskCreate(vTelemetryTask,      "Telem",   configMINIMAL_STACK_SIZE * 2, NULL, PRIO_TELEMETRY,  NULL);
    xTaskCreate(vHealthTask,         "Health",  configMINIMAL_STACK_SIZE * 2, NULL, PRIO_HEALTH,     NULL);
    xTaskCreate(vCommandHandlerTask, "Command", configMINIMAL_STACK_SIZE * 2, NULL, PRIO_COMMAND,    NULL);

    vTaskStartScheduler();

    /* vTaskStartScheduler() only returns if there isn't enough heap for
     * the idle/timer tasks -- should be unreachable with a 20KB heap_4
     * and this task set, but trap here instead of running off into
     * undefined behaviour if it ever does. */
    for (;;) { }
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask; (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
