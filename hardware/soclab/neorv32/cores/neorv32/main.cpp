// main.cpp - Arduino entry point
//
// crt0 (ext/neorv32/sw/common/crt0.S) copies .data (and the RAMFUNC code in
// it) to DMEM, clears .bss and runs the C++ static constructors
// (__init_array) before main, with mie = 0 and mstatus.MIE = 0. crt0 then
// rewrites mstatus from t0, which a constructor may have changed:
// neorv32_rte_setup() sets mstatus again first. It also clears mie, which a
// constructor may have set (e.g. Serial.begin in a global object's
// constructor enables the UART RX interrupt): main keeps those enables.
#include "Arduino.h"
#include "arduino_internal.h"

extern "C" void __arduino_trap_init(void);

int main(void)
{
    uint32_t mie = neorv32_cpu_csr_read(CSR_MIE);   // enabled by constructors
    neorv32_rte_setup();        // mstatus = M-mode, mtvec = RTE, mie = 0
    __arduino_trap_init();      // mtvec = DMEM trap entry (trap.c)
    neorv32_cpu_csr_write(CSR_MIE, mie);
    interrupts();               // global enable; sources are enabled one by one
    setup();
    for (;;)
        loop();
    return 0;
}

// ---------------------------------------------------------------- C++ runtime
// No heap (__neorv32_heap_size = 0), no exceptions, no RTTI.
extern "C" void __cxa_pure_virtual(void)
{
    for (;;)
        ;
}

void *__dso_handle = 0;

void operator delete(void *) noexcept {}
void operator delete(void *, unsigned int) noexcept {}
