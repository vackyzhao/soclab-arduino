#include <SocAudio.h>

// No line parser or dynamic String is needed: each character is a command.
// The core's UART RX ring has 64 bytes (63 usable). Read at most 8 commands
// per loop and rate-limit requested status output to one report per 250 ms.
static const uint8_t notes[] = {60, 62, 64, 65, 67, 69, 71, 72};
static const uint8_t commandsPerLoop = 8;
static uint8_t masterVolume = 64;
static uint8_t selectedNote = 0;
static bool reportRequested = false;
static uint32_t lastReport = 0;

static void printStatus()
{
    Serial.print("voices=");
    Serial.print(SYNTH_VOICES);
    Serial.print(" volume=");
    Serial.print(masterVolume);
    Serial.print(" note=");
    if (selectedNote != 0)
        Serial.print(selectedNote);
    else
        Serial.print("off");
    Serial.print(" rx_overruns=");
    Serial.println(Serial.overruns());
    Synth.printStats(Serial, "serial-control");
}

void setup()
{
    Serial.begin(115200);
    Serial.println("SynthSerialControl: 115200 8N1, single-character commands");
    Serial.println("1..8: C4 D4 E4 F4 G4 A4 B4 C5; +/-: volume; space: release; ?: stats");
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Synth.begin(SYNTH_IRQ, 128);
    Synth.setVolume(masterVolume);
    Synth.setEnvelope(8, 120);
    Synth.resetStats();
}

void loop()
{
    bool volumeChanged = false;
    for (uint8_t count = 0; count < commandsPerLoop && Serial.available() > 0; ++count) {
        const int command = Serial.read();
        if (command >= '1' && command <= '8') {
            selectedNote = notes[command - '1'];
            Synth.noteOn(0, selectedNote, 100);  // voice 0 works with 1..16 voices
            digitalWrite(LED_BUILTIN, HIGH);
        } else if (command == ' ') {
            Synth.noteOff(0);
            selectedNote = 0;
            digitalWrite(LED_BUILTIN, LOW);
        } else if (command == '+') {
            masterVolume = masterVolume > 247 ? 255 : masterVolume + 8;
            volumeChanged = true;
        } else if (command == '-') {
            masterVolume = masterVolume < 8 ? 0 : masterVolume - 8;
            volumeChanged = true;
        } else if (command == '?') {
            reportRequested = true;
        }
        // CR/LF and unknown characters are ignored. No note timeout:
        // use space to release, or another note key to replace the pitch.
    }
    if (volumeChanged)
        Synth.setVolume(masterVolume);  // one sine-table update per batch

    const uint32_t now = millis();
    if (reportRequested && now - lastReport >= 250) {
        reportRequested = false;
        lastReport = now;
        printStatus();
    }
    // IRQ playback continues while the main loop handles keys or prints.
}
