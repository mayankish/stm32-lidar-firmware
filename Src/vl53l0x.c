#include "vl53l0x.h"
#include "i2c.h"

/* Register addresses, from ST's VL53L0X datasheet / register map. */
#define REG_IDENTIFICATION_MODEL_ID        0xC0u
#define REG_VHV_CONFIG_PAD_SCL_SDA         0x89u
#define REG_SYSRANGE_START                 0x00u
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO   0x0Au
#define REG_SYSTEM_INTERRUPT_CLEAR         0x0Bu
#define REG_RESULT_INTERRUPT_STATUS        0x13u
#define REG_RESULT_RANGE_STATUS            0x14u

#define EXPECTED_MODEL_ID 0xEEu

static int reg_write8(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c1_write(VL53L0X_I2C_ADDR, buf, 2);
}

static int reg_read8(uint8_t reg, uint8_t *val) {
    return i2c1_write_then_read(VL53L0X_I2C_ADDR, &reg, 1, val, 1);
}

static int reg_read_n(uint8_t reg, uint8_t *buf, int n) {
    return i2c1_write_then_read(VL53L0X_I2C_ADDR, &reg, 1, buf, (size_t)n);
}

int vl53l0x_init(void) {
    uint8_t model_id = 0;
    if (reg_read8(REG_IDENTIFICATION_MODEL_ID, &model_id) < 0) return -1;
    if (model_id != EXPECTED_MODEL_ID) return -2;

    /* 2.8V I/O mode -- the sensor breakout boards commonly used with the
     * Nucleo (3.3V logic) need this bit set; see VL53L0X datasheet
     * section on VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV. */
    uint8_t vhv = 0;
    if (reg_read8(REG_VHV_CONFIG_PAD_SCL_SDA, &vhv) < 0) return -1;
    if (reg_write8(REG_VHV_CONFIG_PAD_SCL_SDA, (uint8_t)(vhv | 0x01u)) < 0) return -1;

    /* Configure the GPIO interrupt source to "new sample ready" so
     * RESULT_INTERRUPT_STATUS reflects measurement completion (we poll
     * this register rather than wiring the physical GPIO1 interrupt pin,
     * to keep the pin count down -- documented as a limitation). */
    if (reg_write8(REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04u) < 0) return -1;

    return 0;
}

int32_t vl53l0x_read_range_mm(void) {
    if (reg_write8(REG_SYSRANGE_START, 0x01u) < 0) return -1;

    uint8_t status = 0;
    /* Busy-poll for completion. Bounded loop count, not a wall-clock
     * timeout -- on real hardware this resolves in 30-50ms per the
     * datasheet; SensorTask's caller budget accounts for that. */
    for (uint32_t guard = 2000000u; guard > 0; guard--) {
        if (reg_read8(REG_RESULT_INTERRUPT_STATUS, &status) < 0) return -1;
        if ((status & 0x07u) != 0u) break;
        if (guard == 1) return -1; /* timed out */
    }

    uint8_t range_buf[12];
    if (reg_read_n(REG_RESULT_RANGE_STATUS, range_buf, 12) < 0) return -1;

    if (reg_write8(REG_SYSTEM_INTERRUPT_CLEAR, 0x01u) < 0) return -1;

    /* range_buf[0] low nibble = range status (0 = valid, "VL53L0X_RANGESTATUS_RANGE_VALID").
     * Distance (mm) is a big-endian uint16 at offset 10/11 in this 12-byte
     * read, per the VL53L0X RESULT_RANGE_STATUS register block layout. */
    uint8_t range_status = range_buf[0] & 0x0Fu;
    uint16_t distance_mm = (uint16_t)((range_buf[10] << 8) | range_buf[11]);

    if (range_status != 0u) {
        return (int32_t)VL53L0X_OUT_OF_RANGE;
    }
    return (int32_t)distance_mm;
}
