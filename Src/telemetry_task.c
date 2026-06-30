#include "telemetry_task.h"
#include "main.h"
#include "uart.h"
#include "data_contract.h"
#include "FreeRTOS.h"
#include "queue.h"

static void send_frame(lb_frame_t *f) {
    f->sof = LB_SOF_TELEMETRY;
    uart1_send_frame(f); /* assigns seq from the shared USART1 tx counter */
}

void vTelemetryTask(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        telemetry_msg_t msg;
        xQueueReceive(xTelemetryQueue, &msg, portMAX_DELAY);

        lb_frame_t f = {0};

        switch (msg.kind) {
        case TM_SAMPLE: {
            lb_scan_sample_t s;
            s.angle_cdeg   = (uint16_t)msg.angle_cdeg;
            s.distance_mm  = (uint16_t)msg.distance_mm;
            s.timestamp_ms = msg.timestamp_ms;
            lb_encode_scan_sample(&f, &s);
            break;
        }
        case TM_SWEEP_COMPLETE: {
            lb_scan_complete_t s;
            s.sweep_dir    = msg.sweep_dir;
            s.timestamp_ms = msg.timestamp_ms;
            lb_encode_scan_complete(&f, &s);
            break;
        }
        case TM_HEALTH: {
            lb_health_status_t s;
            s.fault_flags  = msg.fault_flags;
            s.battery_mv   = msg.battery_mv;
            s.timestamp_ms = msg.timestamp_ms;
            lb_encode_health_status(&f, &s);
            break;
        }
        default:
            continue; /* unknown message kind, drop */
        }

        send_frame(&f);
    }
}
