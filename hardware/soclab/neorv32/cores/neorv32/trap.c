// trap.c - trap entry and interrupt dispatch, in DMEM
//
// mtvec points to __arduino_trap_entry (DMEM). The entry saves the caller-saved
// registers and mepc, the dispatcher calls the handler of the interrupt from a
// table in DMEM. Only traps without a handler (exceptions, unexpected
// interrupts) go to code in flash: the NEORV32 RTE debug handler prints its
// "<NEORV32-RTE>" message, an unexpected interrupt source is disabled, an
// exception returns behind the faulting instruction (as the RTE does).
// Interrupt path: entry + dispatcher + handler, no XIP cache refill as long as
// the handler is in DMEM. Handlers run with mstatus.MIE = 0 (no nesting).
#include "Arduino.h"
#include "arduino_internal.h"

// Frame: ra t0-t2 a0-a7 t3-t6 (16 words), mepc, 3 words padding (80 bytes, 16-byte aligned)
#define FRAME_MEPC 16

static __arduino_handler_t irq_table[32];   // .bss = DMEM

void __attribute__((naked, aligned(4), section(RAMFUNC_SECTION))) __arduino_trap_entry(void)
{
    __asm__ volatile (
        "addi sp, sp, -80   \n"
        "sw   ra,  0(sp)    \n"
        "sw   t0,  4(sp)    \n"
        "sw   t1,  8(sp)    \n"
        "sw   t2, 12(sp)    \n"
        "sw   a0, 16(sp)    \n"
        "sw   a1, 20(sp)    \n"
        "sw   a2, 24(sp)    \n"
        "sw   a3, 28(sp)    \n"
        "sw   a4, 32(sp)    \n"
        "sw   a5, 36(sp)    \n"
        "sw   a6, 40(sp)    \n"
        "sw   a7, 44(sp)    \n"
        "sw   t3, 48(sp)    \n"
        "sw   t4, 52(sp)    \n"
        "sw   t5, 56(sp)    \n"
        "sw   t6, 60(sp)    \n"
        "csrr t0, mepc      \n"
        "sw   t0, 64(sp)    \n"
        "csrr a0, mcause    \n"
        "mv   a1, sp        \n"
        "call __arduino_trap_dispatch \n"
        "lw   t0, 64(sp)    \n"
        "csrw mepc, t0      \n"
        "lw   ra,  0(sp)    \n"
        "lw   t0,  4(sp)    \n"
        "lw   t1,  8(sp)    \n"
        "lw   t2, 12(sp)    \n"
        "lw   a0, 16(sp)    \n"
        "lw   a1, 20(sp)    \n"
        "lw   a2, 24(sp)    \n"
        "lw   a3, 28(sp)    \n"
        "lw   a4, 32(sp)    \n"
        "lw   a5, 36(sp)    \n"
        "lw   a6, 40(sp)    \n"
        "lw   a7, 44(sp)    \n"
        "lw   t3, 48(sp)    \n"
        "lw   t4, 52(sp)    \n"
        "lw   t5, 56(sp)    \n"
        "lw   t6, 60(sp)    \n"
        "addi sp, sp, 80    \n"
        "mret               \n"
    );
}

// Rare traps, in flash
static void __attribute__((noinline)) trap_unhandled(uint32_t mcause, uint32_t *frame)
{
    neorv32_rte_debug_handler();   // prints, disables an interrupt source, halts on fatal ones
    if ((int32_t)mcause >= 0 && mcause != TRAP_CODE_I_ACCESS) {
        // exception: continue behind the faulting instruction (2 bytes if compressed)
        uint32_t step = ((neorv32_cpu_csr_read(CSR_MTINST) & 3u) == 3u) ? 4u : 2u;
        frame[FRAME_MEPC] += step;
    }
}

void __attribute__((section(RAMFUNC_SECTION), used)) __arduino_trap_dispatch(uint32_t mcause, uint32_t *frame)
{
    if ((int32_t)mcause < 0) {
        __arduino_handler_t fn = irq_table[mcause & 31u];
        if (fn) {
            fn();
            return;
        }
    }
    trap_unhandled(mcause, frame);
}

void __arduino_irq_install(uint32_t code, __arduino_handler_t fn)
{
    code &= 31u;
    uint32_t s = __arduino_irq_save();
    irq_table[code] = fn;
    if (fn)
        neorv32_cpu_csr_set(CSR_MIE, 1u << code);
    else
        neorv32_cpu_csr_clr(CSR_MIE, 1u << code);
    __arduino_irq_restore(s);
}

void __arduino_trap_init(void)
{
    neorv32_cpu_csr_write(CSR_MTVEC, (uint32_t)&__arduino_trap_entry);
}
