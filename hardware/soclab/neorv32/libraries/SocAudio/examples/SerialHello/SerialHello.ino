// SerialHello - Print formatting, a C++ static constructor, and an echo of
// every line received on Serial (interrupt-driven RX ring buffer).
// The constructor of the global object starts Serial (before setup(), from
// crt0's __init_array): main() must keep the UART RX interrupt enable it set,
// or the echo would never see a character.

struct Greeter {
    int value;
    Greeter() : value(42) { Serial.begin(19200); }
};
Greeter greeter;

void setup()
{
    Serial.println("SerialHello");
    Serial.print("ctor ");
    Serial.println(greeter.value);
    Serial.print("dec ");
    Serial.println(-12345);
    Serial.print("udec ");
    Serial.println(4000000000UL);
    Serial.print("hex ");
    Serial.println(0xBEEFUL, HEX);
    Serial.print("bin ");
    Serial.println(10, BIN);
    Serial.print("char ");
    Serial.println('A');
    Serial.print("float ");
    Serial.println(3.14159, 3);
    Serial.print("negfloat ");
    Serial.println(-2.5f, 1);
    Serial.print("map ");
    Serial.println(map(512, 0, 1023, 0, 255));
    Serial.print("constrain ");
    Serial.println(constrain(300, 0, 255));
    Serial.print("bool ");
    Serial.print(true);
    Serial.println(false);
    Serial.print("millis ");
    Serial.println(millis());
    Serial.println("send a line");
}

void loop()
{
    static char line[64];
    static int n = 0;
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c == '\n') {
            line[n] = '\0';
            Serial.print("echo: ");
            Serial.print(line);
            Serial.print(" (lost ");
            Serial.print(Serial.overruns());
            Serial.println(")");
            n = 0;
        } else if (c != '\r' && n < (int)sizeof(line) - 1) {
            line[n++] = (char)c;
        }
    }
}
