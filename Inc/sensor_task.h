/* sensor_task.h -- blocks on StepperTask's angle handoff, triggers a
 * VL53L0X range read, and forwards {angle, distance, timestamp} to
 * TelemetryTask. */
#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

void vSensorTask(void *pvParameters);

#endif /* SENSOR_TASK_H */
