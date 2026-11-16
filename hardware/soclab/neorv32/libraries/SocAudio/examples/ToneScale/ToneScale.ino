// ToneScale - C major scale C4..C5 on voice 0, the synth fed by the FIFO-low
// interrupt. Prints each note and the render statistics of the note.
// SIM build (sketch.mk: time scale 10): 20 ms notes with 5 ms gaps (plus print time).
#include <SocAudio.h>

static const uint8_t scale[8] = {60, 62, 64, 65, 67, 69, 71, 72};
static const char *const names[8] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};

#define NOTE_MS 200
#define GAP_MS  50

void setup()
{
    Serial.begin(19200);
    Serial.println("ToneScale: C major scale, FIFO-low interrupt");
    Synth.begin(SYNTH_IRQ, 128);
    Synth.setEnvelope(2, 4);   // real ms: the release ends inside the 5 ms SIM gap
}

void loop()
{
    for (int i = 0; i < 8; i++) {
        Synth.resetStats();
        Synth.noteOn(0, scale[i], 127);
        Serial.print("note ");
        Serial.print(names[i]);
        Serial.print(" midi ");
        Serial.print(scale[i]);
        // exact pitch of the phase increment; integer formatting (a float
        // print costs ~2 ms of XIP refills of the soft-float library)
        uint32_t mhz = Synth.frequencyMilliHz(0);
        Serial.print(" f ");
        Serial.print(mhz / 1000);
        Serial.print('.');
        Serial.print((mhz % 1000) / 100);
        Serial.print((mhz % 100) / 10);
        Serial.print(mhz % 10);
        Serial.println(" Hz");
        delay(NOTE_MS);
        Synth.noteOff(0);
        Synth.printStats(Serial, names[i]);
        delay(GAP_MS);
    }
    // Arduino tone(): voice 0, ends by itself after the duration (the pin is ignored)
    Serial.println("tone A5 880 Hz 100 ms");
    tone(LED_BUILTIN, 880, 100);
    delay(150);
    Serial.println("scale done");
    delay(1000);
}
