# SocAudio

An Arduino library for the SoCLab NEORV32 platform: a stereo PCM FIFO driver and a small sine synthesizer.

## Install

Install the SoCLab NEORV32 board package from [soclab-arduino](https://github.com/vackyzhao/soclab-arduino), then select its board in Arduino IDE. That package includes SocAudio. For a separate library ZIP, the ZIP must contain this `SocAudio/` folder; use **Sketch > Include Library > Add .ZIP Library**. Keep only one active SocAudio installation when updating it.

The library requires this platform's `Arduino.h`, `arduino_internal.h`, DMEM execution support, and GPIO 22 FIFO-low interrupt. `architectures=neorv32` does not imply compatibility with every NEORV32 SoC. The matching hardware has a 256-frame FIFO at `0xFFD00000`, signed 24-bit stereo samples, and 48 kHz playback. The KR260 Lab 3 target preserves that interface.

## First sound

```cpp
#include <SocAudio.h>

void setup() {
  Synth.begin();
  Synth.noteOnHz(0, 440.0f);
}

void loop() {}
```

This produces a continuous 440 Hz sine through the hardware audio output. It does not play a WAV/MP3 file. `Synth` generates samples; `Audio` accepts samples supplied by your program. The output connector and external DAC/filter wiring depend on the FPGA target.

## API

| Call | Behavior |
| --- | --- |
| `Audio.begin(lowWater = 0)` | Reset the FIFO and enable the DAC. |
| `Audio.write(left, right)` / `Audio.write(mono)` | Nonblocking signed 24-bit PCM frame; returns 1 on success, 0 when full. Values are clamped to the 24-bit range. |
| `Audio.write16(left, right)` | Expand signed 16-bit PCM to the 24-bit range. |
| `Audio.level()` / `availableForWrite()` | Used/free frames in the 256-frame FIFO. |
| `Audio.full()` / `empty()` / `low()` | FIFO state. |
| `Audio.setLowWater(n)` | Set the low threshold; clamped to 16..224 while interrupt synth mode runs. |
| `Synth.begin(SYNTH_IRQ, 128)` | Start interrupt-driven synthesis; the default mode. |
| `Synth.begin(SYNTH_POLL)` / `Synth.update()` | Polling refill; call `update()` often enough to keep the FIFO fed. The BSP also calls it while `delay()` waits. |
| `Synth.noteOn(voice, midiNote, velocity = 127)` | Start a MIDI-pitched sine voice. |
| `Synth.noteOnHz(voice, hz, velocity = 127, duration_ms = 0)` | Start a frequency; zero duration sustains until stopped. |
| `Synth.noteOff(voice)` / `allOff()` | Release one/all voices. |
| `Synth.setFrequency(voice, hz)` | Change pitch without restarting the envelope. |
| `Synth.setEnvelope(attack_ms, release_ms)` | Linear attack/release; default 5/20 ms, quantized to 16-frame blocks. |
| `Synth.setVolume(0..255)` | Set master amplitude. Each voice is scaled by the configured voice count to avoid clipping when summed. |
| `Synth.stats()` / `printStats(Serial, label)` | Read or print render/interrupt timing and refill counts. |
| `Synth.end()` | Release voices and stop rendering, leaving the DAC enabled at silence. |
| `Audio.end()` | Stop synthesis, reset the FIFO, and disable the DAC. Disabling the DAC can create a pop. |

Use `Synth.isPlaying(voice)`, `frequencyMilliHz(voice)`, and `resetStats()` for playback/status control. The full declarations and sample units are in [`src/SocAudio.h`](src/SocAudio.h).

Do not mix direct `Audio.write()` calls with a running `Synth`: they share one FIFO. `tone()`/`noTone()` use synth voice 0 and the audio output; their pin argument does not select a GPIO square-wave output. Include `SocAudio.h` to link that backend.

## Examples

Open **File > Examples > SocAudio** after installation.

| Example | Purpose |
| --- | --- |
| `SimpleTone` | Minimal continuous 440 Hz sine with the default interrupt synth. |
| `SinePolling` | Write a 109-sample sine table directly to both channels (about 440.37 Hz). |
| `ToneScale` | Play a scale using the interrupt synth and print timing statistics. |
| `Chord` | Mix three/four sine voices. Use at least 4 synth voices. |
| `FullScale` | Check two-voice peak amplitude. Select **Synth voices: 2** for the stated full-scale result. |
| `SynthBench` | Measure polling render cost at different active voice counts. |
| `SerialSynth` | Exercise UART input/output while the synth runs. |
| `PinInterrupt` | Exercise FIFO-low GPIO interrupt modes; intentionally emits silent frames. |
| `Blink` | Basic board LEDs and timing, using the core API. |
| `SerialHello` | Basic UART and `Print` formatting, using the core API. |

`SYNTH_VOICES` defaults to 4 and can be set to 1..16 using the board menu. A sketch-only `#define SYNTH_VOICES` does not configure the separately compiled library. The voice count divides the maximum amplitude per voice; use the appropriate count when comparing loudness.

## Source and license status

The existing implementation and nine original examples derive from the user's Arduino adaptation in `audio-soc-firmware`, commit `9fb00bc` (2026-11-16), without functional changes. `SimpleTone` was added for this distribution. No ArduinoCore-API source or third-party driver implementation is bundled in this library folder. Metadata and this README were prepared for the independent distribution.

The source snapshot has no explicit license grant for the original adaptation. This distribution preserves that status and does not assign an MIT/BSD license. The separate board package preserves the applicable NEORV32 upstream notices and license texts; see the repository's `LICENSES/` directory.
