/**
 * @example board_validation.c
 * @brief Complete board validation example.
 *
 * Knowledge: L6 Canonical Problem — Pre-production design review
 *
 * Runs all design rule checks on a hypothetical STM32 board
 * and generates a validation report suitable for design review.
 */
#include "stm32_minimal_config.h"
#include "power_system.h"
#include "clock_system.h"
#include "pcb_layout.h"
#include "reset_boot.h"
#include "thermal.h"
#include "board_validation.h"
#include <stdio.h>

int main(void) {
    printf("=== Board Design Validation ===\n\n");

    /* Configure three different board designs and validate each */
    BoardConfig configs[3];
    const char *names[3] = {"Blue Pill (F103)", "Black F407", "H743 High-Perf"};

    stm32f103c8_bluepill_config(&configs[0]);
    stm32f407vet6_black_config(&configs[1]);
    stm32h743vit6_config(&configs[2]);

    for (int b = 0; b < 3; b++) {
        BoardConfig *cfg = &configs[b];
        printf("Board: %s\n", names[b]);
        printf("  MCU: series %d, %d pins, %.0f MHz\n",
               cfg->series, cfg->pin_count, cfg->core_max_freq_hz / 1e6);
        printf("  Flash: %d KB, SRAM: %d KB\n",
               cfg->flash_size_kb, cfg->sram_size_kb);

        /* Validate power */
        int vdd_ok = validate_power_spec(&cfg->power_specs[0], cfg->series);
        printf("  VDD: %s\n", vdd_ok ? "PASS" : "FAIL");

        /* Estimate power */
        double power = estimate_mcu_power(3.3, cfg->core_max_freq_hz, 5);
        printf("  Power: %.0f mW\n", power * 1000);

        /* Check BOR threshold */
        double bor = recommend_bor_threshold(3.3, 3.0, cfg->series);
        printf("  BOR threshold: %.1f V\n", bor);

        /* PCB area estimate */
        double area = compute_pcb_area_estimate(cfg);
        printf("  Min PCB area: %.0f mm2\n", area);

        /* Validate board config */
        int valid = validate_board_config(cfg);
        printf("  Overall: %s\n\n", valid ? "VALID" : "INVALID");
    }

    /* Power integrity score example */
    printf("=== Power Integrity Score ===\n");
    int score = check_power_integrity(3.25, 3.30, 30, 50, 15, 10);
    printf("Score: %d/100 (VDD=3.25V nom=3.3V, ripple=30mV)\n", score);

    /* Signal integrity check */
    printf("\n=== Signal Integrity Check ===\n");
    int si = check_signal_integrity(8, 40, 4e6, 3.5);
    printf("Signal integrity: %s\n", si ? "OK" : "ISSUES FOUND");

    printf("\n=== Validation Complete ===\n");
    return 0;
}
