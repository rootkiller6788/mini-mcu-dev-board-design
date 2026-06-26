/**
 * @file    example_gas_sensor_shield.c
 * @brief   L7 Application: Environmental Gas Sensor Shield for IoT Air Quality
 *
 * @details End-to-end example of an air quality monitoring sensor shield:
 *          1. MQ-135 gas sensor (CO2, NH3, benzene) on Arduino R3 shield
 *          2. CCS811 VOC/eCO2 sensor on I2C bus
 *          3. DHT22 temperature/humidity for gas sensor compensation
 *          4. Moving average + EMA filtering
 *          5. Temperature/humidity compensated gas readings
 *
 * Reference: MQ-135 datasheet, CCS811 datasheet, WHO air quality guidelines
 *            IoT environmental monitoring (Detroit air quality project)
 */

#include <stdio.h>
#include <math.h>
#include "sensor_types.h"
#include "signal_conditioning.h"
#include "sensor_filter.h"
#include <string.h>
#include "shield_interface.h"

int main(void) {
    printf("=== IoT Air Quality Sensor Shield ===\n\n");

    /* 1. Define shield assembly */
    shield_assembly_t shield;
    shield_assembly_init(&shield, "AQ_SHIELD_V1", SHIELD_FORM_ARDUINO_UNO_R3);
    shield_assembly_add_i2c_sensor(&shield, 0x5A); /* CCS811 */
    shield_assembly_add_i2c_sensor(&shield, 0x76); /* BME280 */
    shield_assembly_add_analog_sensor(&shield);     /* MQ-135 */
    strcpy(shield.revision, "1.0");

    int status = shield_assembly_validate(&shield); (void)status;
    printf("Shield: %s rev %s\n", shield.name, shield.revision);
    printf("Form factor: Arduino Uno R3 compatible\n");
    printf("Sensors: 2x I2C + 1x Analog\n");
    shield_assembly_bom_summary(&shield);

    /* 2. Configure MQ-135 gas sensor analog frontend */
    gas_sensor_t mq135;
    gas_sensor_init(&mq135, 1, "MQ-135", GAS_SENSOR_MOS, "CO2/NH3/Benzene");
    mq135.heater_voltage = 5.0;
    mq135.heater_current_ma = 150.0;
    mq135.requires_warmup = true;
    mq135.warmup_time_s = 60.0;
    printf("\nMQ-135: heater %.1fV @ %.0fmA, warmup %.0fs\n",
           mq135.heater_voltage, mq135.heater_current_ma, mq135.warmup_time_s);

    /* 3. Configure analog frontend for MQ-135 */
    analog_frontend_t afe;
    analog_frontend_init(&afe, "MQ135_CH");
    afe.adc_vref = 5.0;
    afe.adc_resolution_bits = 10;
    afe.adc_lsb_voltage = 5.0 / 1024.0;

    /* Add simple RC filter: R=1K, C=100nF -> fc=1.6kHz */
    input_protection_init(&afe.protection, 5.0);
    afe.protection.has_rc_filter = true;
    afe.protection.R_series = 1000.0;
    afe.protection.C_to_gnd = 100e-9;
    printf("Anti-aliasing filter: fc=%.0fHz\n",
           input_protection_cutoff_freq(&afe.protection));

    /* 4. Set up digital filters */
    moving_average_filter_t maf;
    moving_average_init(&maf, 16);

    ema_filter_t ema;
    ema_filter_init_from_cutoff(&ema, 0.1, 10.0); /* very slow, 0.1Hz */

    printf("\nFilter chain: MA(16) + EMA(fc=0.1Hz)\n\n");

    /* 5. Simulate gas concentration readings */
    printf("Air quality monitoring sequence:\n");
    printf("  Time(min)  V_sensor  Rs/R0    CO2(ppm)  MA_CO2   EMA_CO2  Status\n");
    printf("  ---------  --------  ------   --------  -------  -------  ------\n");

    double baseline_co2[] = {400, 410, 405, 420, 450, 500, 600, 700, 800,
                              750, 650, 550, 480, 430, 410, 400};
    int n = sizeof(baseline_co2) / sizeof(baseline_co2[0]);
    moving_average_reset(&maf);
    ema_filter_reset(&ema);

    for (int i = 0; i < n; i++) {
        double co2_true = baseline_co2[i];
        double Rs_R0 = pow(co2_true / 100.0, -0.4); /* typical power law */
        double V_sensor = 5.0 * 10.0 / (Rs_R0 * 10.0 + 10.0); /* voltage divider */
        double V_afe = analog_frontend_process(&afe, V_sensor);
        uint32_t adc = analog_frontend_to_adc_counts(&afe, V_afe);
        double V_adc = analog_frontend_from_adc_counts(&afe, adc);

        /* Gas concentration from voltage */
        double co2_raw = gas_concentration_from_voltage(&mq135, V_adc, 5.0, 25.0, 50.0);
        double co2_ma = moving_average_update(&maf, co2_raw);
        double co2_ema = ema_filter_update(&ema, co2_ma);

        const char *status = "GOOD";
        if (co2_ema > 1000.0) status = "POOR";
        else if (co2_ema > 600.0) status = "FAIR";

        printf("  %8.0f   %7.3f   %5.2f    %7.0f   %6.0f   %6.0f   %s\n",
               i * 0.5, V_sensor, Rs_R0, co2_true, co2_ma, co2_ema, status);
    }

    printf("\nAir quality monitoring shield complete.\n");
    printf("\nDesign considerations for IoT deployment (Detroit air quality study):\n");
    printf("  1. MQ-135 requires 24h+ burn-in before calibration\n");
    printf("  2. Temperature/humidity compensation critical for accuracy\n");
    printf("  3. CCS811 I2C sensor provides digital eCO2 + TVOC\n");
    printf("  4. Data logging to SD card or WiFi (ESP8266 co-processor)\n");
    printf("  5. Shield power: ~200mA continuous (heater dominant)\n");

    return 0;
}
