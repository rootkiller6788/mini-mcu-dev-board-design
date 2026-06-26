/**
 * @file low_power_mcu.h
 * @brief Low-power MCU operating modes, sleep states, and power profiling
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L3 Math Structures, L5 Algorithms
 *
 * References:
 * - STM32L4xx Reference Manual (RM0351), STMicroelectronics, 2018
 * - nRF52840 Product Specification v1.1, Nordic Semiconductor, 2019
 * - MSP430FR59xx User's Guide (SLAU367), Texas Instruments, 2018
 * - Valvano, "Embedded Systems: Real-Time Operating Systems for ARM Cortex-M", 2019
 *
 * Course Mapping:
 * - MIT 6.003 -> Signal sampling, timer interrupts, discrete-time control
 * - Berkeley EE16B -> Power-aware embedded system design
 * - Illinois ECE 310 -> Discrete-time processing on low-power platforms
 */

#ifndef LOW_POWER_MCU_H
#define LOW_POWER_MCU_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * L1 Definitions - MCU power states, sleep modes, clock domains
 * ============================================================================ */

/**
 * @brief MCU power operating modes
 *
 * Real-world current draws (example: STM32L4 @ 3.0V):
 *   RUN:      Cortex-M4 @ 80MHz, all peripherals on     (~10 mA)
 *   SLEEP:    CPU off, peripherals on, any IRQ wakes     (~3 mA)
 *   STOP:     HSI/HSE off, LSI/LSE optional, EXTI wakes  (~50 uA)
 *   STANDBY:  Vcore off, backup domain retained          (~2 uA)
 *   SHUTDOWN: All off except wakeup pins, POR/BOR reset  (~30 nA)
 */
typedef enum {
    MCU_MODE_RUN      = 0,
    MCU_MODE_SLEEP    = 1,
    MCU_MODE_STOP     = 2,
    MCU_MODE_STANDBY  = 3,
    MCU_MODE_SHUTDOWN = 4,
    MCU_MODE_COUNT    = 5
} McuPowerMode;

/**
 * @brief Clock domains available for independent gating
 *
 * Each domain can be independently gated to save power.
 * Gating saves the dynamic power (CV^2*f) of that clock tree.
 */
typedef enum {
    CLOCK_HSE      = (1 << 0),   /* High-Speed External (crystal/oscillator) */
    CLOCK_HSI      = (1 << 1),   /* High-Speed Internal (RC oscillator) */
    CLOCK_PLL      = (1 << 2),   /* Phase-Locked Loop multiplier */
    CLOCK_AHB      = (1 << 3),   /* AHB bus clock (CPU, DMA, memory) */
    CLOCK_APB1     = (1 << 4),   /* APB1 peripheral bus */
    CLOCK_APB2     = (1 << 5),   /* APB2 peripheral bus */
    CLOCK_LSE      = (1 << 6),   /* Low-Speed External (32.768kHz crystal) */
    CLOCK_LSI      = (1 << 7),   /* Low-Speed Internal (~32kHz RC) */
    CLOCK_ADC      = (1 << 8),   /* ADC clock domain */
    CLOCK_USART    = (1 << 9),   /* USART peripheral clock */
    CLOCK_SPI      = (1 << 10),  /* SPI peripheral clock */
    CLOCK_I2C      = (1 << 11),  /* I2C peripheral clock */
    CLOCK_TIMER    = (1 << 12),  /* Timer peripheral clock */
    CLOCK_GPIO     = (1 << 13),  /* GPIO port clock */
    CLOCK_DMA      = (1 << 14),  /* DMA controller clock */
    CLOCK_RTC      = (1 << 15),  /* Real-Time Clock domain (backup) */
} ClockDomain;

/**
 * @brief Wake-up sources from low-power modes
 *
 * Different MCU families support different wake-up sources
 * in different sleep depths. STOP mode typically supports
 * the widest variety of wake-up sources.
 */
typedef enum {
    WAKEUP_NONE          = 0,
    WAKEUP_GPIO_PIN      = (1 << 0),   /* External pin interrupt/event */
    WAKEUP_RTC_ALARM     = (1 << 1),   /* RTC alarm at specified time */
    WAKEUP_RTC_WAKEUP    = (1 << 2),   /* RTC periodic wakeup timer */
    WAKEUP_IWDG          = (1 << 3),   /* Independent watchdog reset */
    WAKEUP_COMPARATOR    = (1 << 4),   /* Analog comparator threshold */
    WAKEUP_USART_RX      = (1 << 5),   /* USART receive activity */
    WAKEUP_I2C_ADDR      = (1 << 6),   /* I2C address match */
    WAKEUP_LPUART_RX     = (1 << 7),   /* Low-power UART receive */
    WAKEUP_TOUCH_SENSE   = (1 << 8),   /* Touch sensing controller */
    WAKEUP_ADC_WDG       = (1 << 9),   /* ADC analog watchdog threshold */
    WAKEUP_BLE_RADIO     = (1 << 10),  /* BLE radio event (nRF52 series) */
    WAKEUP_NFC_FIELD     = (1 << 11),  /* NFC field detect */
} WakeupSource;

/* ============================================================================
 * L2 Core Concepts - Power profiles, duty cycling, energy accounting
 * ============================================================================ */

/**
 * @brief Power consumption parameters for an MCU operating mode
 */
typedef struct {
    double    I_supply_uA;         /* Supply current in this mode (uA) */
    double    V_core_V;            /* Core voltage in this mode (V) */
    double    f_cpu_MHz;           /* CPU clock frequency (0 if stopped) */
    double    wakeup_latency_us;   /* Time from trigger to first instruction */
    double    entry_latency_us;    /* Time to fully enter this mode */
} McuModeProfile;

/**
 * @brief Complete MCU power profile across all modes
 */
typedef struct {
    McuModeProfile modes[MCU_MODE_COUNT];
    double  V_supply_V;
    double  I_leakage_uA;
    double  transition_energy_uJ;
    uint32_t clock_gating_mask;
} McuPowerProfile;

/**
 * @brief A single phase in a duty-cycled operation cycle
 *
 * Example: BLE beacon cycle -
 *   Phase 0: RUN (wake, sample sensor)  - 2ms @ 10mA
 *   Phase 1: RUN (radio TX @ 0dBm)      - 0.5ms @ 8mA
 *   Phase 2: RUN (radio RX window)      - 0.3ms @ 8mA
 *   Phase 3: STOP (deep sleep)          - 997.2ms @ 2uA
 *
 * Total period = 1000ms, avg current = sum(phase_i * t_i) / T
 */
typedef struct {
    McuPowerMode mode;
    double       duration_ms;
    double       I_extra_uA;
    const char  *description;
} DutyCyclePhase;

/**
 * @brief Complete duty cycle definition for periodic operation
 */
typedef struct {
    DutyCyclePhase *phases;
    size_t          num_phases;
    double          period_ms;
    const char     *name;
} DutyCycle;

/* ============================================================================
 * L3 Math Structures - Power state machines, energy modeling
 * ============================================================================ */

/**
 * @brief State transition in the MCU power state machine
 */
typedef struct {
    McuPowerMode from_mode;
    McuPowerMode to_mode;
    double       energy_cost_uJ;
    double       latency_us;
} PowerStateTransition;

/**
 * @brief Finite state machine for MCU power management
 */
typedef struct {
    McuPowerMode           current_mode;
    PowerStateTransition  *transitions;
    size_t                 num_transitions;
    double                 uptime_ms;
    double                 energy_used_uJ;
    uint32_t               transition_count;
} PowerStateMachine;

/**
 * @brief Energy profile of a timed operation
 */
typedef struct {
    double   E_active_uJ;
    double   E_sleep_uJ;
    double   E_transition_uJ;
    double   duty_cycle_pct;
    double   avg_current_uA;
    double   observation_ms;
} OperationEnergy;

/* ============================================================================
 * L5 Algorithms - Sleep scheduling, timer calibration, energy optimization
 * ============================================================================ */

/**
 * @brief Low-power timer calibration data
 *
 * RC oscillators drift with temperature and voltage.
 * Calibration against a known reference (crystal or GPS) improves accuracy.
 */
typedef struct {
    double   nominal_freq_Hz;
    double   calibrated_freq_Hz;
    double   drift_ppm;
    double   temperature_C;
    double   voltage_V;
} TimerCalibration;

/**
 * @brief RTC-based wakeup scheduler for periodic operations
 *
 * Schedules MCU wakeups at precise intervals using the low-power RTC.
 * Compensates for crystal drift and temperature effects.
 */
typedef struct {
    double   wakeup_interval_ms;
    double   next_wakeup_ms;
    double   drift_compensation;
    int      crystal_aging_enabled;
    uint32_t missed_wakeups;
    TimerCalibration rtc_cal;
} WakeupScheduler;

/**
 * @brief Dynamic voltage and frequency scaling parameters
 */
typedef struct {
    double   V_core_V;
    double   f_cpu_MHz;
    double   I_run_uA;
    double   max_f_MHz;
    int      performance_MIPS;
} DVFSOperatingPoint;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* --- Mode Profile Lookup --- */
const McuPowerProfile* mcu_get_power_profile(const char *mcu_family, double V_supply);
const char* mcu_mode_get_name(McuPowerMode mode);
const char* mcu_mode_get_description(McuPowerMode mode);

/* --- Duty Cycle Analysis --- */
double duty_cycle_avg_current(const DutyCycle *dc, const McuPowerProfile *profile);
void duty_cycle_energy(const DutyCycle *dc, const McuPowerProfile *profile,
                       OperationEnergy *energy);
double duty_cycle_percentage(const DutyCycle *dc);
double duty_cycle_optimal_sleep_ms(const DutyCycle *dc,
                                    const McuPowerProfile *profile,
                                    McuPowerMode sleep_mode);

/* --- Power State Machine --- */
void psm_init(PowerStateMachine *psm, McuPowerMode initial_mode);
int psm_transition(PowerStateMachine *psm, McuPowerMode target_mode);
void psm_add_transition(PowerStateMachine *psm, McuPowerMode from, McuPowerMode to,
                        double energy_cost_uJ, double latency_us);
double psm_get_energy(const PowerStateMachine *psm);
double psm_get_uptime_ms(const PowerStateMachine *psm);

/* --- Clock Gating --- */
double clock_gating_savings_uW(const McuPowerProfile *profile, uint32_t gated_clocks);
double peripheral_current_estimate(const McuPowerProfile *profile,
                                   uint32_t active_clocks);

/* --- Wakeup Scheduling --- */
void wakeup_scheduler_init(WakeupScheduler *sched, double interval_ms,
                           double crystal_ppm);
double wakeup_schedule_next(WakeupScheduler *sched, double current_time_ms);
void wakeup_report_fired(WakeupScheduler *sched, double actual_time_ms,
                         double expected_time_ms);
void wakeup_temp_compensate(WakeupScheduler *sched, double temperature_C);

/* --- DVFS Management --- */
const DVFSOperatingPoint* dvfs_get_points(const char *mcu_family, int *num_points);
int dvfs_select_point(const DVFSOperatingPoint *points, int num_points,
                      double required_mips);

/* --- Utility Functions --- */
double current_to_power_uW(double I_uA, double V_volts);
double energy_per_instruction(const McuPowerProfile *profile,
                              const char *mcu_family);
double sleep_breakeven_us(const McuPowerProfile *profile,
                          McuPowerMode sleep_mode, McuPowerMode run_mode);

#endif /* LOW_POWER_MCU_H */