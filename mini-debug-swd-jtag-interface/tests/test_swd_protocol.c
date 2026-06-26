/**
 * test_swd_protocol.c - SWD Protocol Unit Tests
 * Tests: parity computation, request byte building,
 * ACK parsing, error handling, timing calculations.
 */
#include "../include/swd_protocol.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

static void test_parity_odd(void) {
    TEST("swd_compute_parity odd property");
    /* For each 4-bit input, parity must make total 1-bits odd */
    uint8_t ap, rn, a2, a3, p, total;
    int valid = 1;
    for (ap = 0; ap < 2 && valid; ap++) {
    for (rn = 0; rn < 2 && valid; rn++) {
    for (a2 = 0; a2 < 2 && valid; a2++) {
    for (a3 = 0; a3 < 2 && valid; a3++) {
        p = swd_compute_parity(ap, rn, a2, a3);
        total = (ap & 1) + (rn & 1) + (a2 & 1) + (a3 & 1) + (p & 1);
        if (total % 2 != 1) valid = 0;
    }}}}
    CHECK(valid, "parity not odd for all inputs");
}

static void test_parity_32(void) {
    TEST("swd_compute_parity_32 known values");
    CHECK(swd_compute_parity_32(0x00000000) == 0, "parity(0) != 0");
    CHECK(swd_compute_parity_32(0x00000001) == 1, "parity(1) != 1");
    CHECK(swd_compute_parity_32(0xFFFFFFFF) == 0, "parity(all-ones) != 0");
    CHECK(swd_compute_parity_32(0xAAAAAAAA) == 0, "parity(0xAA...) != 0");
}

static void test_verify_parity_32(void) {
    TEST("swd_verify_parity_32");
    CHECK(swd_verify_parity_32(0x00000000, 0) == true,
          "even ones + parity 0 should be even total");
    CHECK(swd_verify_parity_32(0x00000001, 1) == true,
          "odd ones + parity 1 should be even total");
    CHECK(swd_verify_parity_32(0x00000001, 0) == false,
          "odd ones + parity 0 should be odd total");
    CHECK(swd_verify_parity_32(0x0, 1) == false,
          "even ones + parity 1 should be odd total");
}

static void test_build_request_byte(void) {
    TEST("swd_build_request_byte");
    uint8_t req = swd_build_request_byte(0, 0, 0);
    /* DP write to DPIDR: Start=1, APnDP=0, RnW=0, A=00, Parity=? */
    CHECK((req & SWD_START_BIT_MASK) != 0, "start bit missing");
    CHECK((req & SWD_STOP_BIT_MASK) == 0, "stop bit not 0");
    CHECK((req & SWD_PARK_BIT_MASK) != 0, "park bit missing");
    /* AP read from DRW: APnDP=1, RnW=1, A[3:2]=11 */
    req = swd_build_request_byte(1, 1, 3);
    CHECK((req & SWD_APNDP_BIT_MASK) != 0, "APnDP not set");
    CHECK((req & SWD_RNW_BIT_MASK) != 0, "RnW not set");
}

static void test_parse_ack(void) {
    TEST("swd_parse_ack");
    CHECK(swd_parse_ack(0x01) == SWD_ACK_OK, "ACK 001 not OK");
    CHECK(swd_parse_ack(0x02) == SWD_ACK_WAIT, "ACK 010 not WAIT");
    CHECK(swd_parse_ack(0x04) == SWD_ACK_FAULT, "ACK 100 not FAULT");
    CHECK(swd_parse_ack(0x00) == SWD_ACK_INVALID, "ACK 000 not INVALID");
    CHECK(swd_parse_ack(0x07) == SWD_ACK_INVALID, "ACK 111 not INVALID");
}

static void test_overrun_detection(void) {
    TEST("swd_check_overrun");
    CHECK(swd_check_overrun(DP_CTRL_STAT_ORUNDETECT) == true,
          "ORUNDETECT not detected");
    CHECK(swd_check_overrun(0) == false, "false overrun");
}

static void test_sticky_error(void) {
    TEST("swd_check_sticky_error");
    uint32_t err = DP_CTRL_STAT_STICKYORUN |
                   DP_CTRL_STAT_STICKYCMP |
                   DP_CTRL_STAT_STICKYERR;
    CHECK(swd_check_sticky_error(err) == true, "sticky errors not detected");
    CHECK(swd_check_sticky_error(0) == false, "false sticky error");
}

static void test_timing_calculations(void) {
    TEST("swd timing calculations");
    double period = swd_calculate_swclk_period_ns(10e6);
    CHECK(fabs(period - 100.0) < 0.01, "10MHz period != 100ns");
    period = swd_calculate_swclk_period_ns(0);
    CHECK(period == 0.0, "zero freq should give zero period");
}

static void test_dpidr_verification(void) {
    TEST("swd_verify_dpidr");
    uint32_t valid_dpidr = (DP_DPIDR_VERSION_ADIv5 << 16) |
                            (DP_DPIDR_DESIGNER_ARM << 6) | 0x01;
    CHECK(swd_verify_dpidr(valid_dpidr) == true, "valid DPIDR rejected");
    uint32_t invalid_dpidr = 0x00000000;
    CHECK(swd_verify_dpidr(invalid_dpidr) == false, "invalid DPIDR accepted");
}

static void test_error_strings(void) {
    TEST("swd_error_string");
    CHECK(swd_error_string(SWD_ERR_NONE) != NULL, "NULL error string");
    CHECK(swd_error_string(SWD_ERR_NO_TARGET) != NULL, "NULL error string");
    CHECK(swd_error_string(SWD_ERR_ACK_FAULT) != NULL, "NULL error string");
}

int main(void) {
    printf("SWD Protocol Unit Tests\n");
    printf("======================\n\n");

    test_parity_odd();
    test_parity_32();
    test_verify_parity_32();
    test_build_request_byte();
    test_parse_ack();
    test_overrun_detection();
    test_sticky_error();
    test_timing_calculations();
    test_dpidr_verification();
    test_error_strings();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
