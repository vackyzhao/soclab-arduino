# PitchGlide

Open **File > Examples > SocAudio > PitchGlide**. A single sine voice rises
from 220 Hz to 880 Hz and falls back to 220 Hz. The main loop changes its
frequency by 2.2 Hz approximately every 10 ms, taking about three seconds
in each direction. `Synth.setFrequency(0, hz)` changes pitch without resetting
phase or restarting the attack envelope at each step.

There is one `noteOnHz` for the complete up/down sweep, a 150 ms attack, and
a `noteOff` with a 250 ms release at its end. A 700 ms pause, including that
release, precedes the next sweep. `LED_BUILTIN` is high during the sweep and
low during the release/pause. Serial runs at **115200 8N1** and prints only
at startup and once per sweep. Audio is supplied by the FIFO-low interrupt;
no polling refill or `delay()` is required in `loop()`.

Select **Build: Board (real time)**. This uses voice 0 and supports every Synth
voices setting, including 1. It needs the matching Lab3 audio FIFO/DAC/IRQ
hardware and the board's existing filtered audio output and suitable amplifier
or powered listening equipment. On KR260, Serial reaches the PS-side UART
transport configured for the board; an OpenOCD GDB connection is a separate
interface and does not itself provide an Arduino serial monitor port.

Expected result: an up/down pitch glide with a soft start/end and a pause.
Updates occur in 10 ms steps; this is a sine oscillator pitch example rather
than sample playback. Serial output and sound depend on the actual connected
target and output path; compilation is not a physical-audio measurement.
