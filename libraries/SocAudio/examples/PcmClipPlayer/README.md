# PcmClipPlayer

This example plays a complete, finite **80 ms rising electronic chirp**, followed by one second of silence, then repeats. The chirp rises from 440 Hz to 880 Hz under a smooth envelope. It is an authored, digitally generated sample asset, not a recording and not a repeating single-cycle sine table.

`clip_samples.h` contains 3840 signed `int16_t` mono PCM samples at **48,000 samples/s**, with a peak magnitude below 6000 (about 18% of full scale). Playback duplicates each sample to the left and right channels using `Audio.write16(sample, sample)`. The Python standard-library script `generate_clip.py` documents the exact generation formula and regenerates the header:

```sh
python generate_clip.py
```

Python is only needed to regenerate the asset, not to compile or run the supplied sketch. The sample array is compiled into the program. There is no SD card, filesystem, WAV parser, or compressed-audio decoder in this sketch.

Before enabling the DAC, `setup()` copies the constant sample asset into the 7680-byte `clipRam` buffer in DMEM. This avoids contention between code and the larger asset in the ZedBoard's 1 KiB XIP cache during playback. Both course targets have 16 KiB DMEM: keep the clip short enough to leave room for the Arduino core, other global variables, and the stack. The constant asset also occupies 7680 bytes of program storage. Longer clips need a different buffering/storage design; enlarging this array indefinitely will not work.

The clip cursor and silence timer advance only when the FIFO accepts a frame. There are no blocking serial prints or delays during playback. A zero sample is sent throughout silence, keeping the DAC enabled. This example uses direct PCM and does not call `Synth`.

To substitute your own sound:

1. Resample it to **48 kHz mono** and convert it to signed 16-bit PCM. A WAV file's container header is not PCM sample data.
2. Replace the integers in `clipSamples` with the actual samples and set `CLIP_SAMPLE_COUNT` to their count. `CLIP_SAMPLE_RATE` must stay 48000 for this player; changing that constant does not resample or change the hardware rate.
3. Keep samples within `-32768..32767`, start at a moderate amplitude, and use a short fade at the beginning/end to reduce clicks. A byte-oriented source must be decoded into signed 16-bit sample values; the C array itself is not a stream of WAV bytes.
4. Check the compile report for DMEM usage before increasing the length. The RAM copy uses two bytes per mono sample, in addition to all other globals and stack needs.

Use the output connections for your board and bitstream in the [SocAudio guide](../../README.md), including the appropriate analog reconstruction/output circuit.

## Validation

On 2026-11-17 the unmodified sketch compiled for KR260 and ZedBoard: 11,228 program bytes and 8,080 static RAM bytes with four synth voices configured. A CPU/RTL simulation executed the KR260 binary on the NEORV32 and Lab3 audio RTL. All 3,840 clip frames and the following 128 silent frames matched at the DAC sample registers; 4,223 FIFO writes were also checked with zero mismatches. After the FIFO reached 128 frames, it stayed at 128..256 with no starvation. The simulation covered 83.234 ms and direct BRAM boot; it did not cover the full one-second pause/repeat, UART upload, ZedBoard XIP timing, or the external analog path.
