# BreathingLED

Open **File > Examples > SocAudio > BreathingLED** in Arduino IDE. This example
uses the Arduino core only; it does not require an audio output connection.

For the KR260 clean Lab3 hardware, SoC GPIO20 drives UF1 and GPIO21 drives UF2.
The sketch creates 500 Hz software PWM with `micros()` and `digitalWrite()`.
One duty cycle increases while the other decreases, taking 2 seconds in each
direction and 4 seconds for a full cycle. The duty ramp is linear, without
brightness correction. These GPIOs do not have a hardware PWM implementation;
ordinary GPIO `analogWrite()` uses a digital threshold instead.

Select **KR260**, **Build: Board (real time)**, and
**Upload method: UART 115200 through KR260 SSH**, then click **Upload**.
This requires the matching `lab3_kr260_uart` FPGA app, the configured SSH target
in `boards.local.txt`, and the PS `/dev/ttyS0` path. The `lab3-openocd` and
`lab3-jtag` services must be stopped before UART upload because they share the
CPU-reset MMIO lock. The native loader starts the application automatically;
no manual GDB `run` is needed. See [upload setup](../../../../docs/UPLOAD.md).

The sketch prints one `.` per second on `Serial` at 115200 baud. A UART terminal
must be closed during upload; after upload, it can independently observe these
markers. GDB/OpenOCD is also supported with the matching target and services.

On 2026-11-17, this sketch compiled to 5208 program bytes and 680 static RAM bytes.
UART-over-SSH upload passed the native length/checksum check and started at
`0xE0000000`. A separate 6-second UART observation, without reset or debugger,
received six dots spaced 1 second apart. The new GDB recipe was separately
tested with the same sketch and the same continued-execution observation.
These results confirm upload and continued CPU execution. The user subsequently
reported normal LED breathing behavior; the 500 Hz waveform and analog voltages
have not been measured with instruments.
See [validation details](../../../../docs/VALIDATION.md).

The new upload menus and tool are in the current working tree and have not yet
been published in a new Boards Manager release. Local COM upload remains
untested.
