/* vl53l0x.h -- minimal single-shot ranging driver for the ST VL53L0X ToF
 * sensor over I2C1.
 *
 * This is NOT a port of ST's full VL53L0X API. It skips the SPAD
 * reference calibration and full tuning-register sequence that the
 * official API performs at init, and relies on factory default
 * SPAD/timing configuration instead. Functional single-shot ranging
 * works correctly with this minimal sequence (it is the same approach
 * used by several well-known minimal open-source VL53L0X drivers), but
 * absolute accuracy may be slightly worse than ST's reference
 * implementation. This trade-off is documented in the project README's
 * "Known limitations" section -- it is a deliberate scope cut, not an
 * oversight.
 */
#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>

#define VL53L0X_I2C_ADDR 0x29u
#define VL53L0X_OUT_OF_RANGE 0xFFFFu /* matches the data contract's sentinel */

/* Returns 0 on success (model ID matches the expected VL53L0X value),
 * negative on I2C error or unexpected model ID. */
int vl53l0x_init(void);

/* Triggers one single-shot range measurement, blocks until the result is
 * ready (typically 30-50ms per the datasheet's stated conversion time), and
 * returns the distance in mm, or VL53L0X_OUT_OF_RANGE if the sensor
 * reported a range status other than "valid". Returns a negative value
 * only on I2C communication failure (distinct from "out of range"). */
int32_t vl53l0x_read_range_mm(void);

#endif /* VL53L0X_H */
