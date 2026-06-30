#include "sensor_task.h"
#include "main.h"
#include "vl53l0x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

void vSensorTask(void *pvParameters) {
    (void)pvParameters;

    int retry = 0;
    while (vl53l0x_init() != 0) {
        g_i2c_fault_count++;
        retry++;
        vTaskDelay(pdMS_TO_TICKS(200));
        if (retry > 25) {
            /* Sensor never came up. Keep retrying forever rather than
             * silently pretending it's fine -- HealthTask's fault_flags
             * will reflect g_i2c_fault_count climbing, which is the
             * "observable, not invisible" behaviour this design calls for. */
            retry = 0;
        }
    }

    for (;;) {
        int32_t angle_cdeg;
        xQueueReceive(xAngleQueue, &angle_cdeg, portMAX_DELAY);

        int32_t distance_mm = vl53l0x_read_range_mm();
        if (distance_mm < 0) {
            g_i2c_fault_count++;
            distance_mm = (int32_t)VL53L0X_OUT_OF_RANGE;
        }

        telemetry_msg_t msg = {0};
        msg.kind = TM_SAMPLE;
        msg.angle_cdeg = angle_cdeg;
        msg.distance_mm = distance_mm;
        msg.timestamp_ms = millis();
        xQueueSend(xTelemetryQueue, &msg, portMAX_DELAY);
    }
}
