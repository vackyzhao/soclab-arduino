# DacResolution

Open **File > Examples > SocAudio > DacResolution** and select
**Build: Board (real time)**. This demonstrates `analogWriteResolution(8)`,
`analogWriteResolution(12)` and `analogWriteResolution(24)` on the existing
`DAC_LEFT` / `DAC_RIGHT` sigma-delta outputs. `SocAudio.h` is included so the
Arduino build links their backend. The sketch does not start Synth or feed
PCM audio concurrently.

For each resolution, the left channel steps through approximately 0%, 25%,
50%, 75% and 100% of the unsigned range. Each level lasts two seconds.
The right channel uses the complementary code `maximum - left`. After ten
seconds the next resolution begins; the complete sequence repeats every
30 seconds. The requested codes are rounded to the selected resolution:

| Bits | Maximum | Left codes |
| --- | --- | --- |
| 8 | 255 | 0, 64, 128, 191, 255 |
| 12 | 4095 | 0, 1024, 2048, 3071, 4095 |
| 24 | 16777215 | 0, 4194304, 8388608, 12582911, 16777215 |

Serial at **115200 8N1** prints the selected resolution and codes once per
step. These are software requests, not ADC measurements or calibrated
voltages. The outputs are digital pulse-density streams. With the matching
Lab3 DAC and an appropriate external low-pass filter, their settled average
levels should move in opposite directions. The two API calls queue stereo
frames in sequence, so the brief step transition is not a simultaneous
two-channel GPIO edge.

An AC-coupled audio path removes the steady DC component. A speaker path may
produce clicks at transitions rather than sustained tones; use a suitable
filter/buffer path for observing DC. Resolution describes the input-code
range, not a claim of 24-bit analog accuracy. Ordinary GPIO `analogWrite()`
uses a digital half-scale threshold on this SoC: it is not PWM and is not
the DAC demonstrated here. Pin locations and available analog circuitry
depend on the actual FPGA/board mapping. Compilation does not verify the
physical output levels.
