/**
 * @file    example_thermistor_shield.c
 * @brief   L6 Canonical Problem: Thermistor Temperature Sensor Shield Design
 *
 * @details Complete end-to-end example of designing an Arduino Uno R3
 *          thermistor sensor shield:
 *          1. Select NTC thermistor (10K, beta=3950)
 *          2. Design voltage divider with optimal fixed resistor
 *          3. Apply Steinhart-Hart calibration
 *          4. Configure analog frontend with anti-aliasing filter
 *          5. Convert ADC reading to temperature
 *          6. Apply EMA filter for smoothing
 *
 * Reference: Fraden (2016), Arduino analog input tutorial
 */

#include <stdio.h>
#include <math.h>
#include "sensor_types.h"
#include "signal_conditioning.h"
#include "sensor_calibration.h"
#include "sensor_filter.h"

int main(void) {
    printf("=== Thermistor Sensor Shield Design Example ===\n\n");

    /* 1. Define the thermistor */
    thermistor_t ntc;
    thermistor_init(&ntc, 1, "NTCLE100E3103", 10000.0, 3950.0);
    thermistor_set_steinhart(&ntc, 1.129e-3, 2.341e-4, 8.775e-8);

    printf("Thermistor: R25=%.0f ohms, Beta=%.0f\n",
           ntc.resistance_at_25c, ntc.beta_value);
    printf("Steinhart-Hart: A=%.6e, B=%.6e, C=%.6e\n\n",
           ntc.steinhart_A, ntc.steinhart_B, ntc.steinhart_C);

    /* 2. Design voltage divider */
    double R_fixed = voltage_divider_optimal_r_fixed(
        thermistor_resistance_at_temp(&ntc, 25.0), 5.0, 0.1);
    printf("Optimal fixed resistor: %.0f ohms\n", R_fixed);

    voltage_divider_t vd;
    voltage_divider_init(&vd, 10000.0, R_fixed, 5.0);
    printf("Voltage divider: R_top=sensor, R_bottom=%.0f, Vin=5.0V\n\n", R_fixed);

    /* 3. Configure analog frontend */
    analog_frontend_t afe;
    analog_frontend_init(&afe, "THERM_CH0");
    afe.adc_vref = 5.0;
    afe.adc_resolution_bits = 10;
    afe.adc_lsb_voltage = 5.0 / 1024.0;

    printf("ADC: %d-bit, Vref=%.1fV, LSB=%.3fmV\n\n",
           afe.adc_resolution_bits, afe.adc_vref, afe.adc_lsb_voltage * 1000.0);

    /* 4. Set up EMA filter */
    ema_filter_t ema;
    ema_filter_init_from_cutoff(&ema, 0.5, 10.0); /* 0.5Hz cutoff, 10Hz sample rate */
    printf("EMA filter: fc=0.5Hz, fs=10Hz\n\n");

    /* 5. Simulate temperature readings */
    printf("Temperature measurements:\n");
    printf("  Time(s)   ADC    V_div(V)   R_ntc(ohm)  T_raw(C)  T_filt(C)\n");
    printf("  --------  ---    --------   ----------  --------  ---------\n");

    double sim_temps[] = {25.0, 25.5, 30.0, 35.0, 40.0, 45.0, 50.0, 55.0,
                          60.0, 55.0, 50.0, 45.0, 40.0, 35.0, 30.0, 25.0};
    int n_samples = sizeof(sim_temps) / sizeof(sim_temps[0]);

    ema_filter_reset(&ema);
    for (int i = 0; i < n_samples; i++) {
        double T_actual = sim_temps[i];
        /* Simulate: R_ntc -> V_divider -> ADC -> R_back -> T_measured */
        double R_ntc = thermistor_resistance_at_temp(&ntc, T_actual);
        vd.R_top = R_ntc; /* sensor is R_top */
        double V_div = voltage_divider_vout(&vd);
        double V_afe = analog_frontend_process(&afe, V_div);
        uint32_t adc = analog_frontend_to_adc_counts(&afe, V_afe);
        /* Convert back */
        double V_adc = analog_frontend_from_adc_counts(&afe, adc);
        double R_measured = voltage_divider_r_from_vout(&vd, V_adc, true);
        double T_raw = thermistor_steinhart_temp(&ntc, R_measured);
        double T_filt = ema_filter_update(&ema, T_raw);

        printf("  %6.2f    %4u    %6.3f    %8.1f    %7.2f   %7.2f\n",
               i * 0.1, adc, V_div, R_ntc, T_raw, T_filt);
    }

    printf("\nSensor shield design complete.\n");
    printf("Key design decisions:\n");
    printf("  1. NTC thermistor in voltage divider (R_bottom fixed)\n");
    printf("  2. 10-bit ADC with 5V reference => ~4.9mV resolution\n");
    printf("  3. EMA filter smooths temperature readings\n");
    printf("  4. Shield form factor: Arduino Uno R3 compatible\n");

    return 0;
}
