// Print.h - text output base class (own implementation)
#ifndef ARDUINO_PRINT_H
#define ARDUINO_PRINT_H

#include <stdint.h>
#include <stddef.h>

class Print {
public:
    constexpr Print() {}

    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *buf, size_t len);
    size_t write(const char *s);
    size_t write(const char *buf, size_t len) { return write((const uint8_t *)buf, len); }
    virtual int availableForWrite() { return 0; }
    virtual void flush() {}

    size_t print(const char *s);
    size_t print(char c);
    size_t print(signed char n, int base = 10) { return print((long)n, base); }
    size_t print(unsigned char n, int base = 10) { return print((unsigned long)n, base); }
    size_t print(short n, int base = 10) { return print((long)n, base); }
    size_t print(unsigned short n, int base = 10) { return print((unsigned long)n, base); }
    size_t print(int n, int base = 10) { return print((long)n, base); }
    size_t print(unsigned int n, int base = 10) { return print((unsigned long)n, base); }
    size_t print(long n, int base = 10);
    size_t print(unsigned long n, int base = 10);
    size_t print(long long n, int base = 10);
    size_t print(unsigned long long n, int base = 10);
    size_t print(double x, int digits = 2);
    // No print(bool): as in Arduino a bool is promoted to int and prints 1 / 0,
    // and a pointer other than a char pointer matches no overload (a compile
    // error instead of a silent pointer -> bool conversion printing "1").

    size_t println(void);
    template <typename T>
    size_t println(T v) { size_t n = print(v); return n + println(); }
    template <typename T>
    size_t println(T v, int fmt) { size_t n = print(v, fmt); return n + println(); }

private:
    size_t printUnsigned(unsigned long long n, int base, bool negative);
};

#endif // ARDUINO_PRINT_H
