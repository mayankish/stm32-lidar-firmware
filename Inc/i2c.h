/* i2c.h -- minimal, polling/blocking I2C1 master driver, no HAL.
 *
 * Used exclusively for the VL53L0X ToF sensor on PB8 (SCL) / PB9 (SDA).
 * Blocking by design: each transaction is microseconds to low-single-digit
 * milliseconds, and it only ever runs inside SensorTask, which has nothing
 * else to do while waiting on the sensor anyway. See
 * stm32-lidar-firmware/README.md "Known limitations" for why this isn't
 * interrupt/DMA driven.
 */
#ifndef I2C_H
#define I2C_H

#include "stm32f4xx.h"
#include <stddef.h>

void i2c1_init(void);

/* Returns 0 on success, negative on NACK/timeout. */
int i2c1_write(uint8_t addr7, const uint8_t *data, size_t len);
int i2c1_write_then_read(uint8_t addr7, const uint8_t *wdata, size_t wlen,
                          uint8_t *rdata, size_t rlen);

#endif /* I2C_H */
