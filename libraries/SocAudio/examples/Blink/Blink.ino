// Blink - both LEDs (D2 = LED_BUILTIN, D3) toggle every 250 ms.
// Same behaviour as the course blinky (banner, LEDs on, toggle), so the SIM
// build also passes the blinky checks of tb_soc (checks=blinky).

void setup()
{
    Serial.begin(19200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_D3, OUTPUT);
    Serial.println("LED blinky demo");
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(LED_D3, HIGH);
}

void loop()
{
    delay(250);
    digitalToggle(LED_BUILTIN);
    digitalToggle(LED_D3);
}
