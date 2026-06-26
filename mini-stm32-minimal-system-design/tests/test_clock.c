/**
 * @file test_clock.c
 * @brief Tests for clock system module.
 */
#include "clock_system.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(n) do { tests_run++; printf("  TEST %s... ", n); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); return; } while(0)
#define ASSERT_TRUE(c,m) do { if(!(c)){FAIL(m);} } while(0)
#define ASSERT_EQ_DBL(a,b,e) do { if(fabs((a)-(b))>(e)){FAIL("value mismatch");return;} } while(0)

static void test_load_capacitors(void) {
    TEST("load capacitors 18pF crystal");
    CrystalSpec xtal = {8e6, 30, 18e-12, 7e-12, 80, 500e-6, 0};
    double cl1, cl2;
    int r = compute_load_capacitors(&xtal, 5e-12, &cl1, &cl2);
    ASSERT_TRUE(r == 0, "Should find valid caps");
    ASSERT_EQ_DBL(cl1, 26e-12, 1e-12);
    PASS();

    TEST("load capacitors stray too high");
    r = compute_load_capacitors(&xtal, 20e-12, &cl1, &cl2);
    ASSERT_TRUE(r == -1, "Should fail when Cstray > CL");
    PASS();
}

static void test_gain_margin(void) {
    TEST("gain margin > 5");
    CrystalSpec xtal = {8e6, 30, 18e-12, 7e-12, 80, 500e-6, 0};
    double gm = compute_gain_margin(&xtal, 5e-3);
    ASSERT_TRUE(gm >= 5.0, "Should have adequate gain margin");
    PASS();
}

static void test_pll_frequencies(void) {
    TEST("PLL F103: 8MHz*9=72MHz");
    double vco, sys, usb;
    int r = compute_pll_frequencies(8e6, 1, 9, 1, 1, &vco, &sys, &usb);
    ASSERT_TRUE(r == 0, "Valid F103 PLL config");
    ASSERT_EQ_DBL(vco, 72e6, 1);
    ASSERT_EQ_DBL(sys, 72e6, 1);
    PASS();

    TEST("PLL F407: 25/25*336/2=168MHz");
    r = compute_pll_frequencies(25e6, 25, 336, 2, 7, &vco, &sys, &usb);
    ASSERT_TRUE(r == 0, "Valid F407 PLL config");
    ASSERT_EQ_DBL(vco, 336e6, 1);
    ASSERT_EQ_DBL(sys, 168e6, 1);
    ASSERT_EQ_DBL(usb, 48e6, 1);
    PASS();
}

static void test_find_pll_config(void) {
    TEST("find PLL config for 72MHz from 8MHz");
    int m, n, p, q;
    int r = find_pll_config(8e6, 72e6, 100e6, 960e6, 0, &m, &n, &p, &q);
    ASSERT_TRUE(r == 0, "Should find valid config");
    PASS();

    TEST("find PLL config for 168MHz from 25MHz with USB");
    r = find_pll_config(25e6, 168e6, 100e6, 960e6, 1, &m, &n, &p, &q);
    ASSERT_TRUE(r == 0, "Should find config with 48MHz USB");
    PASS();
}

int main(void) {
    printf("Running clock system tests...\n");
    test_load_capacitors();
    test_gain_margin();
    test_pll_frequencies();
    test_find_pll_config();
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
