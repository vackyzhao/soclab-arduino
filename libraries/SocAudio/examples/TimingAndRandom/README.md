# TimingAndRandom

Print one bounded status line each second at 115200 baud. It demonstrates
`millis()`, `micros()`, the SoC `cycles()` extension, `randomSeed()` / `random()`,
`constrain()`, `map()`, and `bitWrite()` / `bitToggle()` / `bitRead()`.
No LEDs, ADC, buttons, or audio connection are required.

Open **File > Examples > SocAudio > TimingAndRandom**, select the matching
board with **Build: Board (real time)**, upload, then observe the NEORV32 UART
at 115200, 8N1. On KR260 it is reached through the PS `/dev/ttyS0`, rather than
automatically appearing as a Windows COM port. Close that terminal during a
UART-loader upload. This sketch uses only the existing core API.

Each report contains:

- `ms` and `us`: current 32-bit timer values.
- `cycles_low32`: the low 32 bits of the CPU cycle counter.
- `delta_cycles`: unsigned cycle difference from the preceding report, roughly
  98,304,000 for 1 second on this 98.304 MHz core, plus scheduling variation.
- `raw`: a deterministic pseudorandom value from -50 through 150. Both bounds
  are possible because the call uses an excluded upper bound of 151.
- `clipped`: `raw` constrained to 0..100; `mapped`: that value mapped to 0..255
  with integer arithmetic. `map()` itself does not clamp values.
- `flags`: binary bits, with bit0 indicating clipping, bit1 indicating a mapped
  value of at least 128, and bit2 toggling once per report. `clipped_bit` reads
  back bit0 using the helper macro. Binary printing omits leading zeroes.

The seed is fixed at 12345, so restarting the same core implementation repeats
the `raw` sequence. This is a reproducible software generator, not analog noise
or an ADC reading; timer fields naturally differ with startup and UART timing.

`cycles()` wraps about every **43.7 seconds** at 98.304 MHz. Subtracting two
unsigned readings works across a wrap only when the actual interval is shorter
than one complete wrap. `micros()` wraps about every 71.6 minutes, `millis()`
about every 49.7 days. The once-per-second scheduling uses wrap-safe elapsed
subtraction without calling `delay()`. Printing the bounded status line still
takes UART transmission time.

Compilation verifies API usage. The exact counter values and physical UART
output have not been measured for this example.
