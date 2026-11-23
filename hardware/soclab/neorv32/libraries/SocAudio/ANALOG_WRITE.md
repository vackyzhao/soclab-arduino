# Static levels with analogWrite

```cpp
#include <SocAudio.h>

void setup() {
  analogWrite(DAC_LEFT, 128);   // default 8 bits: about 50.196% ones
  analogWrite(DAC_RIGHT, 255);  // maximum positive PCM: nearly 100% ones
}
void loop() {}
```

`DAC_LEFT` and `DAC_RIGHT` are virtual Arduino pin numbers for the existing
left/right sigma-delta DAC. They do not select ordinary GPIO pins. On the
KR260 clean Lab3 mapping they are PMOD1/J2 physical pins 1 and 3 respectively;
the course ZedBoard uses its existing audio outputs. Include `SocAudio.h` so
Arduino discovers the backend library. A Blink sketch does not link it.

`analogWriteResolution(bits)` selects 1..24 bits (values outside that interval
are clamped). The default is 8 bits, 0..255. `analogWrite` clamps its signed
integer value to `0..(2^bits-1)` and maps that range to signed 24-bit PCM
`-8388608..8388607`. Negative full scale has 0% ones, PCM zero has 50%, and
positive full scale has `1 - 1/16777216` ones. The exact 24-bit midpoint is
`analogWriteResolution(24); analogWrite(DAC_LEFT, 8388608);`.

These pins carry 3.3-V digital pulse streams, not buffered analog voltages.
An external low-pass filter is required for an average voltage. The voltage
after a DC-blocking capacitor will not retain this DC level. This API does
not validate or drive an analog load by itself. Ordinary GPIO pins have no
PWM hardware in this SoC and use a digital half-scale threshold instead.

The first DAC `analogWrite` stops Synth's interrupt/poll feed, clears its
queued audio and starts static-output mode. The unaddressed channel starts
at 0% ones. Further writes retain the other channel's last requested level.
Each call queues one stereo frame; after it is consumed, the RTL retains
the last two samples even when the FIFO is empty. No background refill is
needed. Rapid writes queue in order and can block for one sample interval
(about 20.8 microseconds) when the FIFO is full.

`Audio.begin()`, `Audio.end()`, `Audio.write()` and `Synth.begin()` leave this
static mode. `Audio.write()` or `Synth.begin()` discard pending static frames.
The next `analogWrite` takes ownership again, clears queued PCM, and starts
the other channel at 0% ones. `tone()` starts Synth when needed, so it also
takes ownership from static output. Do not use these APIs simultaneously
from different interrupt handlers.

See **File > Examples > SocAudio > AnalogWriteStereo** for opposite-direction
steps on the two channels. The stable output is one audio sample tick after
the new frame reaches the FIFO head; calls are not immediate GPIO edges.
