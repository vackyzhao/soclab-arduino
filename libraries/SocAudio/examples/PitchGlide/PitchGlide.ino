#include <SocAudio.h>

static const float lowHz = 220.0f;
static const float highHz = 880.0f;
static const float stepHz = 2.2f;
static const uint32_t updateMs = 10;
static const uint32_t pauseMs = 700;  // includes the release tail

static float frequencyHz = lowHz;
static int8_t direction = 1;
static bool paused = false;
static uint32_t lastUpdate = 0;
static uint32_t pauseStarted = 0;

static void startGlide(uint32_t now)
{
    frequencyHz = lowHz;
    direction = 1;
    paused = false;
    lastUpdate = now;
    Synth.noteOnHz(0, frequencyHz, 100);  // one trigger for the whole up/down sweep
    digitalWrite(LED_BUILTIN, HIGH);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("PitchGlide: 220 -> 880 -> 220 Hz; pitch updates every 10 ms");
    pinMode(LED_BUILTIN, OUTPUT);
    Synth.begin(SYNTH_IRQ, 128);
    Synth.setVolume(64);
    Synth.setEnvelope(150, 250);
    startGlide(millis());
}

void loop()
{
    const uint32_t now = millis();
    if (paused) {
        if (now - pauseStarted >= pauseMs) {
            // Once per sweep, not on every 10 ms pitch update.
            Serial.println("Restarting glide");
            startGlide(millis());
        }
        return;
    }
    if (now - lastUpdate < updateMs)
        return;
    lastUpdate = now;

    frequencyHz += direction * stepHz;
    if (frequencyHz >= highHz) {
        frequencyHz = highHz;
        direction = -1;
    } else if (frequencyHz <= lowHz) {
        frequencyHz = lowHz;
        Synth.setFrequency(0, frequencyHz);
        Synth.noteOff(0);
        digitalWrite(LED_BUILTIN, LOW);
        paused = true;
        pauseStarted = now;
        return;
    }

    // Changes phase increment without resetting phase or retriggering attack.
    Synth.setFrequency(0, frequencyHz);
}
