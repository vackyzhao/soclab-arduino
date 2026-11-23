// Reconstructed from course dac_sin.elf, main at 0xE00001F0.
// 109 exact samples at 48 kHz -> 440.366972... Hz on both channels.
// Poll the FIFO; toggle GPIO20 after every 256 complete sine periods.
#include <SocAudio.h>
#include "CourseSineTable.h"

static unsigned sampleIndex = 0;
static unsigned sinePeriods = 0;

void setup()
{
    Serial.begin(19200);
    Serial.println("dac_sin demo");
    Serial.println("Configuring DAC");
    pinMode(20, OUTPUT);
    Audio.begin();
}

void loop()
{
    // Advance only after write() accepts the complete stereo frame.
    const int32_t sample = sin_buf[sampleIndex];
    if (!Audio.write(sample, sample))
        return;
    if (++sampleIndex == 109) {
        sampleIndex = 0;
        if (++sinePeriods == 256) {
            sinePeriods = 0;
            digitalToggle(20);
        }
    }
}
