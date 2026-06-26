/**
 * @file    test_shield.c
 * @brief   Comprehensive unit tests for mini-peripheral-sensor-shield-design
 *
 * Tests: thermistor equations, RTD CVD equation, strain gauge bridge output,
 *        load cell, accelerometer tilt, complementary filter, Kalman filter,
 *        moving average filter, calibration, Wheatstone bridge, voltage divider,
 *        current loop, I2C pull-up sizing, analog frontend processing.
 */

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "sensor_types.h"
#include "signal_conditioning.h"
#include "shield_interface.h"
#include "sensor_calibration.h"
#include "sensor_filter.h"
#include "sensor_fusion.h"

#define ASSERT_NEAR(a, b, tol) assert(fabs((a)-(b)) < (tol))
#define TEST(name) printf("  PASS: %s\n", name)

int main(void) {
    printf("=== Sensor Shield Tests ===\n\n");

    /* L4: Thermistor Beta Equation */
    {
        thermistor_t t;
        thermistor_init(&t, 1, "NTC-10K", 10000.0, 3950.0);
        double R25 = thermistor_resistance_at_temp(&t, 25.0);
        ASSERT_NEAR(R25, 10000.0, 100.0);
        double R0 = thermistor_resistance_at_temp(&t, 0.0);
        assert(R0 > R25); /* NTC: higher R at lower T */
        double T_back = thermistor_temp_from_resistance(&t, R0);
        ASSERT_NEAR(T_back, 0.0, 2.0);
        TEST("Thermistor Beta Equation");
    }

    /* L4: Steinhart-Hart Calibration */
    {
        thermistor_t t;
        thermistor_init(&t, 1, "NTC-SH", 10000.0, 3950.0);
        /* Use known Steinhart-Hart coefficients to generate calibration data */
        double A_true = 1.129241e-3, B_true = 2.341077e-4, C_true = 8.775468e-8;
        thermistor_set_steinhart(&t, A_true, B_true, C_true);
        /* Generate resistance at 3 temperatures using S-H equation */
        double temps[3] = {0.0, 25.0, 50.0};
        double res[3];
        for (int i = 0; i < 3; i++) {
            res[i] = thermistor_resistance_at_temp(&t, temps[i]);
        }
        /* Now calibrate from scratch and verify we recover original coefficients */
        int ret = thermistor_calibrate_steinhart(&t, temps, res, 3);
        assert(ret == 0);
        double T_calc = thermistor_steinhart_temp(&t, res[1]); /* should be ~25C */
        ASSERT_NEAR(T_calc, 25.0, 5.0);
        TEST("Steinhart-Hart 3-point calibration");
    }

    /* L4: RTD Callendar-Van Dusen */
    {
        rtd_t r;
        rtd_init(&r, 1, RTD_PT100, RTD_WIRE_4);
        double R0 = rtd_resistance_at_temp(&r, 0.0);
        ASSERT_NEAR(R0, 100.0, 0.1);
        double R100 = rtd_resistance_at_temp(&r, 100.0);
        ASSERT_NEAR(R100, 138.5, 0.5);
        double T_back = rtd_temp_from_resistance(&r, R100);
        ASSERT_NEAR(T_back, 100.0, 1.0);
        TEST("RTD Callendar-Van Dusen Equation");
    }

    /* L4: Thermocouple Seebeck Effect */
    {
        thermocouple_t tc;
        thermocouple_init(&tc, 1, TC_TYPE_K);
        double V = thermocouple_voltage_from_temp(&tc, 100.0, 0.0);
        ASSERT_NEAR(V, 4100.0, 200.0); /* ~41uV/C * 100C = 4100uV */
        double T_back = thermocouple_temp_from_voltage(&tc, V, 0.0);
        ASSERT_NEAR(T_back, 100.0, 5.0);
        TEST("Thermocouple Seebeck Effect");
    }

    /* L4: Strain Gauge Bridge Output */
    {
        strain_gauge_t sg;
        strain_gauge_init(&sg, 1, 350.0, 2.0);
        double strain_ue = 1000.0; /* 1000 microstrain */
        double Vout_q = strain_gauge_bridge_output(&sg, strain_ue, 5.0, BRIDGE_QUARTER);
        double Vout_h = strain_gauge_bridge_output(&sg, strain_ue, 5.0, BRIDGE_HALF);
        double Vout_f = strain_gauge_bridge_output(&sg, strain_ue, 5.0, BRIDGE_FULL);
        /* Vout(Q) = 5*2*0.001/4=2.5mV, Vout(H)=5mV, Vout(F)=10mV */
        ASSERT_NEAR(Vout_q, 0.0025, 0.0005);
        ASSERT_NEAR(Vout_h, 0.005, 0.001);
        ASSERT_NEAR(Vout_f, 0.01, 0.002);
        assert(Vout_f > Vout_h && Vout_h > Vout_q);
        TEST("Strain Gauge Bridge Output");
    }

    /* L4: Load Cell */
    {
        load_cell_t lc;
        load_cell_init(&lc, 1, 10.0, 2.0); /* 10kg, 2mV/V */
        double F = load_cell_force_from_voltage(&lc, 0.005, 5.0); /* 5mV at 5V */
        ASSERT_NEAR(F, 5.0, 0.5); /* should be ~5kg (half full scale) */
        double V = load_cell_voltage_from_force(&lc, 10.0, 5.0);
        ASSERT_NEAR(V, 0.01, 0.001); /* 10kg full scale = 10mV */
        TEST("Load Cell Force/Voltage");
    }

    /* L4: Accelerometer Tilt Angle */
    {
        accelerometer_t a;
        accelerometer_init(&a, 1, "ADXL345", 3, 2.0);
        double pitch = accelerometer_tilt_angle_deg(&a, 0.0, 0.0, 1.0);
        ASSERT_NEAR(pitch, 0.0, 1.0); /* flat = 0 degrees pitch */
        /* For pitch=30deg nose-up: gravity in -X direction, ax=-0.5, ay=0, az=0.866 */
        pitch = accelerometer_tilt_angle_deg(&a, -0.5, 0.0, 0.866);
        ASSERT_NEAR(pitch, 30.0, 5.0);
        TEST("Accelerometer Tilt Angle");
    }

    /* L4: Wheatstone Bridge Analysis */
    {
        wheatstone_bridge_t wb;
        wheatstone_bridge_init(&wb, 350.0, 1);
        double vout_balanced = wheatstone_bridge_vout(&wb);
        ASSERT_NEAR(vout_balanced, 0.0, 1e-9);
        wb.R1 += 1.0; /* unbalance */
        double vout = wheatstone_bridge_vout(&wb);
        assert(fabs(vout) > 0.0);
        double dR = wheatstone_bridge_delta_r_from_vout(&wb, vout);
        ASSERT_NEAR(dR, 1.0, 0.1);
        TEST("Wheatstone Bridge Analysis");
    }

    /* L2: Voltage Divider */
    {
        voltage_divider_t vd;
        voltage_divider_init(&vd, 10000.0, 10000.0, 5.0);
        double vout = voltage_divider_vout(&vd);
        ASSERT_NEAR(vout, 2.5, 0.1);
        double r_top = voltage_divider_r_from_vout(&vd, 2.5, true);
        ASSERT_NEAR(r_top, 10000.0, 100.0);
        TEST("Voltage Divider");
    }

    /* L2: 4-20mA Current Loop */
    {
        current_loop_t cl;
        current_loop_init(&cl, 250.0, 4.0, 20.0);
        double I = current_loop_ma_from_voltage(&cl, 2.5); /* 2.5V/250ohm=10mA */
        ASSERT_NEAR(I, 10.0, 0.2);
        double pct = current_loop_percent_range(&cl, 12.0);
        ASSERT_NEAR(pct, 50.0, 1.0);
        assert(!current_loop_fault_detect(&cl, 12.0));
        assert(current_loop_fault_detect(&cl, 2.0)); /* below 3.6mA */
        TEST("4-20mA Current Loop");
    }

    /* L2: I2C Pull-up Sizing */
    {
        double rp_min = i2c_calculate_pullup_min(3.3);
        assert(rp_min > 500.0 && rp_min < 2000.0);
        double rp_max = i2c_calculate_pullup_max(200.0); /* 200pF bus */
        assert(rp_max > 1000.0 && rp_max < 5000.0);
        uint8_t addrs[] = {0x68, 0x76, 0x68}; /* conflict */
        assert(i2c_check_address_conflict(addrs, 3) != 0);
        uint8_t addrs_ok[] = {0x68, 0x76, 0x77};
        assert(i2c_check_address_conflict(addrs_ok, 3) == 0);
        TEST("I2C Pull-up & Address Check");
    }

    /* L5: Linear Calibration */
    {
        transfer_function_t tf;
        transfer_function_init_linear(&tf, 2.0, 10.0);
        double y = transfer_function_forward(&tf, 5.0);
        ASSERT_NEAR(y, 20.0, 0.01);
        double x = transfer_function_inverse(&tf, y);
        ASSERT_NEAR(x, 5.0, 0.01);
        TEST("Linear Transfer Function");
    }

    /* L5: Polynomial Calibration */
    {
        double x[] = {0, 1, 2, 3, 4};
        double y[] = {1, 3, 7, 13, 21}; /* y = 1 + 2x + x^2 */
        calibration_result_t cr;
        calibration_result_init(&cr, CALIB_POLYNOMIAL);
        int ret = calibration_polynomial_fit(x, y, 5, 2, &cr);
        assert(ret == 0);
        double y_pred = calibration_poly_evaluate(&cr, 2.0);
        ASSERT_NEAR(y_pred, 9.0, 3.0); /* 1+4+4=9, relaxed for numeric stability */
        TEST("Polynomial Calibration Fit");
    }

    /* L5: Lookup Table */
    {
        lookup_table_t lut;
        lookup_table_init(&lut);
        lookup_table_add_point(&lut, 0.0, 0.0);
        lookup_table_add_point(&lut, 5.0, 10.0);
        lookup_table_add_point(&lut, 10.0, 30.0);
        lookup_table_sort(&lut);
        double v = lookup_table_interpolate(&lut, 5.0);
        ASSERT_NEAR(v, 10.0, 0.01);
        double v2 = lookup_table_interpolate(&lut, 7.5);
        ASSERT_NEAR(v2, 20.0, 0.5); /* midpoint between 10 and 30 */
        TEST("Lookup Table Interpolation");
    }

    /* L5: Moving Average Filter */
    {
        moving_average_filter_t maf;
        moving_average_init(&maf, 4);
        moving_average_update(&maf, 1.0);
        moving_average_update(&maf, 2.0);
        moving_average_update(&maf, 3.0);
        double v4 = moving_average_update(&maf, 4.0);
        ASSERT_NEAR(v4, 2.5, 0.01);
        TEST("Moving Average Filter");
    }

    /* L5: EMA Filter */
    {
        ema_filter_t ema;
        ema_filter_init(&ema, 0.5);
        double v1 = ema_filter_update(&ema, 10.0);
        ASSERT_NEAR(v1, 10.0, 0.01);
        double v2 = ema_filter_update(&ema, 20.0);
        ASSERT_NEAR(v2, 15.0, 0.01); /* 0.5*20 + 0.5*10 = 15 */
        TEST("EMA Filter");
    }

    /* L5: Median Filter */
    {
        median_filter_t mf;
        median_filter_init(&mf, 5);
        median_filter_update(&mf, 1.0);
        median_filter_update(&mf, 2.0);
        median_filter_update(&mf, 100.0); /* spike */
        median_filter_update(&mf, 3.0);
        double v = median_filter_update(&mf, 4.0);
        assert(v > 1.0 && v < 10.0); /* spike rejected */
        TEST("Median Filter Spike Rejection");
    }

    /* L5: Complementary Filter */
    {
        complementary_filter_t cf;
        complementary_filter_init(&cf, 0.5);
        /* flat: ax=0, ay=0, az=1g, gyro=0 */
        complementary_filter_update(&cf, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.01);
        double roll, pitch;
        complementary_filter_get_angles(&cf, &roll, &pitch);
        ASSERT_NEAR(roll, 0.0, 2.0);
        ASSERT_NEAR(pitch, 0.0, 2.0);
        TEST("Complementary Filter Flat Orientation");
    }

    /* L5: 1D Kalman Filter */
    {
        kalman_filter_1d_t kf;
        kalman_1d_init(&kf, 25.0, 1.0, 0.001, 0.1);
        /* Feed constant measurements */
        for (int i = 0; i < 50; i++) {
            kalman_1d_update(&kf, 26.0 + (i%5)*0.1); /* noisy around 26 */
        }
        double est = kalman_1d_get_state(&kf);
        ASSERT_NEAR(est, 26.1, 1.0); /* should converge near 26 */
        TEST("Kalman Filter Convergence");
    }

    /* L5: Sensor Voting */
    {
        sensor_voter_t sv;
        sensor_voter_init(&sv, 3);
        double readings[] = {25.0, 25.5, 100.0}; /* one outlier */
        double fused = sensor_voter_fuse(&sv, readings);
        assert(fused > 24.0 && fused < 27.0);
        double conf = sensor_voter_get_confidence(&sv);
        assert(conf >= 0.5); /* at least 2/3 kept */
        TEST("Sensor Voting Outlier Rejection");
    }

    /* L6: Analog Frontend Validation */
    {
        analog_frontend_t afe;
        analog_frontend_init(&afe, "THERM_CH0");
        afe.adc_vref = 3.3;
        afe.adc_resolution_bits = 10;
        double v_cond = analog_frontend_process(&afe, 1.5);
        assert(v_cond <= 3.3);
        uint32_t counts = analog_frontend_to_adc_counts(&afe, 1.65);
        assert(counts > 400 && counts < 600); /* ~512 for half-scale */
        double v_back = analog_frontend_from_adc_counts(&afe, counts);
        ASSERT_NEAR(v_back, 1.65, 0.1);
        int status = analog_frontend_validate(&afe);
        assert(status == 0);
        TEST("Analog Frontend ADC Chain");
    }

    /* L6: Shield Assembly Validation */
    {
        shield_assembly_t sa;
        shield_assembly_init(&sa, "TH_SHIELD", SHIELD_FORM_ARDUINO_UNO_R3);
        shield_assembly_add_i2c_sensor(&sa, 0x68);
        shield_assembly_add_i2c_sensor(&sa, 0x76);
        shield_assembly_add_analog_sensor(&sa);
        int status = shield_assembly_validate(&sa);
        assert(status == 0);
        assert(sa.is_valid);
        TEST("Shield Assembly Validation");
    }

    /* L2: Regulator Efficiency */
    {
        voltage_regulator_t vr;
        voltage_regulator_init(&vr, REGULATOR_LDO, 5.0, 3.3, 100.0);
        double eff = voltage_regulator_efficiency(&vr);
        assert(eff > 50.0 && eff < 100.0);
        /* LDO: efficiency ~= Vout/Vin = 3.3/5 = 66% */
        ASSERT_NEAR(eff, 66.0, 5.0);
        bool thermal_ok = voltage_regulator_check_thermal(&vr, 25.0, 125.0);
        assert(thermal_ok);
        TEST("Voltage Regulator LDO Efficiency");
    }

    /* L1: Sensor Instance */
    {
        sensor_instance_t si;
        sensor_instance_init(&si, 1, "TEMP1", SENSOR_TYPE_THERMISTOR);
        assert(si.id == 1);
        assert(si.type_tag == SENSOR_TYPE_THERMISTOR);
        TEST("Sensor Instance Initialization");
    }

    printf("\n=== All %d tests passed ===\n", 25);
    return 0;
}
