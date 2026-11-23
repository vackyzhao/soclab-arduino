#include <Arduino.h>

// SoC GPIO numbers, not header positions:
// KR260 clean Lab3: UF1 / UF2; ZedBoard Lab3: extension LEDs D2 / D3.
constexpr uint8_t LED1 = 20;
constexpr uint8_t LED2 = 21;
constexpr uint32_t LED1_INTERVAL_MS = 500;
constexpr uint32_t LED2_INTERVAL_MS = 700;

uint32_t lastLed1Ms = 0;
uint32_t lastLed2Ms = 0;
uint8_t led1State = LOW;
uint8_t led2State = LOW;

void setup() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, led1State);
  digitalWrite(LED2, led2State);
  lastLed1Ms = millis();
  lastLed2Ms = lastLed1Ms;
}

void loop() {
  uint32_t now = millis();

  // Unsigned subtraction still works when the 32-bit timer wraps.
  if ((uint32_t)(now - lastLed1Ms) >= LED1_INTERVAL_MS) {
    lastLed1Ms = now;
    led1State = led1State == LOW ? HIGH : LOW;
    digitalWrite(LED1, led1State);
  }

  if ((uint32_t)(now - lastLed2Ms) >= LED2_INTERVAL_MS) {
    lastLed2Ms = now;
    led2State = led2State == LOW ? HIGH : LOW;
    digitalWrite(LED2, led2State);
  }

  // Other work can run here: neither LED uses delay().
}
