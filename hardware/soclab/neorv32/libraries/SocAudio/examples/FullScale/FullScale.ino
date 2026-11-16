// FullScale - SYNTH_VOICES = 2 (sketch.mk) at volume 255: the largest samples
// the synth can produce. Two voices in unison (same note, started in the same
// block: same phase) sum to twice the table entry. The entries are clamped to
// (2^23 - 1) / 2 = 4194303, so the sum peaks at 8388606 (-0.0 dBFS) and never
// wraps (before the clamp the peak entry was 4194304: 2 x 4194304 = 2^23
// became -2^23 after the 24-bit mask, a full-scale click). Then a two-note
// chord at full velocity.
// SIM build (sketch.mk: time scale 10): 25 ms notes, 5 ms gaps.
#include <SocAudio.h>

#define NOTE_MS 250
#define GAP_MS  50

static void play(const char *label, uint8_t n0, uint8_t n1)
{
    Serial.print("play ");
    Serial.println(label);
    Synth.resetStats();
    noInterrupts();               // both voices start in the same block
    Synth.noteOn(0, n0, 127);
    Synth.noteOn(1, n1, 127);
    interrupts();
    delay(NOTE_MS);
    Synth.allOff();
    Synth.printStats(Serial, label);
    delay(GAP_MS);
}

void setup()
{
    Serial.begin(19200);
    Serial.println("FullScale: 2 voices in unison at volume 255");
    Synth.begin(SYNTH_IRQ, 128);
    Synth.setEnvelope(2, 4);   // real ms: the release ends inside the 5 ms SIM gap
    Serial.print("voices ");
    Serial.println(SYNTH_VOICES);
}

void loop()
{
    play("A4+A4", 69, 69);
    play("C5+C5", 72, 72);
    play("A4+E5", 69, 76);
    Serial.println("fullscale done");
    delay(1000);
}
