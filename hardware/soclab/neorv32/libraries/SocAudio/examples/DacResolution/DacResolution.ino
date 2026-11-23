#include <SocAudio.h>

// DAC pins are the existing sigma-delta audio outputs, not GPIO PWM pins.
// Use an external low-pass filter to observe their average level.
// This sketch generates DC steps, not a tone. Do not run Synth concurrently.
static const uint8_t resolutions[] = {8, 12, 24};
static uint8_t resolutionIndex = 0;
static uint8_t levelIndex = 0;  // 0, 1, 2, 3, 4 -> 0, 25, 50, 75, 100%
static uint32_t changedAt = 0;

static void applyLevel()
{
    const uint8_t bits = resolutions[resolutionIndex];
    const uint32_t maximum = (1UL << bits) - 1UL;
    const uint32_t left = (maximum * levelIndex + 2UL) / 4UL;
    const uint32_t right = maximum - left;

    analogWriteResolution(bits);
    analogWrite(DAC_LEFT, (int)left);
    analogWrite(DAC_RIGHT, (int)right);

    Serial.print(bits);
    Serial.print(" bit, left ");
    Serial.print(levelIndex * 25);
    Serial.print("% = ");
    Serial.print(left);
    Serial.print(" / ");
    Serial.print(maximum);
    Serial.print(", right = ");
    Serial.println(right);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("DacResolution: static DC steps; external low-pass filter required");
    applyLevel();
    changedAt = millis();
}

void loop()
{
    const uint32_t now = millis();
    if (now - changedAt < 2000)
        return;
    changedAt = now;

    if (++levelIndex > 4) {
        levelIndex = 0;
        resolutionIndex = (resolutionIndex + 1) % 3;
    }
    applyLevel();
}
