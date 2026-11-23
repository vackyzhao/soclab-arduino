// Reconstructed from course dac_sin_ir.elf, main at 0xE00001F0.
// GPIO22 rising edge: audio FIFO level < 128. ISR sets a flag only.
#include <SocAudio.h>
#include "CourseSineTable.h"

static volatile bool fifoLowIr = false;
static unsigned sampleIndex = 0;
static uint8_t wakeCount = 0;

static void RAMFUNC onFifoLow()
{
    // Arduino's GPIO dispatcher has already acknowledged the pending bit.
    fifoLowIr = true;
}

void setup()
{
    Serial.begin(19200);
    Serial.println("dac_sin demo");
    Serial.println("Configuring DAC");
    pinMode(20, OUTPUT);
    pinMode(PIN_AUDIO_LOW, INPUT);
    Audio.begin(128);
    Serial.println("Configuring Interrupts");
    attachInterrupt(digitalPinToInterrupt(PIN_AUDIO_LOW), onFifoLow, RISING);
    // loop() fills the initially empty FIFO without waiting for an IRQ.
}

void loop()
{
    if (Audio.full()) {
        // Preserve the ELF's busy wait. Only ~2.67 ms of buffered audio
        // remain when the low-water interrupt arrives; do not add delay().
        while (!fifoLowIr) {
        }
        fifoLowIr = false;
        wakeCount = uint8_t(wakeCount + 1u);
        if (wakeCount == 0)       // wraps every 256 flag consumptions
            digitalToggle(20);
        return;
    }
    const int32_t sample = sin_buf[sampleIndex];
    if (Audio.write(sample, sample)) {
        if (++sampleIndex == 109)
            sampleIndex = 0;
    }
}
