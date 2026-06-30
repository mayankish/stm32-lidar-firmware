#include "i2c.h"
#include "gpio.h"

#define I2C1_TIMEOUT_LOOPS 100000u /* crude busy-wait timeout; see README */

static int wait_flag(volatile uint32_t *reg, uint32_t mask) {
    uint32_t guard = I2C1_TIMEOUT_LOOPS;
    while (!(*reg & mask)) {
        if (--guard == 0) return -1;
    }
    return 0;
}

void i2c1_init(void) {
    gpio_clock_enable(GPIOB);
    /* PB8 = I2C1_SCL, PB9 = I2C1_SDA -- open-drain + pull-up, AF4 */
    gpio_configure(GPIOB, 8, GPIO_MODE_AF, GPIO_OTYPE_OD, GPIO_PUPD_UP, 4);
    gpio_configure(GPIOB, 9, GPIO_MODE_AF, GPIO_OTYPE_OD, GPIO_PUPD_UP, 4);

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    __DSB();

    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;

    /* APB1 = 42 MHz (see system_stm32f4xx.c) */
    I2C1->CR2 = 42; /* FREQ field, MHz */

    /* Standard mode, 100 kHz: CCR = Fpclk1 / (2 * Fscl) */
    I2C1->CCR = (uint16_t)(42000000UL / (2u * 100000UL));
    I2C1->TRISE = 43; /* (1000ns / (1/42MHz)) + 1, per RM0383 example for 100kHz @ 42MHz */

    I2C1->CR1 |= I2C_CR1_PE;
}

static int i2c1_start(uint8_t addr7, int read) {
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_flag(&I2C1->SR1, I2C_SR1_SB) < 0) return -1;

    I2C1->DR = (uint8_t)((addr7 << 1) | (read ? 1u : 0u));
    uint32_t guard = I2C1_TIMEOUT_LOOPS;
    while (!(I2C1->SR1 & I2C_SR1_ADDR)) {
        if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= ~I2C_SR1_AF; return -2; } /* NACK */
        if (--guard == 0) return -1;
    }
    (void)I2C1->SR2; /* clear ADDR by reading SR1 then SR2 */
    return 0;
}

static void i2c1_stop(void) {
    I2C1->CR1 |= I2C_CR1_STOP;
}

int i2c1_write(uint8_t addr7, const uint8_t *data, size_t len) {
    if (i2c1_start(addr7, 0) < 0) { i2c1_stop(); return -1; }
    for (size_t i = 0; i < len; i++) {
        if (wait_flag(&I2C1->SR1, I2C_SR1_TXE) < 0) { i2c1_stop(); return -1; }
        I2C1->DR = data[i];
    }
    if (wait_flag(&I2C1->SR1, I2C_SR1_BTF) < 0) { i2c1_stop(); return -1; }
    i2c1_stop();
    return 0;
}

int i2c1_write_then_read(uint8_t addr7, const uint8_t *wdata, size_t wlen,
                          uint8_t *rdata, size_t rlen) {
    if (wlen > 0) {
        if (i2c1_start(addr7, 0) < 0) { i2c1_stop(); return -1; }
        for (size_t i = 0; i < wlen; i++) {
            if (wait_flag(&I2C1->SR1, I2C_SR1_TXE) < 0) { i2c1_stop(); return -1; }
            I2C1->DR = wdata[i];
        }
        if (wait_flag(&I2C1->SR1, I2C_SR1_BTF) < 0) { i2c1_stop(); return -1; }
    }

    if (i2c1_start(addr7, 1) < 0) { i2c1_stop(); return -1; }

    I2C1->CR1 |= I2C_CR1_ACK;
    for (size_t i = 0; i < rlen; i++) {
        if (i == rlen - 1) {
            I2C1->CR1 &= ~I2C_CR1_ACK; /* NACK the last byte */
            i2c1_stop();
        }
        if (wait_flag(&I2C1->SR1, I2C_SR1_RXNE) < 0) return -1;
        rdata[i] = (uint8_t)I2C1->DR;
    }
    return 0;
}
