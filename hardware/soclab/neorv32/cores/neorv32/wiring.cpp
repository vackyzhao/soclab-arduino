// wiring.cpp - time, digital I/O, pin interrupts, random, tone (own implementation)
#include "Arduino.h"
#include "arduino_internal.h"

// ---------------------------------------------------------------- time
// Time base: the 64-bit cycle counter [m]cycle[h] (F_CPU per second, counts
// every clock unless the CPU sleeps, which the core never does). Same count
// as the CLINT MTIME, but a CSR read: MTIME (IO offset 0xBFF8) cannot be
// simulated on this SoC, every IO access with address bits [6:5] != 0 makes
// the 8-word boot ROM of NEORV32 v1.11 index out of range (fatal in xsim).
// With ARDUINO_TIME_SCALE S the sketch time runs S times faster than real
// time (SIM builds). Read inline (not neorv32_cpu_get_cycle()): the delay()
// loop then runs in its own cache line(s) wherever the linker puts the NEORV32
// library (one XIP refill per iteration otherwise, if the two map to the same
// line of the 1 KB direct-mapped cache).
static inline uint64_t __attribute__((always_inline)) mtime_now(void)
{
    uint32_t hi, lo, hi2;
    do {
        __asm__ volatile ("csrr %0, mcycleh" : "=r"(hi));
        __asm__ volatile ("csrr %0, mcycle" : "=r"(lo));
        __asm__ volatile ("csrr %0, mcycleh" : "=r"(hi2));
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

unsigned long millis(void)
{
    return (unsigned long)(mtime_now() * (uint64_t)ARDUINO_TIME_SCALE / (F_CPU / 1000u));
}

unsigned long micros(void)
{
    // F_CPU / 1e6 = 98.304 = 12288 / 125
    return (unsigned long)(mtime_now() * (125ull * ARDUINO_TIME_SCALE) / 12288ull);
}

voidFuncPtr __arduino_idle_hook = 0;      // arduino_internal.h
voidFuncPtr __arduino_rx_poll_hook = 0;   // arduino_internal.h, set by Serial.begin

extern "C" void __arduino_set_idle_hook(voidFuncPtr fn)
{
    __arduino_idle_hook = fn;
}

extern "C" void __attribute__((weak)) yield(void)
{
}

// 32-bit arithmetic where it fits: the 64-bit division routine is slow from XIP
void delay(unsigned long ms)
{
    uint64_t n;
    if (ms < 0xFFFFFFFFu / (F_CPU / 1000u))
        n = (uint32_t)ms * (uint32_t)(F_CPU / 1000u) / (uint32_t)ARDUINO_TIME_SCALE;
    else
        n = (uint64_t)ms * (F_CPU / 1000u) / ARDUINO_TIME_SCALE;
    uint64_t t_end = mtime_now() + n;
    while (mtime_now() < t_end) {
        if (__arduino_idle_hook)
            __arduino_idle_hook();
        yield();
    }
}

void delayMicroseconds(unsigned int us)
{
    // cycles = us * 98.304 / scale = us * 12288 / (125 * scale)
    uint32_t n;
    if (us < 0xFFFFFFFFu / 12288u)
        n = us * 12288u / (125u * ARDUINO_TIME_SCALE);
    else
        n = (uint32_t)((uint64_t)us * 12288u / (125ull * ARDUINO_TIME_SCALE));
    uint32_t t0 = cycles();
    while ((uint32_t)(cycles() - t0) < n)
        ;
}

uint32_t cycles(void)
{
    return neorv32_cpu_csr_read(CSR_MCYCLE);
}

// ---------------------------------------------------------------- digital I/O
// The GPIO has separate inputs and outputs (gpio_i / gpio_o), no direction
// register. pinMode only records whether digitalRead returns the output latch
// (OUTPUT) or the input (INPUT). Only gpio_i[22] (audio FIFO low) is
// connected on the Lab 3 SoC, the other inputs read 0.
static uint32_t output_pins = 0;

void pinMode(uint8_t pin, uint8_t mode)
{
    if (pin >= NUM_DIGITAL_PINS)
        return;
    if (mode == OUTPUT)
        output_pins |= 1u << pin;
    else
        output_pins &= ~(1u << pin);
}

void digitalWrite(uint8_t pin, uint8_t val)
{
    if (pin >= NUM_DIGITAL_PINS)
        return;
    uint32_t m = 1u << pin;
    uint32_t s = __arduino_irq_save();   // read-modify-write, an ISR may write too
    if (val)
        NEORV32_GPIO->PORT_OUT |= m;
    else
        NEORV32_GPIO->PORT_OUT &= ~m;
    __arduino_irq_restore(s);
}

void digitalToggle(uint8_t pin)
{
    if (pin >= NUM_DIGITAL_PINS)
        return;
    uint32_t s = __arduino_irq_save();
    NEORV32_GPIO->PORT_OUT ^= 1u << pin;
    __arduino_irq_restore(s);
}

int digitalRead(uint8_t pin)
{
    if (pin >= NUM_DIGITAL_PINS)
        return LOW;
    uint32_t m = 1u << pin;
    uint32_t v;
    if (output_pins & m)
        v = NEORV32_GPIO->PORT_OUT;
    else
        v = NEORV32_GPIO->PORT_IN;
    return (v & m) ? HIGH : LOW;
}

// ---------------------------------------------------------------- pin interrupts
// One GPIO FIRQ (FIRQ 8) for all pins. Dispatcher in DMEM; the sketch's
// callbacks are wherever the sketch put them (flash unless RAMFUNC).
static voidFuncPtr pin_handler[NUM_DIGITAL_PINS];   // DMEM
static uint32_t change_pins = 0;                    // pins in CHANGE mode

static void RAMFUNC gpio_dispatch(void)
{
    uint32_t pend = NEORV32_GPIO->IRQ_PENDING;
    NEORV32_GPIO->IRQ_PENDING = ~pend;   // clear the bits handled here
    uint32_t chg = pend & change_pins;
    if (chg) {
        // NEORV32 GPIO has no both-edges trigger: re-arm for the opposite edge
        uint32_t in = NEORV32_GPIO->PORT_IN;
        NEORV32_GPIO->IRQ_POLARITY = (NEORV32_GPIO->IRQ_POLARITY & ~chg) | (~in & chg);
    }
    uint32_t m = 1;
    voidFuncPtr *h = pin_handler;
    while (pend) {
        if (pend & m) {
            pend &= ~m;
            if (*h)
                (*h)();
        }
        m <<= 1;
        h++;
    }
}

static void gpio_irq_on(void)
{
    __arduino_irq_install(GPIO_TRAP_CODE & 31u, gpio_dispatch);
}

extern "C" void __arduino_gpio_handler(int pin, __arduino_handler_t fn)
{
    if (pin < 0 || pin >= NUM_DIGITAL_PINS)
        return;
    uint32_t s = __arduino_irq_save();
    pin_handler[pin] = fn;
    change_pins &= ~(1u << pin);
    gpio_irq_on();
    __arduino_irq_restore(s);
}

void attachInterrupt(int pin, voidFuncPtr fn, int mode)
{
    if (pin < 0 || pin >= NUM_DIGITAL_PINS)
        return;
    uint32_t m = 1u << pin;
    uint32_t s = __arduino_irq_save();
    neorv32_gpio_irq_disable(m);   // also clears a pending request
    pin_handler[pin] = fn;
    change_pins &= ~m;
    switch (mode) {
    case LOW:     neorv32_gpio_irq_setup(pin, GPIO_TRIG_LEVEL_LOW); break;
    case HIGH:    neorv32_gpio_irq_setup(pin, GPIO_TRIG_LEVEL_HIGH); break;
    case FALLING: neorv32_gpio_irq_setup(pin, GPIO_TRIG_EDGE_FALLING); break;
    case CHANGE:
        change_pins |= m;
        neorv32_gpio_irq_setup(pin, (NEORV32_GPIO->PORT_IN & m) ? GPIO_TRIG_EDGE_FALLING
                                                                : GPIO_TRIG_EDGE_RISING);
        break;
    default:      neorv32_gpio_irq_setup(pin, GPIO_TRIG_EDGE_RISING); break;
    }
    gpio_irq_on();
    if (fn)
        neorv32_gpio_irq_enable(m);
    __arduino_irq_restore(s);
}

void detachInterrupt(int pin)
{
    if (pin < 0 || pin >= NUM_DIGITAL_PINS)
        return;
    uint32_t m = 1u << pin;
    uint32_t s = __arduino_irq_save();
    neorv32_gpio_irq_disable(m);
    pin_handler[pin] = 0;
    change_pins &= ~m;
    __arduino_irq_restore(s);
}

void interrupts(void)
{
    __asm__ volatile ("csrsi mstatus, 8" ::: "memory");
}

void noInterrupts(void)
{
    __asm__ volatile ("csrci mstatus, 8" ::: "memory");
}

// ---------------------------------------------------------------- math
long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    if (in_max == in_min)
        return out_min;
    return (long)((long long)(x - in_min) * (out_max - out_min) / (in_max - in_min)) + out_min;
}

// xorshift32
static uint32_t rnd_state = 0x2545F491u;

void randomSeed(unsigned long seed)
{
    if (seed != 0)
        rnd_state = (uint32_t)seed;
}

static uint32_t rnd_next(void)
{
    uint32_t x = rnd_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rnd_state = x;
    return x;
}

long random(long max)
{
    if (max <= 0)
        return 0;
    return (long)(rnd_next() % (uint32_t)max);
}

long random(long min, long max)
{
    if (min >= max)
        return min;
    return min + (long)(rnd_next() % (uint32_t)(max - min));
}

// ---------------------------------------------------------------- analog output
static uint8_t analog_write_bits = 8;
extern "C" void __arduino_analog_write(uint8_t, uint32_t, uint8_t) __attribute__((weak));

void analogWriteResolution(int bits)
{
    analog_write_bits = (uint8_t)((bits < 1) ? 1 : ((bits > 24) ? 24 : bits));
}

void analogWrite(uint8_t pin, int value)
{
    uint8_t bits = analog_write_bits;
    uint32_t maximum = (1u << bits) - 1u;
    uint32_t v = value < 0 ? 0u : (uint32_t)value;
    if (v > maximum)
        v = maximum;
    if (pin == DAC_LEFT || pin == DAC_RIGHT) {
        if (__arduino_analog_write)
            __arduino_analog_write(pin, v, bits);
    } else if (pin < NUM_DIGITAL_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, v >= (1u << (bits - 1)) ? HIGH : LOW);
    }
}

// ---------------------------------------------------------------- tone
extern "C" void __arduino_tone(uint8_t, unsigned int, unsigned long) __attribute__((weak));
extern "C" void __arduino_no_tone(uint8_t) __attribute__((weak));

void tone(uint8_t pin, unsigned int frequency, unsigned long duration)
{
    if (__arduino_tone)
        __arduino_tone(pin, frequency, duration);
}

void noTone(uint8_t pin)
{
    if (__arduino_no_tone)
        __arduino_no_tone(pin);
}
