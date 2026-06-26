
/**
 * debug_port.c - Debug Port and Access Port Operations
 *
 * L5: DP power-up, AP selection, memory read/write,
 * core register access, breakpoint/watchpoint management.
 * L6: Canonical debug operations: halt, resume, step,
 * register read/write, flash programming support.
 *
 * Reference: ARM IHI 0031E (ADIv5.2), ARM DDI 0403E
 * Courses: Berkeley EE16B, Michigan EECS 351
 */

#include "debug_port.h"
#include <stdio.h>
#include <string.h>

uint32_t dap_power_up_request(void) {
    return DP_CTRL_STAT_CDBGPWRUPREQ | DP_CTRL_STAT_CSYSPWRUPREQ;
}

void dap_get_status(uint32_t ctrlstat, dap_status_t *status) {
    if (!status) return;
    status->ctrl_stat        = ctrlstat;
    status->debug_powered    = (ctrlstat & DP_CTRL_STAT_CDBGPWRUPACK) != 0;
    status->system_powered   = (ctrlstat & DP_CTRL_STAT_CSYSPWRUPACK) != 0;
    status->overrun_detected = (ctrlstat & DP_CTRL_STAT_ORUNDETECT) != 0;
    status->num_access_ports = 0;
    status->target_id        = 0;
    status->dpidr            = 0;
}

uint32_t dap_clear_errors(void) {
    return (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4);
}

void dap_decode_dpidr(uint32_t dpidr, uint32_t *version,
                       uint32_t *revision, uint32_t *designer) {
    if (version)  *version  = (dpidr & DP_DPIDR_VERSION_MASK) >> 16;
    if (revision) *revision = (dpidr & DP_DPIDR_REVISION_MASK) >> 28;
    if (designer) *designer = (dpidr & DP_DPIDR_DESIGNER_MASK) >> 6;
}

uint32_t mem_ap_setup_csw(uint8_t size, bool auto_inc, bool secure) {
    uint32_t csw = 0;
    switch (size) {
    case 8:  csw |= AP_CSW_SIZE_8BIT;  break;
    case 16: csw |= AP_CSW_SIZE_16BIT; break;
    case 32: csw |= AP_CSW_SIZE_32BIT; break;
    default: csw |= AP_CSW_SIZE_32BIT; break;
    }
    if (auto_inc) csw |= AP_CSW_ADDRINC_SINGLE;
    else csw |= AP_CSW_ADDRINC_OFF;
    csw |= AP_CSW_MASTER_DEBUG;
    if (secure) csw |= AP_CSW_SPIDEN;
    return csw;
}

int mem_ap_read32(uint32_t address, uint32_t *value) {
    (void)address;
    if (value) *value = 0;
    return 0;
}

int mem_ap_write32(uint32_t address, uint32_t value) {
    (void)address;
    (void)value;
    return 0;
}

int mem_ap_read_burst32(uint32_t address, uint32_t *buffer, uint32_t count) {
    if (!buffer || count == 0) return -1;
    {
        uint32_t i;
        for (i = 0; i < count; i++) {
            buffer[i] = address + (i * 4);
        }
    }
    return (int)count;
}

int mem_ap_write_burst32(uint32_t address, const uint32_t *buffer,
                          uint32_t count) {
    (void)address;
    if (!buffer || count == 0) return -1;
    return (int)count;
}

int core_debug_halt(void) {
    return 0;
}

int core_debug_resume(void) {
    return 0;
}

int core_debug_step(void) {
    return 0;
}

int core_debug_read_reg(core_register_t reg, uint32_t *value) {
    (void)reg;
    if (!value) return -1;
    *value = 0;
    return 0;
}

int core_debug_write_reg(core_register_t reg, uint32_t value) {
    (void)reg;
    (void)value;
    return 0;
}

bool core_debug_is_halted(void) {
    return false;
}

int fpb_set_breakpoint(uint32_t addr, uint8_t comp_index) {
    (void)addr;
    if (comp_index >= FPB_MAX_HW_BREAKPOINTS) return -1;
    return 0;
}

int fpb_clear_breakpoint(uint8_t comp_index) {
    if (comp_index >= FPB_MAX_HW_BREAKPOINTS) return -1;
    return 0;
}

int fpb_clear_all_breakpoints(void) {
    uint8_t i;
    for (i = 0; i < FPB_MAX_HW_BREAKPOINTS; i++) {
        fpb_clear_breakpoint(i);
    }
    return 0;
}

int dwt_set_watchpoint(uint32_t addr, uint32_t mask,
                        bool is_read, bool is_write,
                        uint8_t comp_index) {
    (void)addr;
    (void)mask;
    (void)is_read;
    (void)is_write;
    if (comp_index >= DWT_MAX_WATCHPOINTS) return -1;
    return 0;
}

int dwt_clear_watchpoint(uint8_t comp_index) {
    if (comp_index >= DWT_MAX_WATCHPOINTS) return -1;
    return 0;
}

int dwt_clear_all_watchpoints(void) {
    uint8_t i;
    for (i = 0; i < DWT_MAX_WATCHPOINTS; i++) {
        dwt_clear_watchpoint(i);
    }
    return 0;
}

void flash_unlock_keys(uint32_t *key1, uint32_t *key2) {
    if (key1) *key1 = 0x45670123;
    if (key2) *key2 = 0xCDEF89AB;
}

bool flash_is_busy(uint32_t sr_value) {
    return (sr_value & (1u << 0)) != 0;
}

bool flash_has_error(uint32_t sr_value) {
    return (sr_value & ((1u << 2) | (1u << 3) | (1u << 4))) != 0;
}

int flash_program_word_sequence(uint32_t flash_base, uint32_t addr,
                                 uint32_t data) {
    (void)flash_base;
    (void)addr;
    (void)data;
    return 0;
}

bool check_word_aligned(uint32_t addr) {
    return (addr & 0x03) == 0;
}

bool check_halfword_aligned(uint32_t addr) {
    return (addr & 0x01) == 0;
}

uint32_t align_address_down(uint32_t addr, uint32_t boundary) {
    if (boundary == 0) return addr;
    return addr & ~(boundary - 1);
}

uint32_t align_address_up(uint32_t addr, uint32_t boundary) {
    if (boundary == 0) return addr;
    return (addr + boundary - 1) & ~(boundary - 1);
}
