# SynthSerialControl

Open **File > Examples > SocAudio > SynthSerialControl**, select
**Build: Board (real time)**, and connect the target's actual UART transport
at **115200 baud, 8N1**. Send individual characters; a line ending is not
required. A terminal that buffers text until Enter will apply its buffered
commands together. There is no USB CDC device implemented by this sketch.
On KR260 use the configured PS-side UART or its serial forwarding route;
GDB/OpenOCD's port is not a serial terminal. Close a terminal that owns this
UART before using it for a bootloader upload.

| Key | Action |
| --- | --- |
| `1`..`8` | Hold C4, D4, E4, F4, G4, A4, B4 or C5 respectively |
| `+` / `-` | Raise/lower master volume by 8, clamped to 0..255 |
| Space | Release the note over 120 ms |
| `?` | Print voice count, volume, selected MIDI note, RX overrun count and Synth statistics |

Only voice 0 is used, so **Synth voices: 1** also works. A new note key
replaces/retriggers that voice with an 8 ms attack. The startup volume is 64
and velocity is 100. The LED is high while a note is commanded and low after
Space, even while its release tail is still audible. FIFO-low interrupts
feed audio; there is no `delay()` or explicit refill call in the main loop.

The core has a 64-byte RX ring by default (63 bytes usable); this sketch allocates no extra
line buffer. It consumes at most eight characters per loop, combines volume
changes within that batch, and prints requested status at most once every
250 ms. This is an interactive controller, not a bulk-data receiver. A long
burst or repeated status requests can still overrun RX buffering. `?` shows
`Serial.overruns()` so lost input is visible; the counter also records a
detected hardware FIFO overrun, so it is not an exact count of every lost byte.
Serial TX can wait for UART capacity while the audio IRQ keeps operating;
check the reported Synth stalls/load on the actual target under your workload.

Expected result: note keys produce sine tones, `+`/`-` alter their amplitude,
Space fades out, and `?` reports counters. This requires the matching Lab3
audio FIFO/DAC/IRQ and a working board-specific filter/amplifier/audio path.
Successful compilation does not establish UART wiring or physical audio.
