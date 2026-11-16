// SerialSynth - Serial input while the FIFO-low interrupt synth renders 4
// voices at velocity 100 (the gain multiply: the longest refill, ~0.7 ms with
// interrupts off). Every received line is echoed with the number of lost
// characters. The UART RX FIFO holds one character (0.52 ms at 19200 baud):
// without help a character arriving early in a refill would be overwritten by
// the next one. The refill drains the RX FIFO into Serial's ring buffer after
// every block (Serial's RX poll hook, ~0.07 ms apart with 4 voices), so no
// character is lost: "lost 0".
// SIM build: tb_soc sends a 48-character line 8 times with 80 us after every
// character (88 us per character at 1228800 baud, ~8 characters per refill).
// -DSTRESS_NO_RX_POLL (SKETCH_FLAGS) removes the poll hook: the same run then
// loses characters (negative control).
#include <SocAudio.h>
#if defined(STRESS_NO_RX_POLL)
#include <arduino_internal.h>
#endif

#define LINES 8

static const uint8_t notes[4] = {60, 64, 67, 72};
static char line[80];
static int len = 0;
static uint32_t lines = 0, chars = 0;

void setup()
{
    Serial.begin(19200);
    Serial.println("SerialSynth: 4 voices (FIFO-low interrupt) + serial echo");
#if defined(STRESS_NO_RX_POLL)
    __arduino_rx_poll_hook = 0;
#endif
    Synth.begin(SYNTH_IRQ, 128);
    for (int i = 0; i < 4; i++)
        Synth.noteOn(i, notes[i], 100);
    Synth.resetStats();
    Serial.println("send lines");
}

void loop()
{
    while (Serial.available() > 0) {
        int c = Serial.read();
        chars++;
        if (c != '\n') {
            if (c != '\r' && len < (int)sizeof(line) - 1)
                line[len++] = (char)c;
            continue;
        }
        line[len] = '\0';
        len = 0;
        lines++;
        Serial.print("echo ");
        Serial.print(lines);
        Serial.print(": ");
        Serial.print(line);
        Serial.print(" (lost ");
        Serial.print(Serial.overruns());
        Serial.println(")");
        if (lines == LINES) {
            Synth.printStats(Serial, "rx");
            Serial.print("rx ");
            Serial.print(chars);
            Serial.print(" chars ");
            Serial.print(lines);
            Serial.print(" lines lost ");
            Serial.println(Serial.overruns());
            Serial.println("stress done");
        }
    }
}
