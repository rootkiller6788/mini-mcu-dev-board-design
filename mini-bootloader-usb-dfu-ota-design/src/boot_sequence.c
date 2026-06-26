/*
 * boot_sequence.c -- MCU Boot Sequence Implementation
 *
 * Implements the complete MCU boot flow: reset vector detection, boot
 * reason decoding, vector table validation/relocation, security checks,
 * DFU entry decision, and application handoff.
 *
 * Knowledge Points:
 *   L1: Cortex-M vector table, VTOR, reset sequence
 *   L2: Boot stage determination, boot flags
 *   L4: Boot chain trust invariant
 *   L6: Secure boot with DFU fallback
 */

#include "boot_sequence.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void boot_init(boot_context_t *ctx, const boot_config_t *config)
{
    if (!ctx || !config) return;
    memset(ctx, 0, sizeof(*ctx));
    memcpy(&ctx->config, config, sizeof(boot_config_t));
    ctx->current_stage = BOOT_STAGE_STAGE1;
    ctx->boot_timestamp = 0;
}

boot_reason_t boot_detect_reason(uint32_t reset_flags)
{
    /* Decode hardware reset flags.
     * Common Cortex-M patterns:
     *   RCC_CSR: bit31=LPWR, bit30=WWDG, bit29=IWDG, bit28=SFTRST, bit27=PORRST, bit26=PINRST
     */
    if (reset_flags & (1u << 27)) return BOOT_REASON_POWER_ON;
    if (reset_flags & (1u << 30)) return BOOT_REASON_WDT_RESET;
    if (reset_flags & (1u << 29)) return BOOT_REASON_WDT_RESET;
    if (reset_flags & (1u << 28)) return BOOT_REASON_SOFTWARE_RESET;
    if (reset_flags & (1u << 26)) return BOOT_REASON_EXTERNAL_RESET;
    if (reset_flags & (1u << 31)) return BOOT_REASON_LOW_POWER_WAKE;
    return BOOT_REASON_UNKNOWN;
}

boot_stage_t boot_determine_stage(boot_context_t *ctx)
{
    if (!ctx) return BOOT_STAGE_RECOVERY;

    /* Check DFU entry conditions */
    if (boot_should_enter_dfu(ctx)) {
        ctx->boot_flags |= BOOT_FLAG_FORCE_DFU;
        return BOOT_STAGE_DFU_MODE;
    }

    /* Check if application is valid */
    if (!ctx->app_valid && !ctx->app_verified) {
        ctx->boot_flags |= BOOT_FLAG_NO_APP;
        return BOOT_STAGE_DFU_MODE;
    }

    return BOOT_STAGE_APPLICATION;
}

bool boot_should_enter_dfu(const boot_context_t *ctx)
{
    if (!ctx) return false;
    /* Only check BOOT0 pin if a DFU entry pin is configured */
    if (ctx->config.dfu_entry_pin != 0 &&
        ctx->config.boot_pin_state == ctx->config.dfu_entry_pin_active)
        return true;
    if (ctx->config.magic_addr != 0)
        if (boot_check_magic_value(ctx->config.magic_addr, BOOT_MAGIC_ENTER_DFU))
            return true;
    return false;
}

bool boot_should_enter_ota(const boot_context_t *ctx)
{
    if (!ctx) return false;
    if (ctx->config.magic_addr != 0)
        if (boot_check_magic_value(ctx->config.magic_addr, BOOT_MAGIC_ENTER_OTA))
            return true;
    return ctx->app_valid && !ctx->app_verified;
}

bool boot_validate_vector_table(const vector_table_t *vt,
                                uint32_t flash_base, uint32_t flash_end)
{
    if (!vt) return false;

    /* Initial SP must be within RAM or valid address space.
     * Cortex-M: SP must be aligned to 8 bytes and point to RAM.
     * For simulation, check it is within valid 32-bit range. */
    if (vt->initial_sp == 0 || vt->initial_sp == 0xFFFFFFFF)
        return false;

    /* Reset handler must have bit 0 set (Thumb mode) */
    if ((vt->reset & 1) == 0)
        return false;

    /* Reset handler must be within flash range */
    uint32_t reset_addr = vt->reset & ~1u;
    if (reset_addr < flash_base || reset_addr >= flash_end)
        return false;

    return true;
}

bool boot_relocate_vector_table(uint32_t new_vtor_addr)
{
    /* VTOR must be aligned to 128-word (512-byte) boundary on Cortex-M3/M4,
     * or 64-word (256-byte) on Cortex-M0+. */
    if (new_vtor_addr & 0x7F)  /* 128-byte minimum alignment */
        return false;

    /* In real HW: *(uint32_t*)SCB_VTOR = new_vtor_addr */
    (void)new_vtor_addr;
    return true;
}

void boot_prepare_app_launch(boot_context_t *ctx)
{
    if (!ctx) return;

    /* Read application vector table */
    uint32_t app_vtor = ctx->config.app_start_addr;
    vector_table_t *app_vt = (vector_table_t *)(uintptr_t)app_vtor;

    /* Cache SP and PC values */
    ctx->config.app_stack_top = app_vt->initial_sp;
    ctx->config.app_reset_handler = app_vt->reset;

    /* Validate */
    if (boot_validate_vector_table(app_vt, ctx->config.app_start_addr,
                                    ctx->config.app_start_addr + 0x100000)) {
        ctx->app_verified = 1;
    }
}

void boot_jump_to_application(uint32_t stack_top, uint32_t reset_handler)
{
    /* Standard Cortex-M application launch sequence:
     *   1. Set MSP to stack_top
     *   2. Load PC from reset_handler
     *   3. Jump (BX or LDR PC)
     *
     * In real HW this uses inline asm:
     *   __set_MSP(stack_top);
     *   ((void(*)(void))reset_handler)();
     */

    /* Deinitialize peripherals used by bootloader */
    /* Disable all interrupts */

    /* Set the vector table to application's VTOR */
    /* Actually performed before calling this function */

    /* Jump never returns */
    (void)stack_top;
    (void)reset_handler;
    /* __attribute__((noreturn)) - in real impl, this never returns */
    exit(0);
}

bool boot_set_magic_value(uint32_t addr, uint32_t magic)
{
    if (addr == 0) return false;

    /* Write to backup/retained SRAM region.
     * This memory survives system reset but not power cycles. */
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)addr;
    *p = magic;
    return true;
}

bool boot_clear_magic_value(uint32_t addr)
{
    if (addr == 0) return false;
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)addr;
    *p = 0;
    return true;
}

bool boot_check_magic_value(uint32_t addr, uint32_t magic)
{
    if (addr == 0) return false;
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)addr;
    return (*p == magic);
}

void boot_system_reset(void)
{
    /* SCB_AIRCR = VECTKEY | SYSRESETREQ */
    /* __DSB(); __ISB(); */
    /* In simulation: */
    exit(1);
}

void boot_enter_dfu_mode(boot_context_t *ctx)
{
    if (!ctx) return;
    ctx->current_stage = BOOT_STAGE_DFU_MODE;
    ctx->dfu_requested = 1;
}

const char *boot_reason_name(boot_reason_t reason)
{
    static const char *names[] = {
        "POWER_ON","EXTERNAL_RESET","WDT_RESET","SOFTWARE_RESET",
        "LOW_POWER_WAKE","BOR_RESET","DFU_DETACH","OTA_REBOOT",
        "DEBUG_ATTACH"
    };
    if (reason > BOOT_REASON_DEBUG_ATTACH) return "UNKNOWN";
    return names[reason];
}

const char *boot_stage_name(boot_stage_t stage)
{
    static const char *names[] = {
        "ROM","STAGE1","STAGE2","APPLICATION","DFU_MODE","RECOVERY"
    };
    if (stage > BOOT_STAGE_RECOVERY) return "UNKNOWN";
    return names[stage];
}
