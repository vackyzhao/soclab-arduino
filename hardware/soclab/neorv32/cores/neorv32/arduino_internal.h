// arduino_internal.h - interfaces between the core and the libraries (not for sketches)
#ifndef ARDUINO_INTERNAL_H
#define ARDUINO_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*__arduino_handler_t)(void);

// Interrupt handler for mcause = 0x80000000 | code (code 0..31: 3 MSI, 7 MTI,
// 11 MEI, 16 + n FIRQ n). Handlers run with interrupts disabled; handlers on
// the audio path must be in DMEM (RAMFUNC). fn = 0 removes the handler.
void __arduino_irq_install(uint32_t code, __arduino_handler_t fn);

// GPIO pin handler without the Arduino mode handling (trigger set by the
// caller with neorv32_gpio_irq_setup). Used by SocAudio for pin 22.
void __arduino_gpio_handler(int pin, __arduino_handler_t fn);

// Called while delay() waits and after/while Serial output (SocAudio polling
// mode: Synth.update). Set with __arduino_set_idle_hook (Arduino.h).
extern __arduino_handler_t __arduino_idle_hook;

// UART RX poll (DMEM): moves received characters from the UART RX FIFO (one
// character deep) into Serial's ring buffer. Installed by Serial.begin, 0
// without Serial. Code that runs with interrupts off for longer than one
// character time calls it now and then (SocAudio: after every render block of
// the FIFO-low refill); only with interrupts off (it races the RX interrupt).
extern __arduino_handler_t __arduino_rx_poll_hook;

// Global interrupt enable (mstatus.MIE): save + disable / restore. Always
// inlined: DMEM code (RAMFUNC) must not call an outlined copy in flash.
static inline __attribute__((always_inline)) uint32_t __arduino_irq_save(void)
{
    uint32_t s;
    __asm__ volatile ("csrrci %0, mstatus, 8" : "=r"(s) :: "memory");
    return s & 8u;
}
static inline __attribute__((always_inline)) void __arduino_irq_restore(uint32_t s)
{
    if (s)
        __asm__ volatile ("csrsi mstatus, 8" ::: "memory");
}

// tone() backend, provided by SocAudio (referenced weakly by the core:
// tone() does nothing without it)
void __arduino_tone(uint8_t pin, unsigned int frequency, unsigned long duration_ms);
void __arduino_no_tone(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif // ARDUINO_INTERNAL_H
