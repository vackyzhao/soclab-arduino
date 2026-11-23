# WaveformPlayer

The sketch repeats four two-second segments: **sine → triangle → sawtooth → square**, all at approximately 220 Hz on both audio channels. This makes their different harmonic content audible. All four tables have a peak magnitude of 6000, about 18% of signed 16-bit full scale; equal peak amplitude does not mean equal perceived loudness or RMS level.

The format is mono signed 16-bit PCM, sent to both channels as stereo frames at 48,000 frames/s. `Audio.write16(sample, sample)` converts to the peripheral's signed 24-bit samples. Four 256-entry tables occupy 2048 bytes of DMEM and are prepared before `Audio.begin()`. Playback uses table reads and a phase accumulator, with no floating-point waveform calculations in `loop()`.

This is a basic sampled-waveform demonstration. The sawtooth and square have sharp transitions, and the triangle also has harmonics above its fundamental. The tables and lookup method do **not** include antialiasing or band-limited waveform synthesis. Harmonics above the 24 kHz Nyquist frequency can alias into the audible band. The analog reconstruction filter cannot remove aliases already folded into that band. Changing waveform can also produce a short click. These limitations matter when changing pitch or using this as a musical oscillator.

Both phase and segment length advance only after `Audio.write16()` returns success. The FIFO holds about 5.3 ms; do not add long delays or blocking serial output to playback. The sketch uses direct PCM output and does not call `Synth`.

See the [SocAudio guide](../../README.md) for the output connections matching your board and bitstream. To change pitch, edit `PHASE_STEP` using `frequency * 2^32 / 48000`; to change segment length, edit `SEGMENT_FRAMES`.
