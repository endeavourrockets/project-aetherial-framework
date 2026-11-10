/*
 * attitude.h - orientation of the payload as a quaternion, from the raw IMU.
 *
 * Team direction (2026-10): use raw accelerometer + gyroscope data and our
 * own filter on the microcontroller, not a sensor's on-chip fusion (which
 * limits ranges and is poor at high g). Needed for science: the field mill
 * measures the field relative to the rocket, so turning that into a world
 * direction needs the rocket's orientation.
 *
 * Method: Mahony complementary filter.
 *   - the gyroscope is integrated every IMU sample (q' = 0.5 q * w);
 *   - the accelerometer pulls the tilt back towards "gravity is down" with
 *     gain ATT_KP, but ONLY while it reads 1 g within ATT_ACC_GATE_MG.
 *     Under thrust, in free fall (coast) or under a parachute the
 *     accelerometer does not show gravity, so the filter runs on the
 *     gyroscope alone: drift grows with time and gyro bias.
 *   - initial tilt from the accelerometer on the pad; yaw (heading) starts
 *     at 0 and is NOT observable without a magnetometer.
 *
 * Frames: body = IMU axes; world = local level, +Z up. q rotates body
 * vectors into the world frame, q = (w, x, y, z), |q| = 1.
 *
 * TBD: gyro bias calibration on the pad, a magnetometer for heading, and
 * checking the drift against flight-like data.
 */
#ifndef ATTITUDE_H
#define ATTITUDE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float q[4];                   /* w, x, y, z */
    bool  init;                   /* tilt initialised from the accelerometer */
} attitude_t;

void attitude_init(attitude_t *a);
/* One IMU sample: accel in mg, gyro in 0.1 deg/s, dt in seconds. */
void attitude_update(attitude_t *a, const int16_t accel_mg[3], const int16_t gyro_dps10[3], float dt);
/* Angle between the body axis IMU_AXIAL_AXIS and world up, degrees (0 = vertical). */
float attitude_tilt_deg(const attitude_t *a);

#endif /* ATTITUDE_H */
