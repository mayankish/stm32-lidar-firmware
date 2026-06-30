/* command_handler.h -- parses incoming control_command frames from
 * USART1 RX (bytes fed by USART1_IRQHandler into a ring buffer) and
 * replies with control_ack, per the data contract. Implemented as its
 * own task (either that or folding it into
 * TelemetryTask would work -- a separate task keeps the byte-at-a-time framing
 * state machine out of TelemetryTask's otherwise simple drain loop). */
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

void vCommandHandlerTask(void *pvParameters);

#endif /* COMMAND_HANDLER_H */
