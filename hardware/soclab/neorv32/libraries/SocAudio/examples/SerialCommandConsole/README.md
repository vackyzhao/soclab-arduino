# SerialCommandConsole

A small 115200 baud, 8N1 LED console using a fixed 48-byte line buffer.
It accepts CR, LF, or CRLF line endings, and supports backspace. Commands are
lowercase and use exactly the spaces shown:

```text
help
status
led1 on
led1 off
led1 toggle
led2 on
led2 off
led2 toggle
```

For example, `led1 on` replies with a line such as
`led1=on led2=off uptime_ms=1234 rx_overruns=0`, then `> `.
The LED fields report the output state written by the sketch, not a physical
voltage measurement. GPIO20/21 are KR260 clean Lab3 UF1/UF2, or the course
ZedBoard extension board's D2/D3; they are not the ZedBoard onboard LEDs.

Open **File > Examples > SocAudio > SerialCommandConsole**. Select the matching
board and **Build: Board (real time)**, upload, then open the NEORV32 UART
terminal at 115200. On KR260 this UART is reached through the PS `/dev/ttyS0`
path; a remote OpenOCD address does not create a Windows COM port. Close the
terminal before a UART-loader upload and reopen it afterward.

A line longer than 47 characters is discarded in full up to its line ending,
with one `ERR: line too long; discarded` reply. Its suffix is never treated as
a new command. There is no dynamic `String`, `parseInt()`, button input, or
input-pullup assumption. Type commands one line at a time: the core's default
RX ring holds 63 characters, and a large pasted block can overrun it while
responses are transmitted. `status` includes the core's overrun counter.

Compilation verifies the supported API usage; interactive UART and visible
LED results must be checked on the selected hardware.
