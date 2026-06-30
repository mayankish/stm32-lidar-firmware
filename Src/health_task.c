#include "health_task.h"
#include "main.h"
#include "uart.h"
#include "gpio.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

static uint32_t last_i2c_fault_seen = 0;

void vHealthTask(void *pvParameters) {
    (void)pvParameters;

    gpio_clock_enable(LED_PORT);
    gpio_configure(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_PUPD_NONE, 0);

    for (;;) {
        uint16_t fault_flags = 0;

        uint32_t i2c_faults_now = g_i2c_fault_count;
        if (i2c_faults_now != last_i2c_fault_seen) {
            fault_flags |= FAULT_FLAG_I2C;
            last_i2c_fault_seen = i2c_faults_now;
        }
        if (uart1_rx_drop_count() > 0) {
            fault_flags |= FAULT_FLAG_UART_RX_DROP;
        }
        /* FAULT_FLAG_STEPPER_STALL intentionally never set -- no
         * stall-detect signal is wired in this revision (wiring one up
         * is listed as a conditional future option: "if you wire a
         * stall-detect signal"). See README "Known limitations". */

        if (fault_flags != 0) {
            /* fast blink = fault present */
            gpio_toggle(LED_PORT, LED_PIN);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_toggle(LED_PORT, LED_PIN);
        } else {
            /* slow heartbeat blink = healthy */
            gpio_toggle(LED_PORT, LED_PIN);
        }

        telemetry_msg_t msg = {0};
        msg.kind = TM_HEALTH;
        msg.fault_flags = fault_flags;
        msg.battery_mv = BATTERY_MV_PLACEHOLDER; /* see main.h comment: no ADC pin allocated for battery sense */
        msg.timestamp_ms = millis();
        xQueueSend(xTelemetryQueue, &msg, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(HEALTH_PERIOD_MS));
    }
}
