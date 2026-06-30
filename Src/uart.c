#include "uart.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"

/* APB clock assumptions, set by system_stm32f4xx.c's SystemClock_Config():
 *   USART1 is on APB2 @ 84 MHz (no prescaler from the 84 MHz AHB).
 *   USART2 is on APB1 @ 42 MHz (APB1 prescaler /2 from 84 MHz AHB).
 * Both are needed to compute the BRR (baud rate register) divisor. */
#define APB2_HZ 84000000UL
#define APB1_HZ 42000000UL

#define UART1_RX_BUF_LEN 128
static volatile uint8_t  uart1_rx_buf[UART1_RX_BUF_LEN];
static volatile uint16_t uart1_rx_head; /* written by ISR */
static volatile uint16_t uart1_rx_tail; /* read by consumer */
static volatile uint32_t uart1_rx_drops; /* incremented on ring-buffer overflow */

static void usart_set_baud(USART_TypeDef *u, uint32_t periph_clk_hz, uint32_t baud) {
    /* Oversampling by 16 (default, OVER8=0): USARTDIV = periph_clk / (16*baud),
     * stored as a 16-bit fixed point value (12-bit mantissa, 4-bit fraction). */
    uint32_t usartdiv_x100 = (periph_clk_hz * 100u) / (16u * baud);
    uint32_t mantissa = usartdiv_x100 / 100u;
    uint32_t frac_x100 = usartdiv_x100 - (mantissa * 100u);
    uint32_t fraction = (frac_x100 * 16u + 50u) / 100u; /* round to nearest /16th */
    if (fraction > 15u) { fraction = 0u; mantissa += 1u; }
    u->BRR = (uint16_t)((mantissa << 4) | fraction);
}

void uart1_init(uint32_t baud) {
    gpio_clock_enable(GPIOA);
    /* PA9 = USART1_TX, PA10 = USART1_RX, AF7 */
    gpio_configure(GPIOA, 9,  GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_PUPD_NONE, 7);
    gpio_configure(GPIOA, 10, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_PUPD_UP,   7);

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    __DSB();

    USART1->CR1 = 0; /* disable while configuring */
    usart_set_baud(USART1, APB2_HZ, baud);
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    USART1->CR2 = 0; /* 1 stop bit */
    USART1->CR3 = 0; /* no flow control */
    USART1->CR1 |= USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, 6); /* below configMAX_SYSCALL_INTERRUPT_PRIORITY's group */
    NVIC_EnableIRQ(USART1_IRQn);
}

void uart2_init(uint32_t baud) {
    gpio_clock_enable(GPIOA);
    /* PA2 = USART2_TX, PA3 = USART2_RX (RX unused but configured for
     * completeness/symmetry -- this pin pair is reserved for the
     * ST-Link VCP per this project's pin map and must not be repurposed). */
    gpio_configure(GPIOA, 2, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_PUPD_NONE, 7);
    gpio_configure(GPIOA, 3, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_PUPD_UP,   7);

    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    __DSB();

    USART2->CR1 = 0;
    usart_set_baud(USART2, APB1_HZ, baud);
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE;
    USART2->CR2 = 0;
    USART2->CR3 = 0;
    USART2->CR1 |= USART_CR1_UE;
}

void uart1_write(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        while (!(USART1->SR & USART_SR_TXE)) { /* wait */ }
        USART1->DR = data[i];
    }
    while (!(USART1->SR & USART_SR_TC)) { /* wait for last byte to clear the line */ }
}

void uart2_write(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        while (!(USART2->SR & USART_SR_TXE)) { /* wait */ }
        USART2->DR = data[i];
    }
    while (!(USART2->SR & USART_SR_TC)) { /* wait */ }
}

int uart1_read_byte(uint8_t *out) {
    if (uart1_rx_tail == uart1_rx_head) {
        return 0; /* empty */
    }
    *out = uart1_rx_buf[uart1_rx_tail];
    uart1_rx_tail = (uint16_t)((uart1_rx_tail + 1) % UART1_RX_BUF_LEN);
    return 1;
}

void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t byte = (uint8_t)USART1->DR; /* reading DR clears RXNE */
        uint16_t next_head = (uint16_t)((uart1_rx_head + 1) % UART1_RX_BUF_LEN);
        if (next_head != uart1_rx_tail) {
            uart1_rx_buf[uart1_rx_head] = byte;
            uart1_rx_head = next_head;
        } else {
            /* Ring buffer full -- an ISR cannot block waiting for
             * command_handler to catch up, so the byte is dropped. It is
             * NOT silent, though: the counter below is surfaced in
             * health_status.fault_flags (FAULT_FLAG_UART_RX_DROP), which
             * is exactly the "observable, not invisible" behaviour I
             * wanted around dropped data. */
            uart1_rx_drops++;
        }
    }
    /* Overrun (ORE) clears itself by reading SR then DR, which the RXNE
     * path above already does on every interrupt entry. */
}

uint32_t uart1_rx_drop_count(void) {
    return uart1_rx_drops;
}

static uint16_t g_uart1_tx_seq;

void uart1_send_frame(lb_frame_t *frame) {
    taskENTER_CRITICAL();
    frame->seq = g_uart1_tx_seq++;
    taskEXIT_CRITICAL();

    uint8_t wire[LB_WIRE_LEN];
    lb_pack(frame, wire);
    uart1_write(wire, LB_WIRE_LEN);
}
