// Reconstructed from course blinky.elf: GPIO20/21 together, 250 ms per state.
#include <Arduino.h>

static bool gpioReady = false;

void setup()
{
    Serial.begin(19200);
    Serial.println("=> Setting up GPIOs");
    gpioReady = neorv32_gpio_available() != 0;
    if (!gpioReady) {
        Serial.println("Error! No GPIO unit synthesized!");
        return;
    }
    pinMode(20, OUTPUT);
    pinMode(21, OUTPUT);
    digitalWrite(20, HIGH);
    digitalWrite(21, HIGH);
    Serial.println("LED blinky demo");
}

void loop()
{
    if (!gpioReady)
        return;
    delay(250);
    digitalToggle(20);
    digitalToggle(21);
}
