# StereoChannelTest

Upload this sketch to check the two audio channels. It repeats this eight-second sequence:

| Time in sequence | Left | Right |
| --- | --- | --- |
| 0–2 s | 440 Hz sine | Silence |
| 2–4 s | Silence | 660 Hz sine |
| 4–6 s | 440 Hz sine | 660 Hz sine |
| 6–8 s | Silence | Silence |

Both channels use signed 16-bit PCM at 48,000 frames/s, with a peak amplitude of 6000 (about 18% of full scale). `Audio.write16(left, right)` converts each sample to the peripheral's signed 24-bit format. The sine table has 256 entries and is calculated once into DMEM in `setup()`; phase accumulators select the table positions.

The sequence timer advances only when the FIFO accepts a frame. Playback contains no `delay()` or serial prints: the 256-frame FIFO holds only about 5.3 ms of audio. Keep calling `loop()` promptly. The DAC remains enabled during silence, receiving zero samples; the sketch does not repeatedly switch the DAC off.

Use the audio output connections for the selected board and bitstream, as described in the [SocAudio guide](../../README.md). A DAC output needs the appropriate reconstruction filter and audio output circuit. This sketch generates the samples; it does not read an audio file and does not use `Synth`.

To change the test, edit `LEFT_STEP`, `RIGHT_STEP`, or `SEGMENT_FRAMES`. The phase-step expression is `frequency * 2^32 / 48000`. Set an inactive channel to signed PCM zero, not `32768`.
