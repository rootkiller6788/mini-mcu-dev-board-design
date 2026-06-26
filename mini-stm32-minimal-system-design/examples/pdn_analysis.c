/**
 * @example pdn_analysis.c
 * @brief PDN impedance analysis example.
 *
 * Knowledge: L6 Canonical Problem — Power delivery network design
 *
 * Demonstrates full PDN impedance sweep and anti-resonance detection
 * for a typical STM32 decoupling network.
 */
#include "stm32_minimal_config.h"
#include "decoupling.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("=== PDN Impedance Analysis ===\n\n");

    /* Build a typical decoupling network */
    DecouplingNetwork net;
    net.num_caps = 4;

    /* 10uF bulk (X7R 0805) */
    net.caps[0].c = 10e-6; net.caps[0].esr = 0.010; net.caps[0].esl = 1.0e-9;
    /* 1uF mid-frequency (X7R 0603) */
    net.caps[1].c = 1e-6;  net.caps[1].esr = 0.005; net.caps[1].esl = 0.5e-9;
    /* 100nF mid (X7R 0603) */
    net.caps[2].c = 0.1e-6; net.caps[2].esr = 0.003; net.caps[2].esl = 0.4e-9;
    /* 10nF HF (NP0 0402) */
    net.caps[3].c = 0.01e-6;net.caps[3].esr = 0.002; net.caps[3].esl = 0.3e-9;

    net.total_c = 0; net.total_esr = 0; net.total_esl = 0;

    /* Compute target impedance */
    double z_target = compute_target_impedance(3.3, 5, 0.1);
    printf("Target impedance: %.3f ohm\n", z_target);

    /* Frequency sweep */
    PDNImpedanceProfile profile;
    pdn_impedance_sweep(&net, 1e3, 1e9, 100, &profile);

    printf("\nImpedance profile (%d points):\n", profile.num_points);
    printf("%-12s %-12s %-8s\n", "Freq", "|Z|", "Type");
    for (int i = 0; i < profile.num_points; i += 10) {
        const char *type = "R";
        if (profile.points[i].is_capacitive) type = "C";
        if (profile.points[i].is_inductive)  type = "L";
        printf("%10.0f Hz %8.3f ohm   %s\n",
               profile.points[i].frequency_hz,
               profile.points[i].impedance_ohm,
               type);
    }

    /* Find anti-resonance peaks */
    double peaks[10];
    int n_peaks = find_anti_resonance_peaks(&profile, z_target, peaks, 10);
    printf("\nAnti-resonance peaks above target (%d found):\n", n_peaks);
    for (int i = 0; i < n_peaks; i++) {
        printf("  Peak at %.1f MHz\n", peaks[i] / 1e6);
    }

    /* Capacitor count for target Z */
    DecouplingCap cap_100nf = {0.1e-6, 0.003, 0.4e-9, 16, 603};
    int n_needed = min_caps_for_impedance(&cap_100nf, z_target, 0.7);
    printf("\n100nF caps needed for %.3f ohm: %d\n", z_target, n_needed);

    printf("\nPeak impedance: %.3f ohm at %.1f MHz\n",
           profile.peak_impedance, profile.peak_impedance_freq / 1e6);
    printf("Status: %s\n",
           profile.peak_impedance <= z_target ? "PASS" : "FAIL - need more caps");

    return 0;
}
