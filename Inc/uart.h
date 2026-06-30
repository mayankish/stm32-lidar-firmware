/* uart.h -- minimal register-level USART driver, no HAL.
 *
 * USART1 (PA9 TX / PA10 RX) is the link to the ESP32 bot radio: it carries
 * outbound telemetry frames and inbound control_command/control_ack
 * frames per the data contract. Baud rate MUST match
 * esp32-raw-mac-radio/bot-radio exactly -- both sides are documented as
 * 115200 8N1 in this project's README and the ESP32 project's README.
 *
 * USART2 (PA2 TX / PA3 RX, routed to the ST-Link virtual COM port) is
 * debug printf only, per this project's pin map -- never used for the
 * data-contract link.
 */
#ifndef UART_H
#define UART_H

#include "stm32f4xx.h"
#include "data_contract.h"
#include <stddef.h>

void uart1_init(uint32_t baud);
void uart2_init(uint32_t baud);

/* Blocking, byte-at-a-time TX (fine at our packet rates: a handful of
 * 16-byte frames per second). */
void uart1_write(const uint8_t *data, size_t len);
void uart2_write(const uint8_t *data, size_t len);

/* Non-blocking read of one byte from the USART1 RX ring buffer, filled by
 * USART1_IRQHandler. Returns 1 and writes *out if a byte was available,
 * 0 otherwise. */
int uart1_read_byte(uint8_t *out);

/* Count of bytes dropped because the USART1 RX ring buffer was full --
 * makes ISR-level drops observable to HealthTask instead of silently
 * disappearing (see USART1_IRQHandler in uart.c). */
uint32_t uart1_rx_drop_count(void);

/* Assigns the next sequence number (shared, monotonically increasing
 * across every frame this device sends on USART1 -- telemetry AND
 * control_ack alike, since they're all on the same physical link and
 * there's no need for separate sequence spaces per sof category), packs
 * the frame, and writes it out. Safe to call from multiple tasks
 * (TelemetryTask and the command-handling task both do). */
void uart1_send_frame(lb_frame_t *frame);

#endif /* UART_H */
