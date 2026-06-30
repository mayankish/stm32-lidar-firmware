/* stepper_task.h -- owns the oscillating sweep state machine and the
 * A4988/DRV8825 STEP/DIR/ENABLE lines. */
#ifndef STEPPER_TASK_H
#define STEPPER_TASK_H

#include "FreeRTOS.h"
#include "task.h"

/* 1/4 microstepping (A4988 MS1=0,MS2=1,MS3=0 -- tie these in hardware,
 * see README wiring table): 200 full steps/rev * 4 = 800 microsteps/rev.
 * 360.00deg / 800 microsteps = 0.45deg/microstep = 45 centidegrees/microstep
 * exactly, so all angle bookkeeping below is exact integer arithmetic in
 * centidegrees -- no rounding error accumulates over a sweep. */
#define STEP_ANGLE_CDEG_PER_MICROSTEP 45

/* Two microsteps per reported sample -> 0.90deg angular resolution.
 * Chosen against the VL53L0X's ~30-50ms
 * conversion time: at 0.45deg resolution (1 microstep/sample) a 270deg
 * one-way sweep is 600 samples * ~40ms = ~24s per pass, which is simply
 * slow but workable; at 0.90deg it's 300 samples * ~40ms = ~12s per pass.
 * 0.90deg is the default tradeoff of resolution vs. sweep time; both are
 * exact multiples of the 45 cdeg/microstep unit so neither chmoice
 * introduces rounding. Override via control_command(set_sweep_range) only
 * changes the swept window, not this resolution -- see README if you want
 * to make resolution runtime-configurable too (documented as a possible
 * follow-up, not implemented in v1). */
#define MICROSTEPS_PER_SAMPLE 2
#define SAMPLE_ANGLE_STEP_CDEG (MICROSTEPS_PER_SAMPLE * STEP_ANGLE_CDEG_PER_MICROSTEP)

#define DEFAULT_MIN_ANGLE_CDEG 0
#define DEFAULT_MAX_ANGLE_CDEG 27000 /* 270.00 degrees */

#define STEP_PULSE_HIGH_US     5     /* A4988 STEP min high time is ~1us; 5us is safety margin */
#define INTER_MICROSTEP_DELAY_US 1000 /* paces microstep rate -> motor speed */
#define SETTLE_DELAY_MS        5     /* vibration settle before triggering a range read */

void vStepperTask(void *pvParameters);

#endif /* STEPPER_TASK_H */
