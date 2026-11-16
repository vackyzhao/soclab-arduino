// HardwareSerial.h - Serial on NEORV32 UART0 (own implementation)
//
// TX: blocking, through the UART TX FIFO, the byte loop in DMEM (printing does
// not thrash the 1 KB XIP cache). RX: interrupt driven (UART0 RX FIRQ,
// handler in DMEM) into a 64-byte ring buffer; the same handler is the RX
// poll hook that long interrupt handlers call (arduino_internal.h).
#ifndef ARDUINO_HARDWARESERIAL_H
#define ARDUINO_HARDWARESERIAL_H

#include "Print.h"

#ifndef SERIAL_RX_BUFFER_SIZE
#define SERIAL_RX_BUFFER_SIZE 64   // power of two
#endif

#define SERIAL_8N1 0   // the only frame format of the NEORV32 UART

class HardwareSerial final : public Print {
public:
    constexpr HardwareSerial() {}

    void begin(unsigned long baud, uint8_t config = SERIAL_8N1);
    void end();
    int available();
    int peek();
    int read();
    int availableForWrite() override;
    void flush() override;   // wait until the last character has left
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buf, size_t len) override;
    using Print::write;
    unsigned long baud() const { return baud_; }
    uint32_t overruns() const;   // lost characters: ring buffer full, +1 if the UART RX FIFO overran (sticky)
    explicit operator bool() const { return baud_ != 0; }

private:
    unsigned long baud_ = 0;
};

extern HardwareSerial Serial;

#endif // ARDUINO_HARDWARESERIAL_H
