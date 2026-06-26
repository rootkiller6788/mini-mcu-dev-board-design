/**
 * @file    sensor_types.h
 * @brief   L1 Core Definitions for Sensor Shield Design
 *
 * Knowledge Mapping:
 *   L1 - 18 sensor type definitions (thermistor, RTD, thermocouple, strain gauge,
 *        load cell, accelerometer, gyroscope, magnetometer, photodiode, pressure,
 *        humidity, ultrasonic, gas, current, Hall, PIR, color, temperature IC)
 *   L2 - Sensor signal chain, output interface types
 *   L3 - Transfer function structures, sensitivity models
 *   L4 - Steinhart-Hart equation, Callendar-Van Dusen equation, Seebeck effect,
 *        Wheatstone bridge, Gauge Factor
 *
 * Reference: Fraden "Handbook of Modern Sensors" (5th ed., 2016)
 *            Wilson "Sensor Technology Handbook" (2005)
 */

#ifndef SENSOR_TYPES_H
#define SENSOR_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Sensor transduction principle taxonomy (L1) */
typedef enum {
    SENSOR_PRINCIPLE_RESISTIVE = 0, SENSOR_PRINCIPLE_CAPACITIVE,
    SENSOR_PRINCIPLE_INDUCTIVE, SENSOR_PRINCIPLE_PIEZOELECTRIC,
    SENSOR_PRINCIPLE_THERMOELECTRIC, SENSOR_PRINCIPLE_PHOTOELECTRIC,
    SENSOR_PRINCIPLE_MEMS_ACCELEROMETER, SENSOR_PRINCIPLE_MEMS_GYROSCOPE,
    SENSOR_PRINCIPLE_HALL_EFFECT, SENSOR_PRINCIPLE_MAGNETORESISTIVE,
    SENSOR_PRINCIPLE_ELECTROCHEMICAL, SENSOR_PRINCIPLE_ULTRASONIC,
    SENSOR_PRINCIPLE_PYROELECTRIC, SENSOR_PRINCIPLE_STRAIN_GAUGE,
    SENSOR_PRINCIPLE_OPTICAL_ENCODER, SENSOR_PRINCIPLE_TIME_OF_FLIGHT,
    SENSOR_PRINCIPLE_EDDY_CURRENT, SENSOR_PRINCIPLE_LVDT
} sensor_principle_t;

/* Sensor output interface type (L2) */
typedef enum {
    SENSOR_OUTPUT_ANALOG_VOLTAGE = 0, SENSOR_OUTPUT_ANALOG_CURRENT,
    SENSOR_OUTPUT_DIGITAL_I2C, SENSOR_OUTPUT_DIGITAL_SPI,
    SENSOR_OUTPUT_DIGITAL_UART, SENSOR_OUTPUT_DIGITAL_ONEWIRE,
    SENSOR_OUTPUT_FREQUENCY, SENSOR_OUTPUT_PULSE_WIDTH,
    SENSOR_OUTPUT_RATIOMETRIC, SENSOR_OUTPUT_WHEATSTONE_BRIDGE,
    SENSOR_OUTPUT_DIFFERENTIAL, SENSOR_OUTPUT_QUADRATURE_ENCODER
} sensor_output_type_t;

/* Physical quantity taxonomy (L1) */
typedef enum {
    PHYSICAL_QUANTITY_TEMPERATURE = 0, PHYSICAL_QUANTITY_PRESSURE,
    PHYSICAL_QUANTITY_HUMIDITY, PHYSICAL_QUANTITY_ACCELERATION,
    PHYSICAL_QUANTITY_ANGULAR_VELOCITY, PHYSICAL_QUANTITY_MAGNETIC_FIELD,
    PHYSICAL_QUANTITY_LIGHT_INTENSITY, PHYSICAL_QUANTITY_DISTANCE,
    PHYSICAL_QUANTITY_FORCE, PHYSICAL_QUANTITY_TORQUE, PHYSICAL_QUANTITY_STRAIN,
    PHYSICAL_QUANTITY_FLOW_RATE, PHYSICAL_QUANTITY_GAS_CONCENTRATION,
    PHYSICAL_QUANTITY_PH, PHYSICAL_QUANTITY_CURRENT, PHYSICAL_QUANTITY_VOLTAGE,
    PHYSICAL_QUANTITY_SOUND_LEVEL, PHYSICAL_QUANTITY_PROXIMITY,
    PHYSICAL_QUANTITY_COLOR, PHYSICAL_QUANTITY_HEART_RATE
} physical_quantity_t;

/* Datasheet accuracy metrics (L1) */
typedef struct {
    double sensitivity; double sensitivity_unit_per_lsb;
    double full_scale_range_min; double full_scale_range_max;
    double resolution; double accuracy; double precision;
    double non_linearity_percent; double hysteresis_percent;
    double repeatability_percent; double long_term_drift_ppm_per_year;
    double temperature_coefficient_ppm_per_c;
    double noise_density; double noise_rms; double signal_to_noise_ratio_db;
    double bandwidth_hz; double response_time_ms; double warm_up_time_ms;
} sensor_specs_t;

/* Operating conditions (L1) */
typedef struct {
    double supply_voltage_min; double supply_voltage_max; double supply_voltage_nominal;
    double supply_current_ua; double supply_current_standby_ua;
    double operating_temp_min_c; double operating_temp_max_c;
    double storage_temp_min_c; double storage_temp_max_c;
    double max_humidity_percent; double max_vibration_g; double max_shock_g;
    uint32_t mtbf_hours;
} sensor_operating_conditions_t;

/* Units for physical quantities (L1) */
typedef enum {
    UNIT_CELSIUS = 0, UNIT_FAHRENHEIT, UNIT_KELVIN,
    UNIT_PASCAL, UNIT_BAR, UNIT_PSI, UNIT_RH_PERCENT,
    UNIT_METER_PER_SEC2, UNIT_G_FORCE, UNIT_DEG_PER_SEC, UNIT_RAD_PER_SEC,
    UNIT_GAUSS, UNIT_MICROTESLA, UNIT_LUX, UNIT_METER, UNIT_MILLIMETER,
    UNIT_NEWTON, UNIT_NEWTON_METER, UNIT_MICROSTRAIN, UNIT_LITER_PER_MIN,
    UNIT_PPM, UNIT_PPB, UNIT_PH, UNIT_AMPERE, UNIT_MILLIAMPERE,
    UNIT_VOLT, UNIT_MILLIVOLT, UNIT_DB_SPL, UNIT_COUNT, UNIT_BPM
} sensor_unit_t;

typedef struct {
    double value; sensor_unit_t unit; uint64_t timestamp_us;
    bool is_valid; uint8_t quality_indicator;
} sensor_reading_t;

/* Thermistor (NTC/PTC) - Steinhart-Hart Law (L4): 1/T = A + B*ln(R) + C*(ln(R))^3 */
typedef struct {
    uint32_t id; char part_number[32]; char manufacturer[32];
    double resistance_at_25c; double beta_value;
    double steinhart_A, steinhart_B, steinhart_C;
    double tolerance_percent; double dissipation_constant_mw_per_c;
    double thermal_time_constant_s; double max_power_mw;
    double operating_temp_min_c; double operating_temp_max_c;
    bool is_ntc; sensor_specs_t specs;
} thermistor_t;

/* RTD - Callendar-Van Dusen Equation (L4): R(T)=R0*(1+A*T+B*T^2) */
typedef enum { RTD_PT100=0, RTD_PT500, RTD_PT1000, RTD_NI100, RTD_NI1000, RTD_CU10 } rtd_type_t;
typedef enum { RTD_WIRE_2=0, RTD_WIRE_3, RTD_WIRE_4 } rtd_wiring_t;
typedef struct {
    uint32_t id; char part_number[32]; rtd_type_t type; rtd_wiring_t wiring;
    double R0; double cv_A, cv_B, cv_C; double tolerance_class_percent;
    double self_heating_coeff_mw_per_c; double excitation_current_ma;
    sensor_specs_t specs;
} rtd_t;

/* Thermocouple - Seebeck Effect (L4): V = S*(T_hot - T_cold) */
typedef enum { TC_TYPE_K=0, TC_TYPE_J, TC_TYPE_T, TC_TYPE_E, TC_TYPE_N, TC_TYPE_R, TC_TYPE_S, TC_TYPE_B } thermocouple_type_t;
typedef struct {
    uint32_t id; thermocouple_type_t type;
    double seebeck_coeff_uv_per_c; double sensitivity_uv_per_c;
    double temp_range_min_c; double temp_range_max_c; double accuracy_c;
    bool requires_cjc; sensor_specs_t specs;
} thermocouple_t;

/* Strain Gauge - Gauge Factor (L4): GF=(dR/R)/epsilon */
typedef enum { BRIDGE_QUARTER=0, BRIDGE_HALF, BRIDGE_FULL } bridge_config_t;
typedef struct {
    uint32_t id; char part_number[32]; double nominal_resistance;
    double gauge_factor; double max_strain_ue; double fatigue_life_cycles;
    double temperature_coeff_ppm_per_c; double transverse_sensitivity;
    double grid_length_mm; double grid_width_mm; bridge_config_t bridge_config;
    sensor_specs_t specs;
} strain_gauge_t;

/* Load Cell - Wheatstone bridge of strain gauges */
typedef struct {
    uint32_t id; char part_number[32]; double rated_capacity_kg;
    double rated_output_mv_per_v; double safe_overload_percent;
    double ultimate_overload_percent; double excitation_voltage_recommended;
    double excitation_voltage_max; double input_resistance; double output_resistance;
    double insulation_resistance_mohm; double creep_percent_per_30min;
    double zero_balance_percent; double compensated_temp_min_c;
    double compensated_temp_max_c; sensor_specs_t specs;
} load_cell_t;

/* MEMS Accelerometer */
typedef struct {
    uint32_t id; char part_number[32]; uint8_t num_axes;
    double full_scale_range_g; double sensitivity_mv_per_g;
    double sensitivity_lsb_per_g; double zero_g_offset_mv; double zero_g_offset_lsb;
    double noise_density_ug_per_sqrt_hz; double bias_instability_ug;
    double velocity_random_walk_m_per_s_per_sqrt_h; double bandwidth_hz;
    double odr_hz; uint8_t resolution_bits; bool has_temperature_sensor;
    bool has_fifo; sensor_output_type_t output_type; sensor_specs_t specs;
} accelerometer_t;

/* MEMS Gyroscope */
typedef struct {
    uint32_t id; char part_number[32]; uint8_t num_axes;
    double full_scale_range_dps; double sensitivity_mv_per_dps;
    double sensitivity_lsb_per_dps; double zero_rate_offset_dps;
    double noise_density_dps_per_sqrt_hz; double angle_random_walk_deg_per_sqrt_hr;
    double bias_instability_dps; double bandwidth_hz; double odr_hz;
    uint8_t resolution_bits; bool has_temperature_sensor;
    sensor_output_type_t output_type; sensor_specs_t specs;
} gyroscope_t;

/* Magnetometer */
typedef struct {
    uint32_t id; char part_number[32]; uint8_t num_axes;
    double full_scale_range_ut; double sensitivity_lsb_per_ut;
    double noise_density_nt_per_sqrt_hz; double hard_iron_offset[3];
    double soft_iron_matrix[9]; double max_non_linearity_percent;
    double bandwidth_hz; double odr_hz; uint8_t resolution_bits;
    sensor_output_type_t output_type; sensor_specs_t specs;
} magnetometer_t;

/* Photodiode */
typedef struct {
    uint32_t id; char part_number[32]; double responsivity_A_per_W;
    double dark_current_na; double shunt_resistance_mohm;
    double junction_capacitance_pf; double rise_time_us;
    double spectral_range_min_nm; double spectral_range_max_nm;
    double peak_wavelength_nm; double nep_w_per_sqrt_hz; double active_area_mm2;
    double reverse_bias_max_v; sensor_output_type_t output_type; sensor_specs_t specs;
} photodiode_t;

/* Pressure Sensor */
typedef enum { PRESSURE_ABSOLUTE=0, PRESSURE_GAUGE, PRESSURE_DIFFERENTIAL, PRESSURE_SEALED_GAUGE } pressure_type_t;
typedef struct {
    uint32_t id; char part_number[32]; pressure_type_t type;
    double pressure_min_kpa; double pressure_max_kpa;
    double sensitivity_mv_per_kpa; double sensitivity_lsb_per_kpa;
    double proof_pressure_kpa; double burst_pressure_kpa; double zero_offset_mv;
    double full_scale_span_mv; double temperature_coeff_offset_ppm_per_c;
    double temperature_coeff_span_ppm_per_c; bool has_internal_temp_sensor;
    sensor_output_type_t output_type; sensor_specs_t specs;
} pressure_sensor_t;

/* Humidity Sensor */
typedef struct {
    uint32_t id; char part_number[32]; double humidity_range_min_percent;
    double humidity_range_max_percent; double accuracy_rh_percent;
    double hysteresis_rh_percent; double response_time_s;
    double long_term_drift_rh_per_year; bool has_internal_temp_sensor;
    double temp_accuracy_c; double nominal_capacitance_pf;
    sensor_output_type_t output_type; sensor_specs_t specs;
} humidity_sensor_t;

/* Ultrasonic Distance Sensor - ToF: d = c*ToF/2 */
typedef struct {
    uint32_t id; char part_number[32]; double min_range_cm; double max_range_cm;
    double resolution_mm; double accuracy_cm; double operating_frequency_khz;
    double beam_angle_deg; double min_pulse_width_us; double max_pulse_width_us;
    bool has_temperature_compensation; sensor_output_type_t output_type; sensor_specs_t specs;
} ultrasonic_sensor_t;

/* Gas Sensor */
typedef enum { GAS_SENSOR_ELECTROCHEMICAL=0, GAS_SENSOR_MOS, GAS_SENSOR_NDIR, GAS_SENSOR_PELLISTOR, GAS_SENSOR_PID } gas_sensor_type_t;
typedef struct {
    uint32_t id; char part_number[32]; gas_sensor_type_t type; char target_gas[32];
    double detection_range_min_ppm; double detection_range_max_ppm;
    double sensitivity_na_per_ppm; double resolution_ppm; double response_time_t90_s;
    double zero_drift_ppm_per_month; double span_drift_percent_per_month;
    double operating_life_years; bool requires_warmup; double warmup_time_s;
    double heater_voltage; double heater_current_ma;
    sensor_output_type_t output_type; sensor_specs_t specs;
} gas_sensor_t;

/* Current Sensor */
typedef enum { CURRENT_SENSOR_SHUNT=0, CURRENT_SENSOR_HALL, CURRENT_SENSOR_CT, CURRENT_SENSOR_ROGOWSKI } current_sensor_type_t;
typedef struct {
    uint32_t id; char part_number[32]; current_sensor_type_t type;
    double max_current_a; double sensitivity_mv_per_a; double sensitivity_lsb_per_a;
    double shunt_resistance_mohm; double offset_voltage_mv; double bandwidth_khz;
    double isolation_voltage_kv; bool is_bidirectional; bool measures_dc;
    bool measures_ac; double accuracy_percent;
    sensor_output_type_t output_type; sensor_specs_t specs;
} current_sensor_t;

/* Hall Effect Sensor */
typedef struct {
    uint32_t id; char part_number[32]; double operate_point_gauss;
    double release_point_gauss; double hysteresis_gauss; double supply_current_ma;
    double max_frequency_hz; bool is_latching; bool has_open_drain_output;
    sensor_output_type_t output_type; sensor_specs_t specs;
} hall_sensor_t;

/* PIR Motion Sensor */
typedef struct {
    uint32_t id; char part_number[32]; double detection_range_m;
    double detection_angle_deg; double output_pulse_width_s;
    double retrigger_delay_s; double warmup_time_s;
    bool has_light_sensor; double light_threshold_lux;
    bool has_temperature_compensation; double supply_current_ua; sensor_specs_t specs;
} pir_sensor_t;

/* Color Sensor */
typedef struct {
    uint32_t id; char part_number[32]; bool has_clear_channel;
    bool has_red_channel; bool has_green_channel; bool has_blue_channel;
    bool has_ir_channel; uint8_t adc_resolution_bits; double integration_time_ms;
    double max_detectable_lux; bool has_interrupt;
    sensor_output_type_t output_type; sensor_specs_t specs;
} color_sensor_t;

/* Comprehensive Sensor Instance (Polymorphic Union) */
#define SENSOR_MAX_NAME 64
#define SENSOR_MAX_PART 40

typedef enum {
    SENSOR_TYPE_THERMISTOR = 0, SENSOR_TYPE_RTD, SENSOR_TYPE_THERMOCOUPLE,
    SENSOR_TYPE_STRAIN_GAUGE, SENSOR_TYPE_LOAD_CELL, SENSOR_TYPE_ACCELEROMETER,
    SENSOR_TYPE_GYROSCOPE, SENSOR_TYPE_MAGNETOMETER, SENSOR_TYPE_PHOTODIODE,
    SENSOR_TYPE_PRESSURE, SENSOR_TYPE_HUMIDITY, SENSOR_TYPE_ULTRASONIC,
    SENSOR_TYPE_GAS, SENSOR_TYPE_CURRENT, SENSOR_TYPE_HALL, SENSOR_TYPE_PIR,
    SENSOR_TYPE_COLOR, SENSOR_TYPE_TEMPERATURE_IC, SENSOR_TYPE_GENERIC_ADC,
    SENSOR_TYPE_GENERIC_GPIO
} sensor_type_tag_t;

typedef struct {
    uint32_t id; char name[SENSOR_MAX_NAME]; char part_number[SENSOR_MAX_PART];
    sensor_type_tag_t type_tag; sensor_principle_t principle;
    sensor_output_type_t output_type; physical_quantity_t physical_quantity;
    sensor_specs_t specs; sensor_operating_conditions_t operating;
    union {
        thermistor_t thermistor; rtd_t rtd; thermocouple_t thermocouple;
        strain_gauge_t strain_gauge; load_cell_t load_cell;
        accelerometer_t accelerometer; gyroscope_t gyroscope;
        magnetometer_t magnetometer; photodiode_t photodiode;
        pressure_sensor_t pressure; humidity_sensor_t humidity;
        ultrasonic_sensor_t ultrasonic; gas_sensor_t gas;
        current_sensor_t current; hall_sensor_t hall;
        pir_sensor_t pir; color_sensor_t color;
    } detail;
    uint8_t i2c_address; uint8_t spi_cs_pin; uint8_t analog_channel; uint8_t gpio_pin;
} sensor_instance_t;

/* Transfer Function Structure (L3) */
typedef enum { TF_LINEAR=0, TF_POLYNOMIAL, TF_LOGARITHMIC, TF_EXPONENTIAL,
               TF_POWER_LAW, TF_STEINHART_HART, TF_CALLENDAR_VAN_DUSEN,
               TF_SEEBECK_POLYNOMIAL, TF_PIECEWISE_LINEAR, TF_LOOKUP_TABLE } transfer_function_type_t;

typedef struct {
    transfer_function_type_t type; uint8_t polynomial_order;
    double coefficients[10]; double input_min; double input_max;
    double output_min; double output_max; char input_unit[16]; char output_unit[16];
    double inverse_coefficients[10]; uint8_t inverse_order;
} transfer_function_t;

/* API Declarations */
void sensor_instance_init(sensor_instance_t *s, uint32_t id, const char *name, sensor_type_tag_t tag);
void sensor_specs_init(sensor_specs_t *specs);

void thermistor_init(thermistor_t *t, uint32_t id, const char *pn, double R25, double beta);
double thermistor_resistance_at_temp(const thermistor_t *t, double temp_c);
double thermistor_temp_from_resistance(const thermistor_t *t, double resistance);
void thermistor_set_steinhart(thermistor_t *t, double A, double B, double C);
double thermistor_steinhart_temp(const thermistor_t *t, double resistance);
int thermistor_calibrate_steinhart(thermistor_t *t, const double *temps, const double *resistances, size_t n);

void rtd_init(rtd_t *r, uint32_t id, rtd_type_t type, rtd_wiring_t wiring);
double rtd_resistance_at_temp(const rtd_t *r, double temp_c);
double rtd_temp_from_resistance(const rtd_t *r, double resistance);
double rtd_wire_resistance_compensation(const rtd_t *r, double measured_R, double lead_R);

void thermocouple_init(thermocouple_t *tc, uint32_t id, thermocouple_type_t type);
double thermocouple_voltage_from_temp(const thermocouple_t *tc, double t_hot, double t_cold);
double thermocouple_temp_from_voltage(const thermocouple_t *tc, double V_uv, double t_cold);

void strain_gauge_init(strain_gauge_t *sg, uint32_t id, double R0, double gf);
double strain_gauge_strain_from_resistance_change(const strain_gauge_t *sg, double dR);
double strain_gauge_bridge_output(const strain_gauge_t *sg, double strain, double Vex, bridge_config_t cfg);

void load_cell_init(load_cell_t *lc, uint32_t id, double capacity, double ro);
double load_cell_force_from_voltage(const load_cell_t *lc, double Vm, double Vex);
double load_cell_voltage_from_force(const load_cell_t *lc, double F, double Vex);

void accelerometer_init(accelerometer_t *a, uint32_t id, const char *pn, uint8_t axes, double fs);
double accelerometer_accel_from_voltage(const accelerometer_t *a, double Vm, uint8_t axis);
double accelerometer_accel_from_lsb(const accelerometer_t *a, int16_t raw, uint8_t axis);
double accelerometer_tilt_angle_deg(const accelerometer_t *a, double ax, double ay, double az);

void gyroscope_init(gyroscope_t *g, uint32_t id, const char *pn, uint8_t axes, double fs);
double gyroscope_rate_from_voltage(const gyroscope_t *g, double Vm, uint8_t axis);
double gyroscope_rate_from_lsb(const gyroscope_t *g, int16_t raw, uint8_t axis);

void magnetometer_init(magnetometer_t *m, uint32_t id, const char *pn, uint8_t axes, double fs);
double magnetometer_field_from_lsb(const magnetometer_t *m, int16_t raw, uint8_t axis);
double magnetometer_heading_deg(const magnetometer_t *m, double mx, double my, double mz);

void photodiode_init(photodiode_t *pd, uint32_t id, const char *pn, double resp);
double photodiode_power_from_current(const photodiode_t *pd, double I_photo);
double photodiode_transimpedance_gain(double R_feedback);

void pressure_sensor_init(pressure_sensor_t *ps, uint32_t id, pressure_type_t type, double pmin, double pmax);
double pressure_from_voltage(const pressure_sensor_t *ps, double Vm, double Vs);
double pressure_from_lsb(const pressure_sensor_t *ps, uint32_t raw, uint32_t fs);

void humidity_sensor_init(humidity_sensor_t *hs, uint32_t id, const char *pn, double acc);
double humidity_dew_point_c(double temp_c, double rh);
double humidity_absolute_humidity_g_per_m3(double temp_c, double rh);
double humidity_vapor_pressure_kpa(double temp_c);

void ultrasonic_sensor_init(ultrasonic_sensor_t *us, uint32_t id, double rmin, double rmax);
double ultrasonic_distance_from_time(double tof_us, double temp_c);
double ultrasonic_speed_of_sound(double temp_c);
double ultrasonic_max_range_from_attenuation(double freq_khz, double rh);

void gas_sensor_init(gas_sensor_t *gs, uint32_t id, const char *pn, gas_sensor_type_t type, const char *target);
double gas_concentration_from_voltage(const gas_sensor_t *gs, double Vm, double Vs, double Tc, double rh);
double gas_sensor_ratio_correction(double Rs_R0_ratio, double Tc, double rh);

void current_sensor_init(current_sensor_t *cs, uint32_t id, current_sensor_type_t type, double Imax);
double current_from_voltage(const current_sensor_t *cs, double Vm);
double shunt_power_dissipation(double I_a, double R_shunt);

void transfer_function_init_linear(transfer_function_t *tf, double m, double b);
void transfer_function_init_polynomial(transfer_function_t *tf, const double *coeffs, uint8_t order);
double transfer_function_forward(const transfer_function_t *tf, double in);
double transfer_function_inverse(const transfer_function_t *tf, double out);
double transfer_function_sensitivity(const transfer_function_t *tf, double in);
int transfer_function_calibrate_linear(transfer_function_t *tf, const double *ins, const double *outs, size_t n);

#endif /* SENSOR_TYPES_H */
