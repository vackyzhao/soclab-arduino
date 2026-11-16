// SynthBench - render cost of the synth for 0..SYNTH_VOICES voices, at unity
// gain (velocity 127: no multiply) and at velocity 100 (one multiply per
// voice and frame). Polling mode: delay() calls Synth.update(), the stats
// count only the time inside the render function (cycles per frame).
#include <SocAudio.h>

#define SETTLE_MS 20    // attack done (envelope 1 ms)
#define BENCH_MS  50

static void bench(int voices, uint8_t velocity)
{
    for (int i = 0; i < SYNTH_VOICES; i++) {
        if (i < voices)
            Synth.noteOn(i, 60 + 4 * i, velocity);
        else
            Synth.noteOff(i);
    }
    delay(SETTLE_MS);
    Synth.resetStats();
    delay(BENCH_MS);
    char label[16] = "v0_unity";
    label[1] = (char)('0' + voices);
    if (velocity != 127)
        strcpy(&label[2], "_mul");
    Synth.printStats(Serial, label);
}

void setup()
{
    Serial.begin(19200);
    Serial.println("SynthBench: cycles per frame, polling mode");
    Synth.begin(SYNTH_POLL);
    Synth.setEnvelope(1, 1);
    bench(0, 127);
    for (int v = 1; v <= SYNTH_VOICES; v++) {
        bench(v, 127);
        bench(v, 100);
    }
    Synth.allOff();
    Serial.println("bench done");
}

void loop()
{
    Synth.update();
}
