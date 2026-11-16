// HardwareSerial.cpp - Serial on NEORV32 UART0 (own implementation)
#include "Arduino.h"
#include "arduino_internal.h"

HardwareSerial Serial;

static volatile uint8_t rx_buf[SERIAL_RX_BUFFER_SIZE];   // DMEM
static volatile uint32_t rx_head = 0;                     // written by the ISR
static volatile uint32_t rx_tail = 0;                     // written by read()
static volatile uint32_t rx_lost = 0;

// UART0 RX FIRQ: level "RX FIFO not empty", drain the FIFO into the ring buffer.
// The RX FIFO holds one character: an interrupt delayed by more than one
// character time (e.g. by an XIP cache refill, ~22 us, or by a long handler
// with interrupts off) loses characters; the UART's overrun flag is sticky
// (until the UART is disabled), it adds 1. The same function is the RX poll
// hook (arduino_internal.h) that long handlers call with interrupts off
// (SocAudio's FIFO-low refill, after every block).
static volatile uint32_t rx_hw_over = 0;

static void RAMFUNC serial_rx_drain(void)
{
    uint32_t ctrl = NEORV32_UART0->CTRL;
    if (ctrl & (1u << UART_CTRL_RX_OVER))
        rx_hw_over = 1u;
    while (ctrl & (1u << UART_CTRL_RX_NEMPTY)) {
        uint8_t c = (uint8_t)(NEORV32_UART0->DATA >> UART_DATA_RTX_LSB);
        uint32_t h = rx_head;
        uint32_t next = (h + 1u) & (SERIAL_RX_BUFFER_SIZE - 1u);
        if (next != rx_tail) {
            rx_buf[h] = c;
            rx_head = next;
        } else {
            rx_lost = rx_lost + 1u;
        }
        ctrl = NEORV32_UART0->CTRL;
    }
}

void HardwareSerial::begin(unsigned long baud, uint8_t config)
{
    (void)config;
#if ARDUINO_SIM
    baud = ARDUINO_SIM_BAUD;   // simulation build: 80 clocks per bit
#endif
    if (baud == 0)
        baud = 19200;
    __arduino_irq_install(UART0_RX_TRAP_CODE & 31u, 0);
    __arduino_rx_poll_hook = 0;
    neorv32_uart0_setup(baud, 1u << UART_CTRL_IRQ_RX_NEMPTY);
    rx_head = 0;
    rx_tail = 0;
    rx_lost = 0;
    rx_hw_over = 0;
    baud_ = baud;
    __arduino_rx_poll_hook = serial_rx_drain;
    __arduino_irq_install(UART0_RX_TRAP_CODE & 31u, serial_rx_drain);
}

void HardwareSerial::end()
{
    flush();
    __arduino_irq_install(UART0_RX_TRAP_CODE & 31u, 0);
    __arduino_rx_poll_hook = 0;
    neorv32_uart0_disable();
    baud_ = 0;
}

int HardwareSerial::available()
{
    return (int)((rx_head - rx_tail) & (SERIAL_RX_BUFFER_SIZE - 1u));
}

int HardwareSerial::peek()
{
    uint32_t t = rx_tail;
    if (rx_head == t)
        return -1;
    return rx_buf[t];
}

int HardwareSerial::read()
{
    uint32_t t = rx_tail;
    if (rx_head == t)
        return -1;
    int c = rx_buf[t];
    rx_tail = (t + 1u) & (SERIAL_RX_BUFFER_SIZE - 1u);
    return c;
}

uint32_t HardwareSerial::overruns() const
{
    return rx_lost + rx_hw_over;
}

int HardwareSerial::availableForWrite()
{
    if (baud_ == 0)
        return 0;
    return (NEORV32_UART0->CTRL & (1u << UART_CTRL_TX_FULL)) ? 0 : 1;
}

void HardwareSerial::flush()
{
    if (baud_ == 0)
        return;
    while (NEORV32_UART0->CTRL & (1u << UART_CTRL_TX_BUSY))
        ;
}

// TX in DMEM: the wait for the TX FIFO is the hot loop of every print. The
// idle hook (polling synth) runs while waiting and once per call, so that
// printing does not starve the audio FIFO.
static void RAMFUNC uart_tx(const uint8_t *buf, size_t len)
{
    __arduino_handler_t idle = __arduino_idle_hook;
    while (len--) {
        while (NEORV32_UART0->CTRL & (1u << UART_CTRL_TX_FULL))
            if (idle)
                idle();
        NEORV32_UART0->DATA = (uint32_t)*buf++ << UART_DATA_RTX_LSB;
    }
    if (idle)
        idle();
}

size_t HardwareSerial::write(uint8_t c)
{
    if (baud_ == 0)
        return 0;
    uart_tx(&c, 1);
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buf, size_t len)
{
    if (baud_ == 0)
        return 0;
    uart_tx(buf, len);
    return len;
}
