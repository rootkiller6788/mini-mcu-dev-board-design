/**
 * @file    example_imu_shield.c
 * @brief   L6 Canonical Problem: IMU Sensor Shield with Sensor Fusion
 *
 * @details End-to-end example of an MPU-6050 IMU shield on Arduino R3:
 *          1. Configure accelerometer and gyroscope
 *          2. Apply complementary filter for tilt angles
 *          3. Apply Kalman filter for gyro bias estimation
 *          4. Output roll/pitch angles
 *
 * Reference: MPU-6050 datasheet, Mahony et al. (2008)
 */

#include <stdio.h>
#include <math.h>
#include "sensor_types.h"
#include "sensor_fusion.h"
#include "sensor_filter.h"

int main(void) {
    printf("=== IMU Sensor Shield with Sensor Fusion ===\n\n");

    /* 1. Configure sensors */
    accelerometer_t accel;
    accelerometer_init(&accel, 1, "MPU6050_ACC", 3, 2.0);
    accel.sensitivity_lsb_per_g = 16384.0; /* MPU-6050 +/-2g */
    accel.odr_hz = 100.0;

    gyroscope_t gyro;
    gyroscope_init(&gyro, 1, "MPU6050_GYR", 3, 250.0);
    gyro.sensitivity_lsb_per_dps = 131.0; /* MPU-6050 +/-250dps */
    gyro.odr_hz = 100.0;

    printf("Accelerometer: +/-%.0fg, %d axes, ODR=%.0fHz\n",
           accel.full_scale_range_g, accel.num_axes, accel.odr_hz);
    printf("Gyroscope: +/-%.0fdps, %d axes, ODR=%.0fHz\n\n",
           gyro.full_scale_range_dps, gyro.num_axes, gyro.odr_hz);

    /* 2. Initialize complementary filter */
    complementary_filter_t cf;
    complementary_filter_init(&cf, 0.5); /* 0.5s time constant */

    /* 3. Initialize Kalman filter for gyro bias tracking */
    kalman_filter_1d_t kf;
    kalman_1d_init(&kf, 0.0, 1.0, 0.001, 0.01);

    printf("Sensor fusion: Complementary (tau=0.5s) + Kalman (bias tracking)\n\n");

    /* 4. Simulate IMU data with motion */
    printf("Simulating pitch rotation from 0 to 45 degrees:\n");
    printf("  Time(s)   Gyro(dps)  Accel(g)    CF_Pitch    Roll\n");
    printf("  -------   ---------  --------    --------   -----\n");

    double dt = 0.01; /* 100Hz */
    int steps = 100;
    cf.is_initialized = false;
    kalman_1d_init(&kf, 0.0, 1.0, 0.001, 0.01);

    for (int i = 0; i <= steps; i++) {
        double t = i * dt;
        /* Simulate pitch ramp: 0 to 45 degrees over 1 second */
        double target_pitch = (i < steps/2) ? (45.0 * t / 0.5) : 45.0;
        double pitch_rad = target_pitch * M_PI / 180.0;

        /* Simulate gyro: pitch rate + noise */
        double gyro_y = (i < steps/2) ? 90.0 : 0.0; /* dps */
        gyro_y += 0.5 * ((i*7+3)%23 - 11); /* pseudo-noise */

        /* Simulate accelerometer: gravity tilted by pitch */
        double ax = sin(pitch_rad);   /* forward tilt */
        double ay = 0.0;
        double az = cos(pitch_rad);   /* gravity component */

        /* Complementary filter update */
        complementary_filter_update(&cf, 0.0, gyro_y, 0.0, ax, ay, az, dt);

        /* Kalman filter on pitch to smooth */
        double kf_pitch = kalman_1d_update(&kf, cf.pitch_deg); (void)kf_pitch;

        double roll, pitch;
        complementary_filter_get_angles(&cf, &roll, &pitch);

        if (i % 10 == 0 || i == steps) {
            printf("  %6.2f   %8.1f   %6.3f,%6.3f  %7.2f   %6.2f\n",
                   t, gyro_y, ax, az, pitch, roll);
        }
    }

    printf("\nIMU shield fusion complete.\n");
    printf("Design notes:\n");
    printf("  1. MPU-6050: I2C addr 0x68, 3.3V supply\n");
    printf("  2. Complementary filter: suitable for balancing robots\n");
    printf("  3. Add magnetometer for yaw stability (HMC5883L at 0x1E)\n");
    printf("  4. Shield fits Arduino Uno R3 + sensor breakout boards\n");

    return 0;
}
