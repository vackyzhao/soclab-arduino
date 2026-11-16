// PinInterrupt - attachInterrupt() on gpio_i[22] = PIN_AUDIO_LOW (audio FIFO
// level < FIFO_LOW = 64), the only input pin the Lab 3 SoC connects. Bursts
// of 128 silent frames move the level across the threshold:
//   RISING   attached after a burst; the DAC drains the FIFO (no writes, the
//            level only falls): exactly one rising edge
//   FALLING  attached with an empty FIFO; a burst crosses 64 upwards: at least
//            one falling edge (a DAC read right after the crossing write can
//            make the level hover: fifo_low has no hysteresis)
//   CHANGE   attached after a burst (pin 0: armed for the rising edge); drain:
//            one edge, then the dispatcher re-arms it for the falling edge
// The callback is in DMEM (RAMFUNC); from flash each interrupt would cost an
// XIP cache refill (~22 us, one DAC frame). Between the bursts the FIFO runs
// empty on purpose (tb_soc: allow_stops=1).
#include <SocAudio.h>

static volatile uint32_t edges = 0;

static void RAMFUNC onEdge()
{
    edges = edges + 1;
}

static const int irq = digitalPinToInterrupt(PIN_AUDIO_LOW);

static void burst()
{
    for (int i = 0; i < 128; i++)
        Audio.write(0, 0);
}

static void drain()
{
    while (Audio.level() > 0)     // the DAC takes one frame per 20.8 us
        ;
    uint32_t t0 = cycles();
    while (cycles() - t0 < 9830u)   // 100 us for the interrupt
        ;
}

static void report(const char *mode, uint32_t n, bool ok)
{
    Serial.print(mode);
    Serial.print(ok ? " ok" : " FAIL");
    Serial.print(" edges ");
    Serial.print(n);
    Serial.print(" pin ");
    Serial.println(digitalRead(PIN_AUDIO_LOW));
}

void setup()
{
    Serial.begin(19200);
    Serial.println("PinInterrupt: RISING, FALLING, CHANGE on PIN_AUDIO_LOW");
    Audio.begin(64);
    pinMode(PIN_AUDIO_LOW, INPUT);
    Serial.print("pin ");
    Serial.println(digitalRead(PIN_AUDIO_LOW));   // 1: empty FIFO, level < 64

    uint32_t e0;
    burst();
    attachInterrupt(irq, onEdge, RISING);
    e0 = edges;
    drain();
    report("RISING", edges - e0, edges - e0 == 1);

    detachInterrupt(irq);
    attachInterrupt(irq, onEdge, FALLING);
    e0 = edges;
    burst();
    drain();
    report("FALLING", edges - e0, edges - e0 >= 1);

    detachInterrupt(irq);
    burst();
    attachInterrupt(irq, onEdge, CHANGE);
    e0 = edges;
    drain();
    bool armed_falling = (NEORV32_GPIO->IRQ_POLARITY & (1u << PIN_AUDIO_LOW)) == 0;
    report("CHANGE", edges - e0, edges - e0 == 1 && armed_falling);

    detachInterrupt(irq);
    Audio.end();
    Serial.println("done");
}

void loop()
{
}
