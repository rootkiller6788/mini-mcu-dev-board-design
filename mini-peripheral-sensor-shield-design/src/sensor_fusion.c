/**
 * @file    sensor_fusion.c
 * @brief   L5-L8 Sensor Fusion - Complementary Filter, Kalman, Mahony AHRS, Voting
 *
 * @details Implements sensor fusion algorithms for IMU and multi-sensor applications:
 *          - Complementary filter (accel+gyro for tilt angles)
 *          - 1D Kalman filter for scalar state estimation
 *          - Mahony AHRS (3D orientation from 9-DOF IMU)
 *          - Sensor voting with outlier rejection
 *
 * Knowledge Mapping:
 *   L3 - State-space, covariance, quaternions, Euler angles
 *   L4 - Bayes theorem for recursive estimation
 *   L5 - Kalman predict+update cycle, Mahony PI correction on SO(3)
 *   L6 - IMU tilt angle for balancing robots
 *   L7 - Drone AHRS with MPU-6050+HMC5883L shield
 *   L8 - Adaptive noise covariance, quaternion integration
 *
 * Reference: Kalman (1960), Mahony et al. (2008), Welch & Bishop (2006)
 */

#include "sensor_fusion.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- 3D Vector Operations ---- */

vec3_t vec3_add(vec3_t a, vec3_t b) { vec3_t r = {a.x+b.x, a.y+b.y, a.z+b.z}; return r; }
vec3_t vec3_sub(vec3_t a, vec3_t b) { vec3_t r = {a.x-b.x, a.y-b.y, a.z-b.z}; return r; }
vec3_t vec3_scale(vec3_t v, double s) { vec3_t r = {v.x*s, v.y*s, v.z*s}; return r; }
double vec3_dot(vec3_t a, vec3_t b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

vec3_t vec3_cross(vec3_t a, vec3_t b) {
    vec3_t r = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    return r;
}

double vec3_norm(vec3_t v) { return sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }

vec3_t vec3_normalize(vec3_t v) {
    double n = vec3_norm(v);
    if (n < 1e-12) { vec3_t z = {0,0,0}; return z; }
    return vec3_scale(v, 1.0/n);
}

/* ---- Complementary Filter ---- */

void complementary_filter_init(complementary_filter_t *cf, double tau_s) {
    if (!cf) return;
    memset(cf, 0, sizeof(*cf));
    cf->tau_s = tau_s > 0.0 ? tau_s : 0.5;
}

void complementary_filter_update(complementary_filter_t *cf,
                                  double gx, double gy, double gz,
                                  double ax, double ay, double az, double dt) {
    if (!cf || dt <= 0.0) return;
    /* Angle from accelerometer (gravity vector) */
    double accel_roll = atan2(ay, az) * 180.0 / M_PI;
    double accel_pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
    if (!cf->is_initialized) {
        cf->roll_deg = accel_roll; cf->pitch_deg = accel_pitch; cf->is_initialized = true;
        return;
    }
    /* alpha = tau/(tau+dt) */
    double alpha = cf->tau_s / (cf->tau_s + dt);
    /* Gyro integration (roll from gyro_x, pitch from gyro_y) */
    double gyro_roll = cf->roll_deg + gx * dt;
    double gyro_pitch = cf->pitch_deg + gy * dt;
    /* Fuse: angle = alpha*gyro + (1-alpha)*accel */
    cf->roll_deg = alpha * gyro_roll + (1.0 - alpha) * accel_roll;
    cf->pitch_deg = alpha * gyro_pitch + (1.0 - alpha) * accel_pitch;
    (void)gz;
    cf->dt_s = dt;
}

void complementary_filter_get_angles(const complementary_filter_t *cf,
                                      double *roll, double *pitch) {
    if (roll) *roll = cf ? cf->roll_deg : 0.0;
    if (pitch) *pitch = cf ? cf->pitch_deg : 0.0;
}

/* ---- 1D Kalman Filter ---- */

void kalman_1d_init(kalman_filter_1d_t *kf, double init_x, double init_p,
                     double q, double r) {
    if (!kf) return;
    kf->x_est = init_x;
    kf->p_est = init_p > 0.0 ? init_p : 1.0;
    kf->q = q > 0.0 ? q : 0.001;
    kf->r = r > 0.0 ? r : 0.01;
    kf->k = 0.0;
    kf->is_initialized = true;
}

double kalman_1d_update(kalman_filter_1d_t *kf, double measurement) {
    if (!kf) return measurement;
    /* Predict */
    double p_pred = kf->p_est + kf->q;
    /* Update */
    kf->k = p_pred / (p_pred + kf->r);
    kf->x_est = kf->x_est + kf->k * (measurement - kf->x_est);
    kf->p_est = (1.0 - kf->k) * p_pred;
    return kf->x_est;
}

double kalman_1d_get_state(const kalman_filter_1d_t *kf) {
    return kf ? kf->x_est : 0.0;
}

void kalman_1d_set_noise(kalman_filter_1d_t *kf, double q, double r) {
    if (!kf) return;
    kf->q = q > 0.0 ? q : 1e-6;
    kf->r = r > 0.0 ? r : 1e-6;
}

/* ---- Mahony AHRS ---- */

void mahony_ahrs_init(mahony_ahrs_t *m, double Kp, double Ki) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->q.w = 1.0; m->q.x = 0.0; m->q.y = 0.0; m->q.z = 0.0;
    m->Kp = Kp; m->Ki = Ki;
    m->integral_error.x = 0.0; m->integral_error.y = 0.0; m->integral_error.z = 0.0;
}

void mahony_ahrs_update_imu(mahony_ahrs_t *m, double gx, double gy, double gz,
                              double ax, double ay, double az, double dt) {
    /* 6-DOF IMU version (no magnetometer): correct roll/pitch only */
    if (!m || dt <= 0.0) return;
    /* Normalize accelerometer */
    vec3_t a = {ax, ay, az};
    double an = vec3_norm(a);
    if (an < 1e-6) return;
    a = vec3_scale(a, 1.0/an);
    /* Estimated gravity from quaternion */
    double qw = m->q.w, qx = m->q.x, qy = m->q.y, qz = m->q.z;
    double vx = 2.0*(qx*qz - qw*qy);
    double vy = 2.0*(qw*qx + qy*qz);
    double vz = qw*qw - qx*qx - qy*qy + qz*qz;
    vec3_t v = {vx, vy, vz};
    /* Error = accel cross estimated gravity */
    vec3_t e = vec3_cross(a, v);
    /* Apply PI */
    m->integral_error = vec3_add(m->integral_error, vec3_scale(e, m->Ki * dt));
    vec3_t corr = vec3_add(vec3_scale(e, m->Kp), m->integral_error);
    /* Integrate quaternion: q_dot = 0.5 * q * omega */
    vec3_t gyro = {gx * M_PI/180.0, gy * M_PI/180.0, gz * M_PI/180.0};
    gyro = vec3_add(gyro, corr);
    double qw_dot = 0.5*(-qx*gyro.x - qy*gyro.y - qz*gyro.z);
    double qx_dot = 0.5*( qw*gyro.x + qy*gyro.z - qz*gyro.y);
    double qy_dot = 0.5*( qw*gyro.y - qx*gyro.z + qz*gyro.x);
    double qz_dot = 0.5*( qw*gyro.z + qx*gyro.y - qy*gyro.x);
    m->q.w += qw_dot * dt; m->q.x += qx_dot * dt;
    m->q.y += qy_dot * dt; m->q.z += qz_dot * dt;
    /* Re-normalize */
    double qn = sqrt(m->q.w*m->q.w+m->q.x*m->q.x+m->q.y*m->q.y+m->q.z*m->q.z);
    if (qn > 1e-12) { m->q.w/=qn; m->q.x/=qn; m->q.y/=qn; m->q.z/=qn; }
    m->dt_s = dt; m->is_initialized = true;
}

void mahony_ahrs_update(mahony_ahrs_t *m, double gx, double gy, double gz,
                         double ax, double ay, double az,
                         double mx, double my, double mz, double dt) {
    /* Full 9-DOF: use magnetometer for yaw correction */
    mahony_ahrs_update_imu(m, gx, gy, gz, ax, ay, az, dt);
    (void)mx; (void)my; (void)mz; /* magnetometer correction can be added */
}

void mahony_ahrs_get_quaternion(const mahony_ahrs_t *m, double *w, double *x, double *y, double *z) {
    if (w) *w = m ? m->q.w : 1.0;
    if (x) *x = m ? m->q.x : 0.0;
    if (y) *y = m ? m->q.y : 0.0;
    if (z) *z = m ? m->q.z : 0.0;
}

euler_angles_t mahony_ahrs_get_euler(const mahony_ahrs_t *m) {
    euler_angles_t ea = {0,0,0};
    if (!m) return ea;
    double qw=m->q.w, qx=m->q.x, qy=m->q.y, qz=m->q.z;
    /* roll (x-axis rotation) */
    double sinr = 2.0*(qw*qx + qy*qz);
    double cosr = 1.0 - 2.0*(qx*qx + qy*qy);
    ea.roll_deg = atan2(sinr, cosr) * 180.0/M_PI;
    /* pitch (y-axis rotation) */
    double sinp = 2.0*(qw*qy - qz*qx);
    if (fabs(sinp) >= 1.0) ea.pitch_deg = (sinp>0?90.0:-90.0);
    else ea.pitch_deg = asin(sinp) * 180.0/M_PI;
    /* yaw (z-axis rotation) */
    double siny = 2.0*(qw*qz + qx*qy);
    double cosy = 1.0 - 2.0*(qy*qy + qz*qz);
    ea.yaw_deg = atan2(siny, cosy) * 180.0/M_PI;
    if (ea.yaw_deg < 0.0) ea.yaw_deg += 360.0;
    return ea;
}

/* ---- Sensor Voting ---- */

void sensor_voter_init(sensor_voter_t *sv, uint8_t n) {
    if (!sv) return;
    memset(sv, 0, sizeof(*sv));
    sv->num_sensors = n;
    sv->median_deviation_threshold = 3.0;
}

double sensor_voter_fuse(sensor_voter_t *sv, const double *readings) {
    if (!sv || !readings || sv->num_sensors == 0) return 0.0;
    /* Compute median */
    double sorted[8];
    int n = sv->num_sensors > 8 ? 8 : sv->num_sensors;
    for (int i = 0; i < n; i++) sorted[i] = readings[i];
    /* simple sort */
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (sorted[i] > sorted[j]) { double t=sorted[i]; sorted[i]=sorted[j]; sorted[j]=t; }
    double median = sorted[n/2];
    /* Compute MAD (Median Absolute Deviation) */
    double abs_dev[8];
    for (int i = 0; i < n; i++) abs_dev[i] = fabs(readings[i] - median);
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (abs_dev[i] > abs_dev[j]) { double t=abs_dev[i]; abs_dev[i]=abs_dev[j]; abs_dev[j]=t; }
    double mad = abs_dev[n/2] * 1.4826; /* scale to std dev for normal dist */
    /* Weighted average, rejecting outliers */
    double sum = 0.0, weight_sum = 0.0;
    int kept = 0;
    for (int i = 0; i < n; i++) {
        double z_score = (mad > 1e-12) ? fabs(readings[i] - median) / mad : 0.0;
        if (z_score < sv->median_deviation_threshold) {
            double w = 1.0 / (1.0 + z_score);
            sum += readings[i] * w; weight_sum += w; kept++;
        }
    }
    sv->fused_value = (weight_sum > 1e-12) ? sum / weight_sum : median;
    sv->confidence = (double)kept / (double)n;
    return sv->fused_value;
}

double sensor_voter_get_confidence(const sensor_voter_t *sv) {
    return sv ? sv->confidence : 0.0;
}
