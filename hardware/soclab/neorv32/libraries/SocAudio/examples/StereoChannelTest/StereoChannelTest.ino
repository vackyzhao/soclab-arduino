#include <SocAudio.h>
#include <math.h>

// Built once in setup, in DMEM rather than the ZedBoard's XIP flash.
int16_t sineTable[256];

static const uint32_t LEFT_STEP =
    (uint32_t)((440ULL * 4294967296ULL) / SOC_AUDIO_RATE);
static const uint32_t RIGHT_STEP =
    (uint32_t)((660ULL * 4294967296ULL) / SOC_AUDIO_RATE);
static const uint32_t SEGMENT_FRAMES = 2UL * SOC_AUDIO_RATE;

static uint32_t leftPhase = 0;
static uint32_t rightPhase = 0;
static uint32_t frameInSegment = 0;
static uint8_t segment = 0;

void setup() {
  // Peak 6000 / 32768: about 18% of signed 16-bit full scale.
  for (unsigned i = 0; i < 256; ++i) {
    sineTable[i] = (int16_t)(6000.0f * sinf(6.28318530718f * i / 256.0f));
  }
  Audio.begin();
}

void loop() {
  for (;;) {
    const bool leftOn = segment == 0 || segment == 2;
    const bool rightOn = segment == 1 || segment == 2;
    const int16_t left = leftOn ? sineTable[leftPhase >> 24] : 0;
    const int16_t right = rightOn ? sineTable[rightPhase >> 24] : 0;

    if (!Audio.write16(left, right)) {
      return;  // FIFO full: retry this same frame next time.
    }

    // Only accepted frames advance time. No delay() or Serial in playback.
    leftPhase += LEFT_STEP;
    rightPhase += RIGHT_STEP;
    if (++frameInSegment == SEGMENT_FRAMES) {
      frameInSegment = 0;
      segment = (segment + 1) & 3;
    }
  }
}
