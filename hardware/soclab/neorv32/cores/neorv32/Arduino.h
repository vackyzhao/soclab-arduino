// Arduino.h - Arduino-style API for the SoC Lab NEORV32 SoC (Lab 3 hardware)
//
// Own implementation (no ArduinoCore-API sources). Board: NEORV32 rv32imc at
// 98.304 MHz, code in SPI flash (XIP), 16 KB DMEM, CLINT, UART0, 24 GPIO,
// audio peripheral at 0xFFD0_0000 (libraries/SocAudio).
#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <neorv32.h>

// ---------------------------------------------------------------- build
#define ARDUINO 10819
#define ARDUINO_ARCH_NEORV32 1
#define ARDUINO_SOCLAB_NEORV32 1

#define F_CPU 98304000UL

// SIM builds (make SIM=1): UART forced to 1228800 baud (80 clocks per bit,
// as the course fw/sim images) and time scaled by ARDUINO_TIME_SCALE: the
// sketch sees millis()/micros() run that many times faster and delay() is
// that much shorter. Audio (48 kHz) is never scaled.
#ifndef ARDUINO_SIM
#define ARDUINO_SIM 0
#endif
#ifndef ARDUINO_TIME_SCALE
#define ARDUINO_TIME_SCALE 1
#endif
#define ARDUINO_SIM_BAUD 1228800UL

// Code placed in DMEM (copied from flash by crt0 with .data): no XIP cache
// refills, for interrupt handlers and time-critical loops. GCC marks the
// section "ax" (code), which makes the assembler warn about a .data section
// with code attributes; the build (tools/rvcc.sh) drops that one warning.
// It must stay a code section: only there do assembler and linker keep the
// relocations that linker relaxation needs.
#define RAMFUNC_SECTION ".data.ramfunc"
#define RAMFUNC __attribute__((section(RAMFUNC_SECTION), noinline))

// ---------------------------------------------------------------- constants
#define LOW  0
#define HIGH 1

#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2   // no pull resistors on this SoC: same as INPUT

// interrupt modes (LOW / HIGH = level, as the NEORV32 GPIO supports)
#define CHANGE  2
#define FALLING 3
#define RISING  4

#define LSBFIRST 0
#define MSBFIRST 1

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

// pins: gpio_o[23:0] / gpio_i[23:0]
#define NUM_DIGITAL_PINS 24
#define LED_BUILTIN      20   // LED D2 (gpio_o[20])
#define LED_D2           20
#define LED_D3           21   // gpio_o[21]
#define PIN_AUDIO_LOW    22   // gpio_i[22]: audio FIFO level below FIFO_LOW
#define NOT_AN_INTERRUPT (-1)
#define digitalPinToInterrupt(p) (((p) >= 0 && (p) < NUM_DIGITAL_PINS) ? (p) : NOT_AN_INTERRUPT)

#define PI         3.1415926535897932384626433832795
#define HALF_PI    1.5707963267948966192313216916398
#define TWO_PI     6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER      2.718281828459045235360287471352

// ---------------------------------------------------------------- types
typedef bool     boolean;
typedef uint8_t  byte;
typedef uint16_t word;

// ---------------------------------------------------------------- bit macros
#define bit(b)                    (1UL << (b))
#define bitRead(value, b)         (((value) >> (b)) & 0x01)
#define bitSet(value, b)          ((value) |= (1UL << (b)))
#define bitClear(value, b)        ((value) &= ~(1UL << (b)))
#define bitToggle(value, b)       ((value) ^= (1UL << (b)))
#define bitWrite(value, b, v)     ((v) ? bitSet(value, b) : bitClear(value, b))
#define lowByte(w)                ((uint8_t)((w) & 0xff))
#define highByte(w)               ((uint8_t)((w) >> 8))
#ifndef _BV
#define _BV(b)                    (1UL << (b))
#endif

#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define sq(x)        ((x) * (x))

#define clockCyclesPerMicrosecond() (F_CPU / 1000000.0)
#define F(s) (s)   // no separate flash address space: strings are in flash already

#ifdef __cplusplus
// min/max/constrain as templates (two types: min(int, long) works)
template <class T, class U>
static inline auto min(const T &a, const U &b) -> decltype(a < b ? a : b) { return (b < a) ? b : a; }
template <class T, class U>
static inline auto max(const T &a, const U &b) -> decltype(a < b ? a : b) { return (a < b) ? b : a; }
template <class T, class L, class H>
static inline T constrain(const T &x, const L &lo, const H &hi)
{
    return (x < lo) ? (T)lo : ((hi < x) ? (T)hi : x);
}
#else
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------- sketch
void setup(void);
void loop(void);
void yield(void);   // weak, called while delay() waits

// ---------------------------------------------------------------- time
unsigned long millis(void);
unsigned long micros(void);
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);

// ---------------------------------------------------------------- digital I/O
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int  digitalRead(uint8_t pin);
void digitalToggle(uint8_t pin);

// ---------------------------------------------------------------- interrupts
typedef void (*voidFuncPtr)(void);
void attachInterrupt(int irq, voidFuncPtr fn, int mode);
void detachInterrupt(int irq);
void interrupts(void);
void noInterrupts(void);

// ---------------------------------------------------------------- math
long map(long x, long in_min, long in_max, long out_min, long out_max);
void randomSeed(unsigned long seed);

// ---------------------------------------------------------------- tone
// Mapped to voice 0 of the SocAudio synth (the pin is ignored: audio goes to
// the DAC outputs). duration 0 = until noTone().
// IDE / arduino-cli sketches must include <SocAudio.h> to link the synth library.
void tone(uint8_t pin, unsigned int frequency, unsigned long duration);
void noTone(uint8_t pin);

// ---------------------------------------------------------------- SoC extras
uint32_t cycles(void);            // mcycle (low word), 98.304 cycles per us
void __arduino_set_idle_hook(voidFuncPtr fn);   // called while delay() and Serial output wait

#ifdef __cplusplus
} // extern "C"

long random(long max);
long random(long min, long max);
static inline void tone(uint8_t pin, unsigned int frequency) { tone(pin, frequency, 0); }

#include "Print.h"
#include "HardwareSerial.h"
#endif

#endif // ARDUINO_H
