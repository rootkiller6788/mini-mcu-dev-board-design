/**
 * @file clock_system.h
 * @brief STM32 clock system design.
 * Knowledge Level: L1 (Definitions), L2 (Core Concepts), L3 (Math), L4 (Laws)
 * Reference: AN2867 (oscillator), AN1709 (PLL)
 * Course mapping: Berkeley EE117, MIT 6.003
 */
#ifndef CLOCK_SYSTEM_H
#define CLOCK_SYSTEM_H
#include "stm32_minimal_config.h"
#include <math.h>

typedef enum {
    CLK_NODE_SOURCE = 0, CLK_NODE_PLL_INPUT, CLK_NODE_PLL_VCO,
    CLK_NODE_PLL_OUTPUT, CLK_NODE_SYSCLK, CLK_NODE_HCLK,
    CLK_NODE_PCLK1, CLK_NODE_PCLK2, CLK_NODE_USBCLK, CLK_NODE_COUNT
} ClockNodeType;

typedef struct {
    double hsi_freq, hse_freq, sysclk, hclk, pclk1, pclk2, usbclk;
    int hse_bypass, pll_enabled, pll_m, pll_n, pll_p, pll_q;
    ClockSource pll_src;
    int ahb_prescaler, apb1_prescaler, apb2_prescaler;
} ClockTree;

typedef enum { OSC_TOPOLOGY_PIERCE = 0, OSC_TOPOLOGY_COLPITTS, OSC_TOPOLOGY_HARTLEY } OscillatorTopology;

typedef struct {
    OscillatorTopology topology;
    double external_resistor, cl1, cl2, cstray, gain_margin, drive_level, startup_time_ms;
    int reliable;
} OscillatorDesign;

/**
 * Compute load capacitor values for Pierce oscillator.
 * L3: CL = (CL1*CL2)/(CL1+CL2) + C_stray. Symmetric: CL1=CL2=2*(CL-C_stray)
 */
int compute_load_capacitors(const CrystalSpec *crystal, double c_stray,
                            double *cl1_out, double *cl2_out);

/**
 * Estimate oscillator gain margin (Barkhausen criterion).
 * L4: gm_crit = 4*ESR*(2*pi*f)^2*(C0+CL)^2. Margin = gm_osc/gm_crit, need >5.
 * Reference: ST AN2867 Section 3.3
 */
double compute_gain_margin(const CrystalSpec *crystal, double gm_osc);

/**
 * Estimate crystal drive level. L4: DL = ESR * (I_RMS)^2
 * I_RMS = (2*pi*f*Vpp*CL)/(2*sqrt(2)). Must not exceed crystal rating.
 */
double compute_drive_level(const CrystalSpec *crystal, double vpp_across);

/**
 * Compute Rext to limit drive level. Rext = 1/(2*pi*f*CL).
 * Reference: ST AN2867 Section 3.5
 */
double compute_rext_limit(const CrystalSpec *crystal, double target_drive_w);

/**
 * Estimate startup time. L4: t_startup = (2*Q_L)/(pi*f*gain_margin).
 * Q_L = 1/(2*pi*f*CL*ESR). Reference: ST AN2867 Section 4
 */
double estimate_startup_time(const CrystalSpec *crystal, double gain_margin);

/**
 * Compute PLL frequencies. VCO_in=F_input/PLLM, VCO_out=VCO_in*PLLN,
 * PLL_out=VCO_out/PLLP, USB=VCO_out/PLLQ (must be 48MHz).
 */
int compute_pll_frequencies(double input_freq_hz, int pll_m, int pll_n,
                            int pll_p, int pll_q,
                            double *vco_out, double *sys_clk, double *usb_clk);

/**
 * Find valid PLL config for target SYSCLK. L5: search over M,N,P,Q.
 */
int find_pll_config(double input_freq_hz, double target_sysclk,
                    double vco_min, double vco_max, int need_usb_48mhz,
                    int *pll_m_out, int *pll_n_out,
                    int *pll_p_out, int *pll_q_out);

/**
 * Validate clock tree against STM32 reference manual constraints.
 */
int validate_clock_tree(const ClockTree *tree, STM32Series series);

#endif /* CLOCK_SYSTEM_H */

double compute_pll_jitter_estimate(double vco_freq, double loop_bandwidth_hz,
                                    double phase_noise_floor_dbc);
int check_pll_lock_range(double vco_freq, double vco_min, double vco_max);
double compute_hse_bypass_resistor(double oscillator_vpp, double mcu_vih_min);
double compute_lse_sensitivity(double esr, double cl, double freq);
int compute_pll_divider_prescaler_chain(double target_freq, double input_freq,
                                         int *prescaler_out);
