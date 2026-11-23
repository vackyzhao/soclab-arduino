#include <Arduino.h>

constexpr uint32_t REPORT_INTERVAL_MS = 1000;
constexpr unsigned long RANDOM_SEED = 12345;

uint32_t lastReportMs = 0;
uint32_t lastCycles = 0;
uint8_t flags = 0;

void setup() {
  Serial.begin(115200);
  randomSeed(RANDOM_SEED); // Reproduce the same sequence after each startup.
  Serial.println("TimingAndRandom: one report per second, seed=12345");
  Serial.println("raw=-50..150; clipped=0..100; mapped=0..255");
  Serial.println("flags: bit0=clipped, bit1=mapped>=128, bit2=heartbeat");
  lastReportMs = millis();
  lastCycles = cycles();
}

void loop() {
  uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastReportMs) < REPORT_INTERVAL_MS) {
    return;
  }
  lastReportMs = nowMs;

  uint32_t nowUs = micros();
  uint32_t nowCycles = cycles();
  uint32_t elapsedCycles = nowCycles - lastCycles;
  lastCycles = nowCycles;

  // The upper bound of random(min, max) is excluded.
  long raw = random(-50L, 151L);
  long clipped = constrain(raw, 0L, 100L);
  long mapped = map(clipped, 0L, 100L, 0L, 255L);

  bitWrite(flags, 0, raw != clipped);
  bitWrite(flags, 1, mapped >= 128);
  bitToggle(flags, 2);

  Serial.print("ms=");
  Serial.print(nowMs);
  Serial.print(" us=");
  Serial.print(nowUs);
  Serial.print(" cycles_low32=");
  Serial.print(nowCycles);
  Serial.print(" delta_cycles=");
  Serial.print(elapsedCycles);
  Serial.print(" raw=");
  Serial.print(raw);
  Serial.print(" clipped=");
  Serial.print(clipped);
  Serial.print(" mapped=");
  Serial.print(mapped);
  Serial.print(" flags=0b");
  Serial.print(flags, BIN);
  Serial.print(" clipped_bit=");
  Serial.println(bitRead(flags, 0));
}
