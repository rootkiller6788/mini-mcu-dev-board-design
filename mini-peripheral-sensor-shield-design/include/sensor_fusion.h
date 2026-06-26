/**
 * @file    sensor_fusion.h
 * @brief   L5-L8 Sensor Fusion Algorithms - Complementary Filter, Kalman Filter, AHRS
 *
 * @details Algorithms for combining multiple sensor inputs to improve accuracy,
 *          reliability, and robustness:
 *          - Complementary filter (accelerometer + gyroscope for tilt angles)
 *          - 1D Kalman filter (simple state estimation)
 *          - Mahony AHRS algorithm (3D orientation from IMU)
 *          - Sensor voting & weighted averaging for redundancy
 *
 * Knowledge Mapping:
 *   L3 - State-space models, covariance matrices, Gaussian distributions
 *   L4 - Bayes' theorem for recursive estimation
 *   L5 - Kalman filter algorithm (predict + update), Mahony AHRS
 *   L6 - IMU-based tilt angle estimation for balancing robots
 *   L7 - Drone attitude estimation with MPU-6050 sensor shield
 *   L8 - Extended Kalman Filter, adaptive noise covariance tuning
 *
 * Reference: Kalman "A New Approach to Linear Filtering" (1960)
 *            Mahony et al. "Nonlinear Complementary Filters on SO(3)" (2008)
 *            Welch & Bishop "An Introduction to the Kalman Filter" (2006)
 */

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- 3D Vector (L3) ---- */
typedef struct { double x, y, z; } vec3_t;
typedef struct { double w, x, y, z; } quat_t;

vec3_t vec3_add(vec3_t a, vec3_t b);
vec3_t vec3_sub(vec3_t a, vec3_t b);
vec3_t vec3_scale(vec3_t v, double s);
double vec3_dot(vec3_t a, vec3_t b);
vec3_t vec3_cross(vec3_t a, vec3_t b);
double vec3_norm(vec3_t v);
vec3_t vec3_normalize(vec3_t v);

/* ---- Euler Angles (L3) ----
 * Roll (phi): rotation about X axis, pitch (theta): Y, yaw (psi): Z
 * Tait-Bryan convention: Z-Y-X (yaw-pitch-roll) */
typedef struct {
    double roll_deg;                 /* -180 to 180 */
    double pitch_deg;                /* -90 to 90 */
    double yaw_deg;                  /* -180 to 180 (or 0-360) */
} euler_angles_t;

/* ---- Complementary Filter (L5-L6) ----
 * Combines high-frequency gyro data with low-frequency accel data
 * angle = alpha * (angle + gyro_rate * dt) + (1-alpha) * accel_angle
 *
 * where: gyro_angle = integrates high-freq but drifts
 *        accel_angle = noisy but zero long-term error
 *        alpha = tau/(tau+dt), tau = time constant (~0.5-1s)
 *
 * Roll from accel only: roll = atan2(ay, az)
 * Pitch from accel only: pitch = atan2(-ax, sqrt(ay^2 + az^2))
 *
 * This is effectively a 1st-order IIR on the gyro integration error */
typedef struct {
    double alpha;                    /* filter coefficient (close to 1) */
    double tau_s;                    /* time constant */
    double roll_deg;                 /* current roll estimate */
    double pitch_deg;                /* current pitch estimate */
    double dt_s;                     /* last sample period */
    bool is_initialized;
} complementary_filter_t;

void complementary_filter_init(complementary_filter_t *cf, double tau_s);
void complementary_filter_update(complementary_filter_t *cf,
                                  double gx_dps, double gy_dps, double gz_dps,
                                  double ax_g, double ay_g, double az_g,
                                  double dt_s);
void complementary_filter_get_angles(const complementary_filter_t *cf,
                                      double *roll, double *pitch);

/* ---- 1D Kalman Filter (L5-L8) ----
 * State: x = [position; velocity]^T  (or any scalar state)
 *
 * Predict:
 *   x = F * x + B * u
 *   P = F * P * F^T + Q
 *
 * Update:
 *   K = P * H^T * (H * P * H^T + R)^(-1)
 *   x = x + K * (z - H * x)
 *   P = (I - K * H) * P
 *
 * For scalar state with constant model:
 *   Predict:  x_hat = x_prev,  p_hat = p_prev + q
 *   Update:   k = p_hat / (p_hat + r),  x = x_hat + k*(z - x_hat),  p = (1-k)*p_hat */
typedef struct {
    double x_est;                    /* estimated state */
    double p_est;                    /* estimated error covariance */
    double q;                        /* process noise covariance */
    double r;                        /* measurement noise covariance */
    double k;                        /* Kalman gain (computed each step) */
    bool is_initialized;
} kalman_filter_1d_t;

void kalman_1d_init(kalman_filter_1d_t *kf, double init_x, double init_p,
                     double process_noise, double meas_noise);
double kalman_1d_update(kalman_filter_1d_t *kf, double measurement);
double kalman_1d_get_state(const kalman_filter_1d_t *kf);
void kalman_1d_set_noise(kalman_filter_1d_t *kf, double q, double r);

/* ---- Mahony AHRS Algorithm (L5-L8) ----
 * Nonlinear complementary filter on SO(3) for 3D orientation
 * Uses quaternion representation to avoid gimbal lock
 *
 * Algorithm (simplified):
 *   1. Get normalized accel vector a_hat, mag vector m_hat
 *   2. Compute expected gravity from current quaternion: v = R(q)^T * [0,0,1]^T
 *   3. Error: e = a_hat x v (cross product)
 *   4. If magnetometer present: correct heading with magnetic reference
 *   5. Apply PI controller: correction = Kp*e + Ki*integral(e)
 *   6. Integrate quaternion: q_dot = 0.5 * q * (gyro + correction)
 *
 * Reference: Mahony, Hamel, Pflimlin "Nonlinear Complementary Filters on SO(3)" (2008)
 */
typedef struct {
    quat_t q;                        /* current orientation quaternion */
    double Kp;                       /* proportional gain (typically 0.5-2.0) */
    double Ki;                       /* integral gain (typically 0.01-0.1) */
    vec3_t integral_error;           /* accumulated error for I term */
    double dt_s;                     /* sample period */
    bool is_initialized;
} mahony_ahrs_t;

void mahony_ahrs_init(mahony_ahrs_t *mahony, double Kp, double Ki);
void mahony_ahrs_update(mahony_ahrs_t *mahony,
                         double gx_dps, double gy_dps, double gz_dps,
                         double ax, double ay, double az,
                         double mx, double my, double mz, double dt_s);
void mahony_ahrs_update_imu(mahony_ahrs_t *mahony,
                              double gx, double gy, double gz,
                              double ax, double ay, double az, double dt_s);
void mahony_ahrs_get_quaternion(const mahony_ahrs_t *mahony,
                                 double *w, double *x, double *y, double *z);
euler_angles_t mahony_ahrs_get_euler(const mahony_ahrs_t *mahony);

/* ---- Sensor Voting / Redundancy (L5-L7) ----
 * For N sensors measuring the same quantity:
 *   1. Remove outliers (outside median +/- threshold*MAD)
 *   2. Weight remaining values by inverse variance
 *   3. Output weighted average with confidence score */
typedef struct {
    double *values;
    double *weights;
    uint8_t num_sensors;
    double median_deviation_threshold;  /* MAD threshold for outlier rejection */
    double fused_value;
    double confidence;                 /* 0-1, higher = more agreement */
} sensor_voter_t;

void sensor_voter_init(sensor_voter_t *sv, uint8_t num_sensors);
double sensor_voter_fuse(sensor_voter_t *sv, const double *sensor_readings);
double sensor_voter_get_confidence(const sensor_voter_t *sv);

#endif /* SENSOR_FUSION_H */
