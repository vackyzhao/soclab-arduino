#include <SocAudio.h>
#include <math.h>

// Four 256-entry tables generated once in DMEM before playback starts.
int16_t waveformTable[4][256];

static const uint32_t PHASE_STEP =
    (uint32_t)((220ULL * 4294967296ULL) / SOC_AUDIO_RATE);
static const uint32_t SEGMENT_FRAMES = 2UL * SOC_AUDIO_RATE;
static uint32_t phase = 0;
static uint32_t frameInSegment = 0;
static uint8_t waveform = 0;  // sine, triangle, sawtooth, square

void setup() {
  for (int i = 0; i < 256; ++i) {
    waveformTable[0][i] =
        (int16_t)(6000.0f * sinf(6.28318530718f * i / 256.0f));

    // Triangle: 0 -> +6000 -> 0 -> -6000 -> 0.
    const int triangle = i < 64 ? i : (i < 192 ? 128 - i : i - 256);
    waveformTable[1][i] = (int16_t)(triangle * 6000 / 64);

    // The sharp shapes are intentionally not band-limited.
    waveformTable[2][i] = (int16_t)(i * 12000 / 256 - 6000);
    waveformTable[3][i] = i < 128 ? 6000 : -6000;
  }
  Audio.begin();
}

void loop() {
  for (;;) {
    const int16_t sample = waveformTable[waveform][phase >> 24];
    if (!Audio.write16(sample, sample)) {
      return;  // Retry without changing phase or waveform position.
    }

    phase += PHASE_STEP;
    if (++frameInSegment == SEGMENT_FRAMES) {
      frameInSegment = 0;
      waveform = (waveform + 1) & 3;
    }
  }
}
