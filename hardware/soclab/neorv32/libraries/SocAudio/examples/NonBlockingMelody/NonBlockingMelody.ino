#include <SocAudio.h>

// One voice is enough. 0 marks a rest; the other values are MIDI notes.
struct MelodyStep {
    uint8_t note;
    uint16_t durationMs;
};

static const MelodyStep melody[] = {
    {60, 300}, {62, 300}, {64, 300}, {0, 180},
    {64, 300}, {65, 300}, {67, 600}, {0, 400},
    {67, 300}, {65, 300}, {64, 300}, {62, 300},
    {60, 700}, {0, 700}
};
static const uint8_t stepCount = sizeof(melody) / sizeof(melody[0]);
static uint8_t stepIndex = 0;
static uint32_t stepStarted = 0;
static bool released = false;

static void startStep(uint32_t now)
{
    stepStarted = now;
    released = melody[stepIndex].note == 0;
    if (released) {
        Synth.noteOff(0);
        digitalWrite(LED_BUILTIN, LOW);
    } else {
        Synth.noteOn(0, melody[stepIndex].note, 100);
        digitalWrite(LED_BUILTIN, HIGH);
    }
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    Synth.begin(SYNTH_IRQ, 128);
    Synth.setVolume(64);       // modest master volume, range 0..255
    Synth.setEnvelope(8, 50);  // attack and release in milliseconds
    startStep(millis());
}

void loop()
{
    const uint32_t now = millis();
    const uint32_t elapsed = now - stepStarted;  // also works across millis wrap
    const uint16_t duration = melody[stepIndex].durationMs;

    // Release after 75% of a note's slot, leaving a short gap before the next.
    if (!released && elapsed >= (uint32_t)duration * 3 / 4) {
        Synth.noteOff(0);
        digitalWrite(LED_BUILTIN, LOW);
        released = true;
    }
    if (elapsed >= duration) {
        stepIndex = (stepIndex + 1) % stepCount;
        startStep(now);
    }

    // Add serial commands or other short jobs here. No delay() or catch-up loop:
    // one call advances at most one melody step. The IRQ supplies audio.
}
