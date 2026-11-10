/*
 * attitude.c - see attitude.h. No C library: square roots use the FPU
 * instruction through __builtin_sqrtf, and the one arccos in
 * attitude_tilt_deg() is a short series (it is only for logging/tests).
 */
#include "attitude.h"
#include "config.h"

#define DEG2RAD 0.017453292519943295f

static float inv_norm(float s)
{
    return s > 1e-12f ? 1.0f / __builtin_sqrtf(s) : 0.0f;
}

void attitude_init(attitude_t *a)
{
    a->q[0] = 1.0f;
    a->q[1] = a->q[2] = a->q[3] = 0.0f;
    a->init = false;
}

/* Tilt-only quaternion that turns the measured "up" (accel direction, body
 * frame) into world +Z: rotation about (u x z) by the angle between them. */
static void init_from_accel(attitude_t *a, float ux, float uy, float uz)
{
    /* q = normalise(1 + uz, uy, -ux, 0) is the shortest rotation u -> +Z */
    float w = 1.0f + uz, x = uy, y = -ux;
    float n = inv_norm(w * w + x * x + y * y);
    if (n == 0.0f) {                  /* upside down: 180 deg about X */
        a->q[0] = 0.0f; a->q[1] = 1.0f; a->q[2] = 0.0f; a->q[3] = 0.0f;
    } else {
        a->q[0] = w * n; a->q[1] = x * n; a->q[2] = y * n; a->q[3] = 0.0f;
    }
    a->init = true;
}

void attitude_update(attitude_t *a, const int16_t accel_mg[3], const int16_t gyro_dps10[3], float dt)
{
    float ax = (float)accel_mg[0], ay = (float)accel_mg[1], az = (float)accel_mg[2];
    float amag2 = ax * ax + ay * ay + az * az;
    float lo = 1000.0f - (float)ATT_ACC_GATE_MG, hi = 1000.0f + (float)ATT_ACC_GATE_MG;
    bool gravity_only = amag2 >= lo * lo && amag2 <= hi * hi;

    if (!a->init) {
        if (gravity_only) {
            float n = inv_norm(amag2);
            init_from_accel(a, ax * n, ay * n, az * n);
        }
        return;
    }

    float gx = (float)gyro_dps10[0] * 0.1f * DEG2RAD;
    float gy = (float)gyro_dps10[1] * 0.1f * DEG2RAD;
    float gz = (float)gyro_dps10[2] * 0.1f * DEG2RAD;
    float qw = a->q[0], qx = a->q[1], qy = a->q[2], qz = a->q[3];

    if (gravity_only && dt > 0.0f) {
        float n = inv_norm(amag2);
        ax *= n; ay *= n; az *= n;
        /* world up (0,0,1) expressed in the body frame, from q */
        float vx = 2.0f * (qx * qz - qw * qy);
        float vy = 2.0f * (qw * qx + qy * qz);
        float vz = qw * qw - qx * qx - qy * qy + qz * qz;
        /* error = measured up x estimated up */
        gx += ATT_KP * (ay * vz - az * vy);
        gy += ATT_KP * (az * vx - ax * vz);
        gz += ATT_KP * (ax * vy - ay * vx);
    }

    /* q' = 0.5 q (0, g) */
    float hd = 0.5f * dt;
    a->q[0] = qw + hd * (-qx * gx - qy * gy - qz * gz);
    a->q[1] = qx + hd * (qw * gx + qy * gz - qz * gy);
    a->q[2] = qy + hd * (qw * gy - qx * gz + qz * gx);
    a->q[3] = qz + hd * (qw * gz + qx * gy - qy * gx);
    float n = inv_norm(a->q[0] * a->q[0] + a->q[1] * a->q[1] + a->q[2] * a->q[2] + a->q[3] * a->q[3]);
    for (int i = 0; i < 4; i++) {
        a->q[i] *= n;
    }
}

float attitude_tilt_deg(const attitude_t *a)
{
    float qw = a->q[0], qx = a->q[1], qy = a->q[2], qz = a->q[3];
    float c;                          /* world-Z component of the rotated axial unit vector */
#if IMU_AXIAL_AXIS == 0
    c = 2.0f * (qx * qz - qw * qy);
#elif IMU_AXIAL_AXIS == 1
    c = 2.0f * (qy * qz + qw * qx);
#else
    c = qw * qw - qx * qx - qy * qy + qz * qz;
#endif
    c *= (float)IMU_AXIAL_SIGN;
    if (c > 1.0f) c = 1.0f;
    if (c < -1.0f) c = -1.0f;
    /* acos(c) = 2 atan2(sqrt(1-c), sqrt(1+c)); atan via a short series on [0, 1] */
    float s = __builtin_sqrtf(1.0f - c), t = __builtin_sqrtf(1.0f + c);
    bool flip = s > t;
    float r = flip ? t / s : s / t;
    float r2 = r * r;
    float at = r * (0.9998660f + r2 * (-0.3302995f + r2 * (0.1801410f + r2 * (-0.0851330f + r2 * 0.0208351f))));
    if (flip) {
        at = 1.5707963f - at;
    }
    return 2.0f * at / DEG2RAD;
}
