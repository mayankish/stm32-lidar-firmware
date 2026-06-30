/* telemetry_task.h -- drains xTelemetryQueue (samples, sweep-complete
 * markers, health snapshots) and serializes each into a 16-byte wire
 * frame per the data contract, written out over USART1. */
#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

void vTelemetryTask(void *pvParameters);

#endif /* TELEMETRY_TASK_H */
