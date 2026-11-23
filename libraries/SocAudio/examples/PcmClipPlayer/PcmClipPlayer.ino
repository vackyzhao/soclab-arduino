#include <SocAudio.h>
#include <string.h>
#include "clip_samples.h"

// This 80 ms clip occupies 7680 bytes of DMEM. Copying before playback avoids
// reading the larger constant asset through the ZedBoard's 1 KiB XIP cache.
int16_t clipRam[CLIP_SAMPLE_COUNT];

static uint32_t clipPosition = 0;
static uint32_t silencePosition = 0;
static bool playingClip = true;
static const uint32_t SILENCE_FRAMES = SOC_AUDIO_RATE;  // one second

void setup() {
  memcpy(clipRam, clipSamples, sizeof(clipRam));
  Audio.begin();
}

void loop() {
  for (;;) {
    const int16_t sample = playingClip ? clipRam[clipPosition] : 0;
    if (!Audio.write16(sample, sample)) {
      return;  // Full FIFO: do not skip a clip sample or a silence frame.
    }

    if (playingClip) {
      if (++clipPosition == CLIP_SAMPLE_COUNT) {
        clipPosition = 0;
        silencePosition = 0;
        playingClip = false;
      }
    } else if (++silencePosition == SILENCE_FRAMES) {
      silencePosition = 0;
      playingClip = true;
    }
  }
}
