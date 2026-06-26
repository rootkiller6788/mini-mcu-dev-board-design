/*
 * arch_compat.h — Architecture compatibility macros
 *
 * Allows firmware code to compile on x86 host for testing
 * while preserving the correct ARM instructions for MCU.
 *
 * On ARM: uses real barrier/control instructions.
 * On x86: no-ops (tested logic, not timing/ordering).
 */

#ifndef ARCH_COMPAT_H
#define ARCH_COMPAT_H

#if defined(__arm__) || defined(__thumb__) || defined(__ARM_ARCH)

/* Real ARM instructions */
#define DSB()       __asm__ volatile ("dsb sy")
#define ISB()       __asm__ volatile ("isb sy")
#define DMB()       __asm__ volatile ("dmb sy")
#define CPSID_I()   __asm__ volatile ("cpsid i")
#define CPSIE_I()   __asm__ volatile ("cpsie i")
#define WFI()       __asm__ volatile ("wfi")
#define NOP()       __asm__ volatile ("nop")
#define SET_MSP(x)  __asm__ volatile ("msr msp, %0" : : "r" (x))
#define BX(addr)    __asm__ volatile ("bx %0" : : "r" (addr) : "memory")

#else

/* x86 host fallbacks — functionally no-ops for logic testing */
#define DSB()       do { /* no-op: memory barrier not needed on host */ } while(0)
#define ISB()       do { /* no-op */ } while(0)
#define DMB()       do { /* no-op */ } while(0)
#define CPSID_I()   do { /* no-op: no real interrupts to disable */ } while(0)
#define CPSIE_I()   do { /* no-op */ } while(0)
#define WFI()       do { /* no-op: host CPU would actually halt */ } while(0)
#define NOP()       do { /* no-op */ } while(0)
#define SET_MSP(x)  do { (void)(x); /* no-op: MSP not meaningful on host */ } while(0)
#define BX(addr)    do { (void)(addr); /* no-op: jump not safe on host */ } while(0)

#endif

#endif /* ARCH_COMPAT_H */
