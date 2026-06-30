#include "command_handler.h"
#include "main.h"
#include "uart.h"
#include "data_contract.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Simple byte-at-a-time framing state machine: USART1 carries both
 * telemetry (from us) and control (to/from us) traffic, but only
 * control_command frames (sof = 0xAB) ever arrive on RX -- the ESP32
 * bot radio only relays control traffic inbound, per the system design
 * in esp32-raw-mac-radio/README.md. We still validate sof defensively. */
typedef enum { WAIT_SOF, COLLECTING } framer_state_t;

static void handle_control_command(const lb_frame_t *f) {
    lb_control_command_t cmd;
    lb_decode_control_command(f, &cmd);

    uint8_t ack_status = 0; /* 0 = ok */

    switch (cmd.cmd_id) {
    case LB_CMD_START_SCAN:
        xSemaphoreTake(xSweepConfigMutex, portMAX_DELAY);
        g_sweep_config.scanning = 1;
        xSemaphoreGive(xSweepConfigMutex);
        break;

    case LB_CMD_STOP_SCAN:
        xSemaphoreTake(xSweepConfigMutex, portMAX_DELAY);
        g_sweep_config.scanning = 0;
        xSemaphoreGive(xSweepConfigMutex);
        break;

    case LB_CMD_SET_SWEEP_RANGE:
        if (cmd.param1 < cmd.param2) {
            xSemaphoreTake(xSweepConfigMutex, portMAX_DELAY);
            g_sweep_config.min_angle_cdeg = (int32_t)cmd.param1;
            g_sweep_config.max_angle_cdeg = (int32_t)cmd.param2;
            xSemaphoreGive(xSweepConfigMutex);
        } else {
            ack_status = 1; /* error: min must be < max */
        }
        break;

    case LB_CMD_PING:
        break; /* ack with status=0 is the whole point of ping */

    default:
        ack_status = 1; /* unknown cmd_id */
        break;
    }

    lb_frame_t ack = {0};
    ack.sof = LB_SOF_CONTROL;
    lb_control_ack_t ack_payload = {
        .cmd_id = cmd.cmd_id,
        .status = ack_status,
        .timestamp_ms = millis(),
    };
    lb_encode_control_ack(&ack, &ack_payload);
    uart1_send_frame(&ack);
}

void vCommandHandlerTask(void *pvParameters) {
    (void)pvParameters;

    uint8_t buf[LB_WIRE_LEN];
    int idx = 0;
    framer_state_t state = WAIT_SOF;

    for (;;) {
        uint8_t byte;
        if (!uart1_read_byte(&byte)) {
            vTaskDelay(pdMS_TO_TICKS(2)); /* nothing to do, yield briefly */
            continue;
        }

        switch (state) {
        case WAIT_SOF:
            if (byte == LB_SOF_CONTROL) {
                buf[0] = byte;
                idx = 1;
                state = COLLECTING;
            }
            /* anything else on RX while waiting for sof is not part of a
             * control frame (it would be our own telemetry looped back,
             * which shouldn't happen on a point-to-point UART, but we
             * stay defensive and just resync rather than asserting). */
            break;

        case COLLECTING:
            buf[idx++] = byte;
            if (idx == LB_WIRE_LEN) {
                lb_frame_t f;
                if (lb_unpack(buf, &f) && f.sof == LB_SOF_CONTROL &&
                    f.type == LB_TYPE_CONTROL_COMMAND) {
                    handle_control_command(&f);
                }
                /* CRC failure or wrong type: drop silently at the
                 * framing layer -- this is exactly the kind of loss the
                 * data contract's CRC is meant to catch; there is no
                 * retransmit mechanism in v1 (documented in README). */
                idx = 0;
                state = WAIT_SOF;
            }
            break;
        }
    }
}
