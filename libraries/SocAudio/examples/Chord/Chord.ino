// Chord - C major triad on three voices (C4 E4 G4), then the same with C5 on
// a fourth voice, interrupt driven. Prints the render statistics of each.
// SIM build (sketch.mk: time scale 10): 60 ms chords, 10 ms gaps.
#include <SocAudio.h>

#define CHORD_MS 600
#define GAP_MS   100

static void chord(const char *label, const uint8_t *notes, int n)
{
    Serial.print("chord ");
    Serial.println(label);
    Synth.resetStats();
    for (int i = 0; i < n; i++)
        Synth.noteOn(i, notes[i], 100);
    delay(CHORD_MS);
    Synth.allOff();
    Synth.printStats(Serial, label);
    delay(GAP_MS);
}

void setup()
{
    Serial.begin(19200);
    Serial.println("Chord: 3 and 4 voices, FIFO-low interrupt");
    Synth.begin(SYNTH_IRQ, 128);
    Synth.setEnvelope(5, 5);
}

void loop()
{
    static const uint8_t triad[3] = {60, 64, 67};
    static const uint8_t four[4] = {60, 64, 67, 72};
    chord("C4-E4-G4", triad, 3);
    chord("C4-E4-G4-C5", four, 4);
    Serial.println("chords done");
    delay(1000);
}
