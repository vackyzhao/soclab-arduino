# BlinkWithoutDelay

Blink two LEDs independently without `delay()`. LED1 changes state every 500 ms
(a 1-second full cycle); LED2 changes every 700 ms (a 1.4-second full cycle).
Both start low. Each timer uses unsigned elapsed-time subtraction so normal
32-bit `millis()` wrap does not stop the blinking.

- KR260 clean Lab3: SoC GPIO20 = UF1, GPIO21 = UF2.
- Course ZedBoard Lab3: GPIO20/21 = the extension board's D2/D3, not the
  ZedBoard's onboard LEDs.

Open **File > Examples > SocAudio > BlinkWithoutDelay**, select your matching
board and **Build: Board (real time)**, then upload. The example needs only
`Arduino.h`; no audio wiring or buttons are used. The two independent `if`
blocks leave the rest of `loop()` free for other work. Slow additional work
can delay an LED transition, so avoid long blocking operations there.

Expected behavior is two different blink rhythms; there is no serial output.
Compilation alone does not verify physical LED behavior.
