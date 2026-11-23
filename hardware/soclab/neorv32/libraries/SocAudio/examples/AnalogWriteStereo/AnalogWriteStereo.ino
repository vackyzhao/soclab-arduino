#include <SocAudio.h>

// DAC_LEFT / DAC_RIGHT are the two existing audio sigma-delta outputs.
// A low-pass filter converts the pulse density into an average voltage.
// On a path with an AC-coupling capacitor, DC does not pass that capacitor.
// This deliberately demonstrates levels, not a tone for a speaker.
void setup()
{
    analogWriteResolution(8);  // default: 0..255
    analogWrite(DAC_LEFT, 0);
    analogWrite(DAC_RIGHT, 255);
}

void loop()
{
    const int levels[] = {0, 64, 128, 192, 255};
    for (unsigned int i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
        analogWrite(DAC_LEFT, levels[i]);
        analogWrite(DAC_RIGHT, 255 - levels[i]);
        delay(1000);
    }
}
