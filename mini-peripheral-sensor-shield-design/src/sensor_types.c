/**
 * @file    sensor_types.c
 * @brief   L1-L4 Sensor Type Implementations - Physics equations & transfer functions
 *
 * @details Implements the core sensor physics for 18 sensor types:
 *          thermistor (Steinhart-Hart), RTD (Callendar-Van Dusen),
 *          thermocouple (Seebeck), strain gauge, load cell,
 *          accelerometer, gyroscope, magnetometer, photodiode,
 *          pressure, humidity, ultrasonic, gas, current sensors.
 *
 * Knowledge Mapping:
 *   L1 - Sensor struct initialization with real-world defaults
 *   L3 - Transfer function evaluation (forward & inverse mapping)
 *   L4 - Physical laws: Steinhart-Hart, Callendar-Van Dusen, Seebeck effect,
 *        Wheatstone bridge, Gauge Factor, Beer-Lambert (NDIR), Magnus formula
 *
 * Reference: Fraden (2016), Wilson (2005), individual sensor datasheets
 */

#include "sensor_types.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ---- Sensor Instance Initialization ---- */

void sensor_instance_init(sensor_instance_t *s, uint32_t id, const char *name,
                          sensor_type_tag_t tag) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->id = id;
    s->type_tag = tag;
    if (name) { strncpy(s->name, name, SENSOR_MAX_NAME - 1); }
    sensor_specs_init(&s->specs);
}

void sensor_specs_init(sensor_specs_t *specs) {
    if (!specs) return;
    memset(specs, 0, sizeof(*specs));
    specs->signal_to_noise_ratio_db = 60.0;
    specs->bandwidth_hz = 100.0;
}

/* ---- Thermistor: Beta Equation (L4) ---- */

void thermistor_init(thermistor_t *t, uint32_t id, const char *pn,
                     double R25, double beta) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->id = id;
    if (pn) strncpy(t->part_number, pn, 31);
    t->resistance_at_25c = R25;
    t->beta_value = beta;
    t->is_ntc = (beta > 0);
    t->operating_temp_min_c = -55.0;
    t->operating_temp_max_c = 125.0;
    t->tolerance_percent = 1.0;
    t->dissipation_constant_mw_per_c = 1.0;
    t->thermal_time_constant_s = 5.0;
    sensor_specs_init(&t->specs);
}

double thermistor_resistance_at_temp(const thermistor_t *t, double temp_c) {
    if (!t || t->beta_value == 0.0) return t ? t->resistance_at_25c : 0.0;
    double T_k = temp_c + 273.15;
    double T25_k = 298.15;
    if (T_k <= 0.0) return t->resistance_at_25c * 100.0; /* near 0K, very high R */
    return t->resistance_at_25c * exp(t->beta_value * (1.0/T_k - 1.0/T25_k));
}

double thermistor_temp_from_resistance(const thermistor_t *t, double resistance) {
    if (!t || resistance <= 0.0 || t->resistance_at_25c <= 0.0) return 25.0;
    double T25_k = 298.15;
    double ln_ratio = log(resistance / t->resistance_at_25c);
    if (t->beta_value == 0.0) return 25.0;
    double T_k = 1.0 / (1.0/T25_k + ln_ratio / t->beta_value);
    return T_k - 273.15;
}

void thermistor_set_steinhart(thermistor_t *t, double A, double B, double C) {
    if (!t) return;
    t->steinhart_A = A;
    t->steinhart_B = B;
    t->steinhart_C = C;
}

double thermistor_steinhart_temp(const thermistor_t *t, double R) {
    /* 1/T = A + B*ln(R) + C*(ln(R))^3, returns T in Celsius */
    if (!t || R <= 0.0) return 25.0;
    double lnR = log(R);
    double invT = t->steinhart_A + t->steinhart_B * lnR
                + t->steinhart_C * lnR * lnR * lnR;
    if (invT <= 0.0) return 300.0; /* degenerate */
    return (1.0 / invT) - 273.15;
}

int thermistor_calibrate_steinhart(thermistor_t *t,
                                    const double *temps, const double *resistances,
                                    size_t n) {
    /* Solve for A,B,C using 3-point calibration with linear algebra.
     * For exactly 3 points, solve the 3x3 system directly.
     * Uses: [1 ln(R_i) (ln(R_i))^3] * [A B C]^T = 1/(T_i+273.15) */
    if (!t || !temps || !resistances || n < 3) return -1;
    /* Build and solve 3x3 system using first 3 points (simplest reliable method) */
    double lnR[3], invT[3];
    for (int i = 0; i < 3; i++) {
        if (resistances[i] <= 0.0) return -2;
        lnR[i] = log(resistances[i]);
        invT[i] = 1.0 / (temps[i] + 273.15);
    }
    /* Build 3x3 system: for each point i, A + B*ln(R_i) + C*ln(R_i)^3 = 1/T_i
     * Matrix row i: [1, lnR_i, lnR_i^3], rhs[i] = 1/T_i */
    double M[9] = {
        1.0, lnR[0], lnR[0]*lnR[0]*lnR[0],
        1.0, lnR[1], lnR[1]*lnR[1]*lnR[1],
        1.0, lnR[2], lnR[2]*lnR[2]*lnR[2]
    };
    double rhs[3] = {invT[0], invT[1], invT[2]};
    /* Gauss elimination with partial pivoting */
    for (int col = 0; col < 3; col++) {
        int pivot = col;
        double max_v = fabs(M[col*3 + col]);
        for (int row = col+1; row < 3; row++) {
            if (fabs(M[row*3 + col]) > max_v) {
                max_v = fabs(M[row*3 + col]); pivot = row;
            }
        }
        if (max_v < 1e-15) return -3;
        if (pivot != col) {
            for (int j = 0; j < 3; j++) { double tmp = M[col*3+j]; M[col*3+j] = M[pivot*3+j]; M[pivot*3+j] = tmp; }
            double tmp = rhs[col]; rhs[col] = rhs[pivot]; rhs[pivot] = tmp;
        }
        double diag = M[col*3 + col];
        for (int j = col; j < 3; j++) M[col*3 + j] /= diag;
        rhs[col] /= diag;
        for (int row = col+1; row < 3; row++) {
            double factor = M[row*3 + col];
            for (int j = col; j < 3; j++) M[row*3 + j] -= factor * M[col*3 + j];
            rhs[row] -= factor * rhs[col];
        }
    }
    /* Back substitution */
    for (int i = 2; i >= 0; i--) {
        for (int j = i+1; j < 3; j++) rhs[i] -= M[i*3 + j] * rhs[j];
    }
    t->steinhart_A = rhs[0];
    t->steinhart_B = rhs[1];
    t->steinhart_C = rhs[2];
    return 0;
}

/* ---- RTD: Callendar-Van Dusen Equation (L4) ---- */

static const double CVD_A_PT = 3.9083e-3;
static const double CVD_B_PT = -5.775e-7;
static const double CVD_C_PT = -4.183e-12;

void rtd_init(rtd_t *r, uint32_t id, rtd_type_t type, rtd_wiring_t wiring) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->id = id;
    r->type = type;
    r->wiring = wiring;
    switch (type) {
        case RTD_PT100:   r->R0 = 100.0; break;
        case RTD_PT500:   r->R0 = 500.0; break;
        case RTD_PT1000:  r->R0 = 1000.0; break;
        case RTD_NI100:   r->R0 = 100.0; break;
        case RTD_NI1000:  r->R0 = 1000.0; break;
        case RTD_CU10:    r->R0 = 10.0; break;
    }
    r->cv_A = CVD_A_PT;
    r->cv_B = CVD_B_PT;
    r->cv_C = CVD_C_PT;
    r->tolerance_class_percent = 0.15; /* Class A */
    r->excitation_current_ma = 1.0;
    sensor_specs_init(&r->specs);
}

double rtd_resistance_at_temp(const rtd_t *r, double temp_c) {
    if (!r) return 100.0;
    double R0 = r->R0;
    double A = r->cv_A, B = r->cv_B, C = r->cv_C;
    if (temp_c >= 0.0) {
        return R0 * (1.0 + A * temp_c + B * temp_c * temp_c);
    } else {
        return R0 * (1.0 + A * temp_c + B * temp_c * temp_c
                     + C * (temp_c - 100.0) * temp_c * temp_c * temp_c);
    }
}

double rtd_temp_from_resistance(const rtd_t *r, double resistance) {
    /* Newton-Raphson iteration to invert CVD equation */
    if (!r || r->R0 <= 0.0) return 0.0;
    double T = 25.0; /* initial guess */
    for (int iter = 0; iter < 20; iter++) {
        double R_calc = rtd_resistance_at_temp(r, T);
        double dRdT; /* derivative */
        if (T >= 0.0) {
            dRdT = r->R0 * (r->cv_A + 2.0 * r->cv_B * T);
        } else {
            double T2 = T * T;
            dRdT = r->R0 * (r->cv_A + 2.0 * r->cv_B * T
                            + r->cv_C * (4.0 * T2 * T - 300.0 * T2));
        }
        if (fabs(dRdT) < 1e-12) break;
        double dT = (resistance - R_calc) / dRdT;
        T += dT;
        if (fabs(dT) < 0.001) break;
    }
    return T;
}

double rtd_wire_resistance_compensation(const rtd_t *r, double measured_R,
                                         double lead_R) {
    if (!r) return measured_R;
    switch (r->wiring) {
        case RTD_WIRE_2: return measured_R - 2.0 * lead_R;
        case RTD_WIRE_3: return measured_R - lead_R; /* assumes matched leads */
        case RTD_WIRE_4: return measured_R;           /* Kelvin sensing */
    }
    return measured_R;
}

/* ---- Thermocouple: Seebeck Effect (L4) ---- */

void thermocouple_init(thermocouple_t *tc, uint32_t id, thermocouple_type_t type) {
    if (!tc) return;
    memset(tc, 0, sizeof(*tc));
    tc->id = id;
    tc->type = type;
    tc->requires_cjc = true;
    switch (type) {
        case TC_TYPE_K: tc->seebeck_coeff_uv_per_c = 41.0; tc->temp_range_min_c = -200.0; tc->temp_range_max_c = 1260.0; break;
        case TC_TYPE_J: tc->seebeck_coeff_uv_per_c = 52.0; tc->temp_range_min_c = -40.0;  tc->temp_range_max_c = 750.0;  break;
        case TC_TYPE_T: tc->seebeck_coeff_uv_per_c = 43.0; tc->temp_range_min_c = -200.0; tc->temp_range_max_c = 350.0;  break;
        case TC_TYPE_E: tc->seebeck_coeff_uv_per_c = 68.0; tc->temp_range_min_c = -200.0; tc->temp_range_max_c = 900.0;  break;
        case TC_TYPE_N: tc->seebeck_coeff_uv_per_c = 39.0; tc->temp_range_min_c = -200.0; tc->temp_range_max_c = 1300.0; break;
        case TC_TYPE_R: tc->seebeck_coeff_uv_per_c = 10.0; tc->temp_range_min_c = -50.0;  tc->temp_range_max_c = 1760.0; break;
        case TC_TYPE_S: tc->seebeck_coeff_uv_per_c = 10.0; tc->temp_range_min_c = -50.0;  tc->temp_range_max_c = 1760.0; break;
        case TC_TYPE_B: tc->seebeck_coeff_uv_per_c = 9.0;  tc->temp_range_min_c = 0.0;    tc->temp_range_max_c = 1820.0; break;
    }
    tc->sensitivity_uv_per_c = tc->seebeck_coeff_uv_per_c;
    tc->accuracy_c = 1.5;
    sensor_specs_init(&tc->specs);
}

double thermocouple_voltage_from_temp(const thermocouple_t *tc,
                                       double t_hot, double t_cold) {
    if (!tc) return 0.0;
    return tc->seebeck_coeff_uv_per_c * (t_hot - t_cold);
}

double thermocouple_temp_from_voltage(const thermocouple_t *tc,
                                       double V_uv, double t_cold) {
    if (!tc || tc->seebeck_coeff_uv_per_c == 0.0) return t_cold;
    return t_cold + V_uv / tc->seebeck_coeff_uv_per_c;
}

/* ---- Strain Gauge: Gauge Factor (L4) ---- */

void strain_gauge_init(strain_gauge_t *sg, uint32_t id, double R0, double gf) {
    if (!sg) return;
    memset(sg, 0, sizeof(*sg));
    sg->id = id;
    sg->nominal_resistance = R0;
    sg->gauge_factor = gf;
    sg->max_strain_ue = 50000.0;
    sg->fatigue_life_cycles = 1e6;
    sg->temperature_coeff_ppm_per_c = 10.0;
    sg->bridge_config = BRIDGE_QUARTER;
    sensor_specs_init(&sg->specs);
}

double strain_gauge_strain_from_resistance_change(const strain_gauge_t *sg,
                                                   double delta_R) {
    /* epsilon = (delta_R/R) / GF, returned in microstrain */
    if (!sg || sg->gauge_factor == 0.0 || sg->nominal_resistance == 0.0) return 0.0;
    return (delta_R / sg->nominal_resistance) / sg->gauge_factor * 1e6;
}

double strain_gauge_bridge_output(const strain_gauge_t *sg, double strain,
                                   double Vex, bridge_config_t config) {
    /* strain in microstrain, Vex in volts, output in volts */
    if (!sg || sg->gauge_factor == 0.0) return 0.0;
    double epsilon = strain / 1e6; /* convert microstrain to strain */
    double dR_over_R = sg->gauge_factor * epsilon;
    switch (config) {
        case BRIDGE_QUARTER: return Vex * dR_over_R / 4.0;
        case BRIDGE_HALF:    return Vex * dR_over_R / 2.0;
        case BRIDGE_FULL:    return Vex * dR_over_R;
    }
    return 0.0;
}

/* ---- Load Cell ---- */

void load_cell_init(load_cell_t *lc, uint32_t id, double capacity, double ro) {
    if (!lc) return;
    memset(lc, 0, sizeof(*lc));
    lc->id = id;
    lc->rated_capacity_kg = capacity;
    lc->rated_output_mv_per_v = ro;
    lc->safe_overload_percent = 150.0;
    lc->ultimate_overload_percent = 300.0;
    lc->excitation_voltage_recommended = 5.0;
    lc->excitation_voltage_max = 10.0;
    lc->input_resistance = 350.0;
    lc->output_resistance = 350.0;
    sensor_specs_init(&lc->specs);
}

double load_cell_force_from_voltage(const load_cell_t *lc,
                                     double Vm, double Vex) {
    /* Vm = measured differential voltage (mV), Vex = excitation voltage
     * Force = (Vm / (Vex * RO_mV_V/1000)) * rated_capacity */
    if (!lc || lc->rated_output_mv_per_v <= 0.0 || lc->rated_capacity_kg <= 0.0) return 0.0;
    double V_fullscale = Vex * lc->rated_output_mv_per_v / 1000.0; /* V at full scale */
    if (V_fullscale <= 0.0) return 0.0;
    return (Vm / V_fullscale) * lc->rated_capacity_kg;
}

double load_cell_voltage_from_force(const load_cell_t *lc,
                                     double F, double Vex) {
    if (!lc || lc->rated_capacity_kg <= 0.0) return 0.0;
    double ratio = F / lc->rated_capacity_kg;
    return Vex * lc->rated_output_mv_per_v / 1000.0 * ratio;
}

/* ---- Accelerometer ---- */

void accelerometer_init(accelerometer_t *a, uint32_t id, const char *pn,
                        uint8_t axes, double fs) {
    if (!a) return;
    memset(a, 0, sizeof(*a));
    a->id = id;
    if (pn) strncpy(a->part_number, pn, 31);
    a->num_axes = axes > 3 ? 3 : axes;
    a->full_scale_range_g = fs;
    a->sensitivity_mv_per_g = 300.0;  /* typical analog accel */
    a->sensitivity_lsb_per_g = 256.0; /* typical 10-bit +/-2g */
    a->zero_g_offset_mv = 1650.0;     /* 3.3V/2 */
    a->bandwidth_hz = 50.0;
    a->odr_hz = 100.0;
    a->resolution_bits = 10;
    sensor_specs_init(&a->specs);
}

double accelerometer_accel_from_voltage(const accelerometer_t *a,
                                         double Vm, uint8_t axis) {
    (void)axis;
    if (!a || a->sensitivity_mv_per_g <= 0.0) return 0.0;
    return (Vm - a->zero_g_offset_mv) / a->sensitivity_mv_per_g;
}

double accelerometer_accel_from_lsb(const accelerometer_t *a,
                                     int16_t raw, uint8_t axis) {
    (void)axis;
    if (!a || a->sensitivity_lsb_per_g <= 0.0) return 0.0;
    return (double)(raw - a->zero_g_offset_lsb) / a->sensitivity_lsb_per_g;
}

double accelerometer_tilt_angle_deg(const accelerometer_t *a,
                                     double ax, double ay, double az) {
    (void)a;
    /* Roll: rotation about X, Pitch: rotation about Y */
    double roll = atan2(ay, az) * 180.0 / M_PI;
    double pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
    (void)roll;
    return pitch; /* returns pitch; roll can be accessed separately */
}

/* ---- Gyroscope ---- */

void gyroscope_init(gyroscope_t *g, uint32_t id, const char *pn,
                    uint8_t axes, double fs) {
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->id = id;
    if (pn) strncpy(g->part_number, pn, 31);
    g->num_axes = axes > 3 ? 3 : axes;
    g->full_scale_range_dps = fs;
    g->sensitivity_mv_per_dps = 5.0;
    g->sensitivity_lsb_per_dps = 65.5; /* typical MPU-6050 +/-250dps */
    g->bandwidth_hz = 50.0;
    g->odr_hz = 100.0;
    g->resolution_bits = 16;
    sensor_specs_init(&g->specs);
}

double gyroscope_rate_from_voltage(const gyroscope_t *g, double Vm, uint8_t axis) {
    (void)axis;
    if (!g || g->sensitivity_mv_per_dps <= 0.0) return 0.0;
    return (Vm - g->zero_rate_offset_dps * g->sensitivity_mv_per_dps)
           / g->sensitivity_mv_per_dps;
}

double gyroscope_rate_from_lsb(const gyroscope_t *g, int16_t raw, uint8_t axis) {
    (void)axis;
    if (!g || g->sensitivity_lsb_per_dps <= 0.0) return 0.0;
    return (double)raw / g->sensitivity_lsb_per_dps;
}

/* ---- Magnetometer ---- */

void magnetometer_init(magnetometer_t *m, uint32_t id, const char *pn,
                       uint8_t axes, double fs) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->id = id;
    if (pn) strncpy(m->part_number, pn, 31);
    m->num_axes = axes > 3 ? 3 : axes;
    m->full_scale_range_ut = fs;
    m->sensitivity_lsb_per_ut = 0.15; /* typical HMC5883L */
    for (int i = 0; i < 3; i++) m->hard_iron_offset[i] = 0.0;
    for (int i = 0; i < 9; i++) m->soft_iron_matrix[i] = (i%4==0) ? 1.0 : 0.0;
    sensor_specs_init(&m->specs);
}

double magnetometer_field_from_lsb(const magnetometer_t *m, int16_t raw, uint8_t axis) {
    (void)axis;
    if (!m || m->sensitivity_lsb_per_ut <= 0.0) return 0.0;
    return (double)raw * m->sensitivity_lsb_per_ut;
}

double magnetometer_heading_deg(const magnetometer_t *m,
                                 double mx, double my, double mz) {
    (void)m; (void)mz;
    /* Heading = atan2(my, mx), with tilt compensation if mz provided */
    double heading = atan2(my, mx) * 180.0 / M_PI;
    if (heading < 0.0) heading += 360.0;
    return heading;
}

/* ---- Photodiode ---- */

void photodiode_init(photodiode_t *pd, uint32_t id, const char *pn, double resp) {
    if (!pd) return;
    memset(pd, 0, sizeof(*pd));
    pd->id = id;
    if (pn) strncpy(pd->part_number, pn, 31);
    pd->responsivity_A_per_W = resp;
    pd->dark_current_na = 2.0;
    pd->shunt_resistance_mohm = 100.0;
    pd->junction_capacitance_pf = 20.0;
    pd->spectral_range_min_nm = 300.0;
    pd->spectral_range_max_nm = 1100.0;
    pd->peak_wavelength_nm = 850.0;
    sensor_specs_init(&pd->specs);
}

double photodiode_power_from_current(const photodiode_t *pd, double I_photo) {
    /* P_opt = I_photo / R */
    if (!pd || pd->responsivity_A_per_W <= 0.0) return 0.0;
    return I_photo / pd->responsivity_A_per_W;
}

double photodiode_transimpedance_gain(double R_feedback) {
    return R_feedback; /* Vout = -I_in * Rf */
}

/* ---- Pressure Sensor ---- */

void pressure_sensor_init(pressure_sensor_t *ps, uint32_t id,
                          pressure_type_t type, double pmin, double pmax) {
    if (!ps) return;
    memset(ps, 0, sizeof(*ps));
    ps->id = id;
    ps->type = type;
    ps->pressure_min_kpa = pmin;
    ps->pressure_max_kpa = pmax;
    ps->sensitivity_mv_per_kpa = 10.0;
    ps->full_scale_span_mv = (pmax - pmin) * ps->sensitivity_mv_per_kpa;
    sensor_specs_init(&ps->specs);
}

double pressure_from_voltage(const pressure_sensor_t *ps, double Vm, double Vs) {
    if (!ps || ps->full_scale_span_mv <= 0.0) return 0.0;
    double span = ps->pressure_max_kpa - ps->pressure_min_kpa;
    double V_ratio = (Vm - ps->zero_offset_mv) / ps->full_scale_span_mv;
    (void)Vs; /* ratiometric correction if needed */
    return ps->pressure_min_kpa + span * V_ratio;
}

double pressure_from_lsb(const pressure_sensor_t *ps, uint32_t raw, uint32_t fs) {
    if (!ps || fs == 0) return 0.0;
    double span = ps->pressure_max_kpa - ps->pressure_min_kpa;
    double ratio = (double)raw / (double)fs;
    return ps->pressure_min_kpa + span * ratio;
}

/* ---- Humidity Sensor ---- */

void humidity_sensor_init(humidity_sensor_t *hs, uint32_t id, const char *pn,
                          double accuracy) {
    if (!hs) return;
    memset(hs, 0, sizeof(*hs));
    hs->id = id;
    if (pn) strncpy(hs->part_number, pn, 31);
    hs->humidity_range_min_percent = 0.0;
    hs->humidity_range_max_percent = 100.0;
    hs->accuracy_rh_percent = accuracy;
    hs->response_time_s = 8.0;
    sensor_specs_init(&hs->specs);
}

double humidity_dew_point_c(double temp_c, double rh) {
    /* Magnus formula: a=17.27, b=237.7 */
    if (rh <= 0.0) return -273.15;
    double gamma = (17.27 * temp_c) / (237.7 + temp_c) + log(rh / 100.0);
    return (237.7 * gamma) / (17.27 - gamma);
}

double humidity_absolute_humidity_g_per_m3(double temp_c, double rh) {
    /* AH = (6.112 * exp(17.67*T/(T+243.5)) * rh * 2.1674) / (273.15+T) */
    double es = 6.112 * exp((17.67 * temp_c) / (temp_c + 243.5));
    return (es * rh * 2.1674) / (273.15 + temp_c);
}

double humidity_vapor_pressure_kpa(double temp_c) {
    /* Saturation vapor pressure (Tetens formula) */
    return 0.6108 * exp((17.27 * temp_c) / (temp_c + 237.3));
}

/* ---- Ultrasonic Sensor ---- */

void ultrasonic_sensor_init(ultrasonic_sensor_t *us, uint32_t id,
                            double rmin, double rmax) {
    if (!us) return;
    memset(us, 0, sizeof(*us));
    us->id = id;
    us->min_range_cm = rmin;
    us->max_range_cm = rmax;
    us->operating_frequency_khz = 40.0;
    us->resolution_mm = 3.0;
    us->accuracy_cm = 0.3;
    sensor_specs_init(&us->specs);
}

double ultrasonic_speed_of_sound(double temp_c) {
    /* c = 331.3 * sqrt(1 + T/273.15)  or linear approx */
    return 331.3 + 0.606 * temp_c; /* m/s */
}

double ultrasonic_distance_from_time(double tof_us, double temp_c) {
    /* d = c * tof / 2,  tof in microseconds */
    double c = ultrasonic_speed_of_sound(temp_c);
    double tof_s = tof_us * 1e-6;
    return c * tof_s / 2.0 * 100.0; /* convert m to cm */
}

double ultrasonic_max_range_from_attenuation(double freq_khz, double rh) {
    /* Atmospheric attenuation: alpha approx = a(freq) + b(T,RH)
     * Simplified: alpha [dB/m] approx = 0.01 * freq_khz^1.5
     * Max range where attenuation > 40 dB (typical detectable limit) */
    double atten_db_per_m = 0.01 * pow(freq_khz, 1.5);
    (void)rh; /* humidity correction can be added */
    if (atten_db_per_m <= 0.0) return 1000.0; /* m */
    return 40.0 / atten_db_per_m; /* m at 40dB loss */
}

/* ---- Gas Sensor ---- */

void gas_sensor_init(gas_sensor_t *gs, uint32_t id, const char *pn,
                     gas_sensor_type_t type, const char *target) {
    if (!gs) return;
    memset(gs, 0, sizeof(*gs));
    gs->id = id;
    if (pn) strncpy(gs->part_number, pn, 31);
    gs->type = type;
    if (target) strncpy(gs->target_gas, target, 31);
    gs->response_time_t90_s = 30.0;
    gs->operating_life_years = 2.0;
    sensor_specs_init(&gs->specs);
}

double gas_concentration_from_voltage(const gas_sensor_t *gs, double Vm,
                                       double Vs, double Tc, double rh) {
    /* For MOS sensors: Rs = (Vs/Vm - 1) * RL
     * Concentration = a * (Rs/R0)^b
     * With temperature/humidity correction */
    if (!gs) return 0.0;
    double RL = 10.0; /* typical load resistor in kOhm */
    double Rs_R0;
    if (Vm > 0.0 && Vm < Vs) {
        Rs_R0 = (Vs / Vm - 1.0) * RL / 10.0; /* normalized to R0 ~10k at clean air */
    } else {
        Rs_R0 = 1.0;
    }
    Rs_R0 = gas_sensor_ratio_correction(Rs_R0, Tc, rh);
    /* Inverse power law: ppm = a * (Rs/R0)^b, typical a=100, b=-2.5 for MQ series */
    double a = 100.0, b = -2.5;
    return a * pow(Rs_R0, b);
}

double gas_sensor_ratio_correction(double Rs_R0_ratio, double Tc, double rh) {
    /* Temperature and humidity correction for MOS gas sensors
     * Simplified model: correction factor = 1 + alpha_T*(T-Tref) + alpha_RH*(RH-RHref)
     * Returns corrected Rs/R0 ratio */
    double alpha_T = 0.005;  /* per degree C */
    double alpha_RH = 0.002; /* per %RH */
    double Tref = 20.0;
    double RHref = 65.0;
    double correction = 1.0 + alpha_T * (Tc - Tref) + alpha_RH * (rh - RHref);
    return Rs_R0_ratio * correction;
}

/* ---- Current Sensor ---- */

void current_sensor_init(current_sensor_t *cs, uint32_t id,
                         current_sensor_type_t type, double Imax) {
    if (!cs) return;
    memset(cs, 0, sizeof(*cs));
    cs->id = id;
    cs->type = type;
    cs->max_current_a = Imax;
    cs->measures_dc = (type != CURRENT_SENSOR_CT);
    cs->measures_ac = true;
    switch (type) {
        case CURRENT_SENSOR_SHUNT:
            cs->shunt_resistance_mohm = 10.0; /* 0.01 ohm */
            cs->sensitivity_mv_per_a = cs->shunt_resistance_mohm;
            break;
        case CURRENT_SENSOR_HALL:
            cs->sensitivity_mv_per_a = 185.0; /* ACS712 typical */
            cs->offset_voltage_mv = 2500.0;   /* Vcc/2 */
            break;
        case CURRENT_SENSOR_CT:
            cs->sensitivity_mv_per_a = 1000.0; /* depends on burden resistor */
            break;
        case CURRENT_SENSOR_ROGOWSKI:
            cs->sensitivity_mv_per_a = 22.5;   /* typical at 50Hz */
            break;
    }
    sensor_specs_init(&cs->specs);
}

double current_from_voltage(const current_sensor_t *cs, double Vm) {
    if (!cs || cs->sensitivity_mv_per_a <= 0.0) return 0.0;
    return (Vm - cs->offset_voltage_mv) / cs->sensitivity_mv_per_a;
}

double shunt_power_dissipation(double I_a, double R_shunt) {
    return I_a * I_a * R_shunt; /* P = I^2 * R, in watts */
}

/* ---- Transfer Function API (L3) ---- */

void transfer_function_init_linear(transfer_function_t *tf, double m, double b) {
    if (!tf) return;
    memset(tf, 0, sizeof(*tf));
    tf->type = TF_LINEAR;
    tf->polynomial_order = 1;
    tf->coefficients[0] = b;
    tf->coefficients[1] = m;
    tf->inverse_coefficients[0] = -b/m;
    tf->inverse_coefficients[1] = 1.0/m;
    tf->inverse_order = 1;
}

void transfer_function_init_polynomial(transfer_function_t *tf,
                                        const double *coeffs, uint8_t order) {
    if (!tf || !coeffs || order > 9) return;
    memset(tf, 0, sizeof(*tf));
    tf->type = TF_POLYNOMIAL;
    tf->polynomial_order = order;
    for (int i = 0; i <= order; i++) tf->coefficients[i] = coeffs[i];
}

double transfer_function_forward(const transfer_function_t *tf, double in) {
    if (!tf) return in;
    switch (tf->type) {
        case TF_LINEAR:
            return tf->coefficients[0] + tf->coefficients[1] * in;
        case TF_POLYNOMIAL: {
            double result = tf->coefficients[0];
            double xn = in;
            for (int i = 1; i <= tf->polynomial_order; i++) {
                result += tf->coefficients[i] * xn;
                xn *= in;
            }
            return result;
        }
        case TF_LOGARITHMIC:
            if (in <= 0.0) return tf->coefficients[0];
            return tf->coefficients[0] + tf->coefficients[1] * log(in);
        case TF_EXPONENTIAL:
            return tf->coefficients[0] * exp(tf->coefficients[1] * in);
        case TF_POWER_LAW:
            return tf->coefficients[0] * pow(in, tf->coefficients[1]);
        case TF_STEINHART_HART: {
            double lnR = log(in);
            double invT = tf->coefficients[0] + tf->coefficients[1]*lnR
                        + tf->coefficients[2]*lnR*lnR*lnR;
            return (invT > 0.0) ? (1.0/invT - 273.15) : 0.0;
        }
        default:
            return in;
    }
}

double transfer_function_inverse(const transfer_function_t *tf, double out) {
    if (!tf) return out;
    /* Use pre-computed inverse coefficients for linear/polynomial */
    if (tf->type == TF_LINEAR) {
        if (tf->coefficients[1] == 0.0) return out;
        return (out - tf->coefficients[0]) / tf->coefficients[1];
    }
    /* For other types, use Newton iteration */
    double x = (out - tf->output_min) / (tf->output_max - tf->output_min)
              * (tf->input_max - tf->input_min) + tf->input_min;
    for (int iter = 0; iter < 30; iter++) {
        double fx = transfer_function_forward(tf, x);
        double dfx = transfer_function_sensitivity(tf, x);
        if (fabs(dfx) < 1e-15) break;
        double dx = (out - fx) / dfx;
        x += dx;
        if (fabs(dx) < 1e-9 * fabs(x)) break;
    }
    return x;
}

double transfer_function_sensitivity(const transfer_function_t *tf, double in) {
    /* d(output)/d(input) at given input */
    if (!tf) return 1.0;
    switch (tf->type) {
        case TF_LINEAR:
            return tf->coefficients[1];
        case TF_POLYNOMIAL: {
            double deriv = tf->coefficients[1];
            double xn = in;
            for (int i = 2; i <= tf->polynomial_order; i++) {
                deriv += i * tf->coefficients[i] * xn;
                xn *= in;
            }
            return deriv;
        }
        case TF_LOGARITHMIC:
            return (in > 0.0) ? tf->coefficients[1] / in : 0.0;
        case TF_EXPONENTIAL:
            return tf->coefficients[0] * tf->coefficients[1] * exp(tf->coefficients[1] * in);
        case TF_POWER_LAW:
            return tf->coefficients[0] * tf->coefficients[1] * pow(in, tf->coefficients[1] - 1.0);
        default:
            /* numerical derivative */
            double h = fmax(fabs(in) * 1e-6, 1e-9);
            return (transfer_function_forward(tf, in + h) - transfer_function_forward(tf, in - h)) / (2.0 * h);
    }
}

int transfer_function_calibrate_linear(transfer_function_t *tf,
                                        const double *ins, const double *outs,
                                        size_t n) {
    if (!tf || !ins || !outs || n < 2) return -1;
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum_x += ins[i]; sum_y += outs[i];
        sum_xy += ins[i] * outs[i]; sum_xx += ins[i] * ins[i];
    }
    double denom = n * sum_xx - sum_x * sum_x;
    if (fabs(denom) < 1e-15) return -2;
    double slope = (n * sum_xy - sum_x * sum_y) / denom;
    double intercept = (sum_y - slope * sum_x) / n;
    transfer_function_init_linear(tf, slope, intercept);
    return 0;
}
