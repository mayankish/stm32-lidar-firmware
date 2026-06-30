/* health_task.h -- lowest-priority task: blinks LD2 to reflect fault
 * state and periodically emits health_status telemetry. */
#ifndef HEALTH_TASK_H
#define HEALTH_TASK_H

#define HEALTH_PERIOD_MS 2000u
#define LED_PORT GPIOA
#define LED_PIN  5

void vHealthTask(void *pvParameters);

#endif /* HEALTH_TASK_H */
