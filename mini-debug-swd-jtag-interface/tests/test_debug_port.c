/**
 * test_debug_port.c - Debug Port Unit Tests
 * Tests: CSW configuration, power-up, alignment, breakpoints.
 */
#include "../include/debug_port.h"
#include <stdio.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define T(n) do { printf("  TEST: %s ... ", n); } while(0)
#define P() do { printf("PASS\n"); passed++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); failed++; } while(0)
#define C(c,m) do { if (c) P(); else F(m); } while(0)

static void test_csw_config(void) {
    T("CSW 32-bit + auto-inc");
    uint32_t csw = mem_ap_setup_csw(32, true, false);
    C((csw & AP_CSW_SIZE_MASK) == AP_CSW_SIZE_32BIT, "size not 32-bit");
    C((csw & AP_CSW_ADDRINC_MASK) == AP_CSW_ADDRINC_SINGLE, "not auto-inc");
    C((csw & AP_CSW_MASTER_DEBUG) != 0, "not debug master");

    T("CSW 8-bit + no auto-inc");
    csw = mem_ap_setup_csw(8, false, true);
    C((csw & AP_CSW_SIZE_MASK) == AP_CSW_SIZE_8BIT, "size not 8-bit");
    C((csw & AP_CSW_ADDRINC_MASK) == AP_CSW_ADDRINC_OFF, "should be off");
    C((csw & AP_CSW_SPIDEN) != 0, "secure not set");
}

static void test_power_up(void) {
    T("Power up request");
    uint32_t req = dap_power_up_request();
    C((req & DP_CTRL_STAT_CDBGPWRUPREQ) != 0, "CDBGPWRUPREQ not set");
    C((req & DP_CTRL_STAT_CSYSPWRUPREQ) != 0, "CSYSPWRUPREQ not set");
}

static void test_clear_errors(void) {
    T("Clear errors");
    uint32_t clear = dap_clear_errors();
    C((clear & (1u << 0)) != 0, "DAPABORT not set");
    C((clear & (1u << 4)) != 0, "ORUNERRCLR not set");
}

static void test_alignment(void) {
    T("Word alignment check");
    C(check_word_aligned(0x20000000) == true, "0x20000000 should be aligned");
    C(check_word_aligned(0x20000001) == false, "0x20000001 not aligned");
    C(check_halfword_aligned(0x20000002) == true, "0x20000002 halfword aligned");

    T("Address alignment helpers");
    C(align_address_down(0x20000003, 4) == 0x20000000, "align down wrong");
    C(align_address_up(0x20000001, 4) == 0x20000004, "align up wrong");
}

static void test_dap_decode(void) {
    T("DAP DPIDR decode");
    uint32_t dpidr = (0x2 << 16) | (0x3 << 28) | (0x23B << 6) | 0x01;
    uint32_t ver, rev, des;
    dap_decode_dpidr(dpidr, &ver, &rev, &des);
    C(ver == 0x2, "version wrong");
    C(rev == 0x3, "revision wrong");
}

int main(void) {
    printf("Debug Port Unit Tests\n");
    printf("=====================\n\n");
    test_csw_config();
    test_power_up();
    test_clear_errors();
    test_alignment();
    test_dap_decode();
    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
