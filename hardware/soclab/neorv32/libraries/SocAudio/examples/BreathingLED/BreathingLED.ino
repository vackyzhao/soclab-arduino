#include <Arduino.h>

// KR260 clean Lab3: GPIO20 = UF1, GPIO21 = UF2.
// No hardware PWM on these GPIOs: 500 Hz software PWM.
constexpr uint8_t LED1 = 20, LED2 = 21;
constexpr uint32_t PWM_US = 2000, HALF_BREATH_US = 2000000;

void setup() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  Serial.begin(115200);
  Serial.println("KR260 breathing UF1/UF2, software PWM 500Hz");
}

void loop() {
  static uint32_t lastMicros = 0, breathUs = 0, pwmUs = 0, lastReport = 0;
  static uint8_t last1 = LOW, last2 = LOW;
  uint32_t now = micros(), elapsed = now - lastMicros;
  lastMicros = now;
  breathUs = (breathUs + elapsed) % (2 * HALF_BREATH_US);
  pwmUs = (pwmUs + elapsed) % PWM_US;
  uint32_t ramp = breathUs < HALF_BREATH_US
                      ? breathUs : 2 * HALF_BREATH_US - breathUs;
  uint32_t duty = ramp / (HALF_BREATH_US / PWM_US);
  uint8_t state1 = pwmUs < duty ? HIGH : LOW;
  uint8_t state2 = pwmUs < (PWM_US - duty) ? HIGH : LOW;
  if (state1 != last1) { digitalWrite(LED1, state1); last1 = state1; }
  if (state2 != last2) { digitalWrite(LED2, state2); last2 = state2; }
  if (now - lastReport >= 1000000UL && Serial.availableForWrite()) {
    lastReport = now;
    Serial.write('.'); // One byte per second for independent run verification.
  }
}
