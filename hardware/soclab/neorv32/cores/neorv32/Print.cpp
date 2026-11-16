// Print.cpp - text output base class (own implementation)
#include "Arduino.h"

size_t Print::write(const uint8_t *buf, size_t len)
{
    size_t n = 0;
    while (len--) {
        if (write(*buf++) == 0)
            break;
        n++;
    }
    return n;
}

// strlen in DMEM: string literals are in flash, and libc's strlen (flash)
// reading a string whose cache line has the same index as its own code
// line (1 KB direct-mapped XIP cache) costs two refills per word. From DMEM
// each line of the string is read once (and is still cached for the TX loop).
static size_t RAMFUNC dmem_strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

size_t Print::write(const char *s)
{
    return s ? write((const uint8_t *)s, dmem_strlen(s)) : 0;
}

size_t Print::print(const char *s)
{
    return write(s);
}

size_t Print::print(char c)
{
    return write((uint8_t)c);
}

size_t Print::println(void)
{
    return write((uint8_t)'\r') + write((uint8_t)'\n');
}

// Digits of n in the given base (2..36), with '-' in front if negative.
// 64 binary digits + sign fit into the buffer.
size_t Print::printUnsigned(unsigned long long n, int base, bool negative)
{
    char buf[66];
    char *p = &buf[sizeof(buf) - 1];
    *p = '\0';
    if (base < 2 || base > 36)
        base = 10;
    do {
        unsigned d = (unsigned)(n % (unsigned)base);
        n /= (unsigned)base;
        *--p = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    } while (n);
    if (negative)
        *--p = '-';
    return write(p);
}

size_t Print::print(unsigned long n, int base)
{
    // 32-bit path: no 64-bit division
    char buf[34];
    char *p = &buf[sizeof(buf) - 1];
    *p = '\0';
    if (base < 2 || base > 36)
        base = 10;
    do {
        unsigned d = (unsigned)(n % (unsigned)base);
        n /= (unsigned)base;
        *--p = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    } while (n);
    return write(p);
}

size_t Print::print(long n, int base)
{
    // as Arduino: a negative number is printed with '-' only in base 10,
    // other bases print the two's complement bits
    if (base == 10 && n < 0) {
        size_t k = write((uint8_t)'-');
        return k + print((unsigned long)(-(n + 1)) + 1UL, 10);
    }
    return print((unsigned long)n, base);
}

size_t Print::print(unsigned long long n, int base)
{
    return printUnsigned(n, base, false);
}

size_t Print::print(long long n, int base)
{
    if (base == 10 && n < 0)
        return printUnsigned((unsigned long long)(-(n + 1)) + 1ULL, 10, true);
    return printUnsigned((unsigned long long)n, base, false);
}

// Soft-float (no F/D extension) from XIP flash is slow: a handful of double
// operations (split, one multiply, rounding), the digits in integer math.
size_t Print::print(double x, int digits)
{
    static const double pow10[10] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9};
    static const uint32_t pow10i[10] = {1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u,
                                        10000000u, 100000000u, 1000000000u};
    size_t n = 0;
    // classify on the bit pattern (soft-float compares are library calls too)
    union { double d; uint64_t u; } v;
    v.d = x;
    bool neg = (v.u >> 63) != 0;
    uint64_t mag = v.u & 0x7FFFFFFFFFFFFFFFull;
    if (mag > 0x7FF0000000000000ull)
        return print("nan");
    if (mag == 0x7FF0000000000000ull)
        return print(neg ? "-inf" : "inf");
    if (mag > 0x41EFFFFFE0000000ull)   // 4294967040.0
        return print("ovf");
    if (digits < 0)
        digits = 0;
    if (digits > 9)
        digits = 9;
    if (neg && mag != 0) {
        n += write((uint8_t)'-');
        v.u = mag;
        x = v.d;
    }
    uint32_t ip = (uint32_t)x;
    uint32_t scale = pow10i[digits];
    uint32_t fd = (uint32_t)((x - (double)ip) * pow10[digits] + 0.5);   // rounded fraction
    if (fd >= scale) {
        fd -= scale;
        ip++;
    }
    n += print((unsigned long)ip, 10);
    if (digits > 0) {
        char buf[10];
        for (int i = digits - 1; i >= 0; i--) {
            buf[i] = (char)('0' + fd % 10u);
            fd /= 10u;
        }
        n += write((uint8_t)'.');
        n += write((const uint8_t *)buf, (size_t)digits);
    }
    return n;
}
