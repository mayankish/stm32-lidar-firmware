#include "stepper_task.h"
#include "main.h"
#include "gpio.h"
#include "stm32f4xx.h"

#define PIN_STEP_PORT GPIOA
#define PIN_STEP_PIN  0
#define PIN_DIR_PORT  GPIOA
#define PIN_DIR_PIN   1
#define PIN_EN_PORT   GPIOC
#define PIN_EN_PIN    1

/* Calibrated busy-wait delay. Not cycle-exact (depends on compiler
 * optimization level and flash wait states) -- documented as an
 * intentional simplification in README "Known limitations". Stepper
 * pulse timing tolerances here are generous (microseconds to low
 * milliseconds) so this is adequate; it would not be adequate for, say,
 * precision PWM. */
extern uint32_t SystemCoreClock;
static void delay_us(uint32_t us) {
    uint32_t loops = (SystemCoreClock / 4000000u) * us;
    while (loops--) {
        __NOP();
    }
}
static void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000);
    }
}

static void stepper_gpio_init(void) {
    gpio_clock_enable(PIN_STEP_PORT);
    gpio_clock_enable(PIN_DIR_PORT);
    gpio_clock_enable(PIN_EN_PORT);

    gpio_configure(PIN_STEP_PORT, PIN_STEP_PIN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_PUPD_NONE, 0);
    gpio_configure(PIN_DIR_PORT,  PIN_DIR_PIN,  GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_PUPD_NONE, 0);
    gpio_configure(PIN_EN_PORT,   PIN_EN_PIN,   GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_PUPD_NONE, 0);

    gpio_clear(PIN_STEP_PORT, PIN_STEP_PIN);
    gpio_clear(PIN_DIR_PORT, PIN_DIR_PIN);   /* DIR=0 -> "forward" by convention */
    gpio_clear(PIN_EN_PORT, PIN_EN_PIN);     /* ENABLE is active-low -- clear to enable the driver */
}

static void issue_microstep(void) {
    gpio_set(PIN_STEP_PORT, PIN_STEP_PIN);
    delay_us(STEP_PULSE_HIGH_US);
    gpio_clear(PIN_STEP_PORT, PIN_STEP_PIN);
    delay_us(INTER_MICROSTEP_DELAY_US);
}

void vStepperTask(void *pvParameters) {
    (void)pvParameters;
    stepper_gpio_init();

    int32_t angle_cdeg = g_sweep_config.min_angle_cdeg;
    uint8_t dir_fwd = 1; /* 1 = increasing angle ("fwd"), 0 = decreasing ("rev") */
    gpio_clear(PIN_DIR_PORT, PIN_DIR_PIN);

    for (;;) {
        int32_t min_a, max_a;
        uint8_t scanning;

        xSemaphoreTake(xSweepConfigMutex, portMAX_DELAY);
        min_a = g_sweep_config.min_angle_cdeg;
        max_a = g_sweep_config.max_angle_cdeg;
        scanning = g_sweep_config.scanning;
        xSemaphoreGive(xSweepConfigMutex);

        if (!scanning) {
            vTaskDelay(pdMS_TO_TICKS(50)); /* idle, wait for start_scan */
            continue;
        }

        /* Clamp current angle into the (possibly just-changed) range. */
        if (angle_cdeg < min_a) angle_cdeg = min_a;
        if (angle_cdeg > max_a) angle_cdeg = max_a;

        for (int i = 0; i < MICROSTEPS_PER_SAMPLE; i++) {
            issue_microstep();
        }
        angle_cdeg += dir_fwd ? SAMPLE_ANGLE_STEP_CDEG : -SAMPLE_ANGLE_STEP_CDEG;

        delay_ms(SETTLE_DELAY_MS);

        /* Hand off to SensorTask -- blocks here if Sensor hasn't drained
         * the previous angle yet, which is the deliberate backpressure
         * mechanism described in the README (Stepper moves fast, the
         * VL53L0X conversion is what actually paces the system). */
        int32_t angle_to_send = angle_cdeg;
        xQueueSend(xAngleQueue, &angle_to_send, portMAX_DELAY);

        if ((dir_fwd && angle_cdeg >= max_a) || (!dir_fwd && angle_cdeg <= min_a)) {
            dir_fwd = !dir_fwd;
            gpio_toggle(PIN_DIR_PORT, PIN_DIR_PIN);

            telemetry_msg_t msg = {0};
            msg.kind = TM_SWEEP_COMPLETE;
            msg.sweep_dir = dir_fwd ? 0u : 1u; /* report the direction we just switched TO */
            msg.timestamp_ms = millis();
            xQueueSend(xTelemetryQueue, &msg, portMAX_DELAY);
        }
    }
}
