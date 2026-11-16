#include <SocAudio.h>

void setup() {
  Synth.begin();
  Synth.setVolume(128);
  Synth.noteOnHz(0, 440.0f);
}

void loop() {}
