# NonBlockingMelody

Open **File > Examples > SocAudio > NonBlockingMelody**. This is a repeating
single-voice melody with explicit rests. `millis()` schedules the notes;
`Synth.begin(SYNTH_IRQ)` supplies PCM frames through the FIFO-low interrupt.
The main loop uses no `delay()` and advances at most one step per call. If
another job holds up the loop, the next step starts when the loop resumes;
it does not play a burst of overdue notes.

Each note releases after 75% of its time slot. The attack/release envelope
softens its edges. `LED_BUILTIN` is high while the note is held and low during
the release gap or a rest. The LED reports the sequencer state, not measured
audio output. Change the `{MIDI note, milliseconds}` table to edit the melody;
note 0 is reserved for a rest in this example.

Select **Build: Board (real time)**. Every supported Synth voices setting,
including **1**, works because only voice 0 is used. The matching Lab3 FPGA
must have the audio FIFO, sigma-delta DAC and FIFO-low IRQ connection. Audio
uses `DAC_LEFT`/`DAC_RIGHT`'s existing hardware outputs, through the board's
appropriate filter/amplifier path; it does not come from `LED_BUILTIN`.
On the KR260 clean Lab3 mapping the LED is UF1; other targets use their BSP
mapping. Start the listening equipment at low volume and adjust as needed.

Expected result: a short rising/falling sine melody repeats, separated by
rests, while the LED follows its held notes. Compilation alone does not
confirm physical sound, wiring or LED polarity on another FPGA design.
