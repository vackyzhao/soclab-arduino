#include <Arduino.h>

// KR260 clean Lab3: UF1 / UF2; ZedBoard Lab3: extension LEDs D2 / D3.
constexpr uint8_t LED1 = 20;
constexpr uint8_t LED2 = 21;
constexpr size_t LINE_SIZE = 48; // At most 47 command characters plus '\0'.

char line[LINE_SIZE];
size_t lineLength = 0;
bool discardLine = false;
bool previousWasCR = false;
uint8_t led1State = LOW;
uint8_t led2State = LOW;

void printHelp() {
  Serial.println("Commands (lowercase):");
  Serial.println("  help");
  Serial.println("  status");
  Serial.println("  led1 on | led1 off | led1 toggle");
  Serial.println("  led2 on | led2 off | led2 toggle");
}

void printStatus() {
  // These are the states written by this sketch, not pin voltage readings.
  Serial.print("led1=");
  Serial.print(led1State == HIGH ? "on" : "off");
  Serial.print(" led2=");
  Serial.print(led2State == HIGH ? "on" : "off");
  Serial.print(" uptime_ms=");
  Serial.print(millis());
  Serial.print(" rx_overruns=");
  Serial.println(Serial.overruns());
}

bool setLedCommand(const char *command, uint8_t pin, uint8_t &state) {
  if (strcmp(command, "on") == 0) {
    state = HIGH;
  } else if (strcmp(command, "off") == 0) {
    state = LOW;
  } else if (strcmp(command, "toggle") == 0) {
    state = state == LOW ? HIGH : LOW;
  } else {
    return false;
  }
  digitalWrite(pin, state);
  return true;
}

void executeCommand() {
  if (strcmp(line, "help") == 0) {
    printHelp();
  } else if (strcmp(line, "status") == 0) {
    printStatus();
  } else if (strncmp(line, "led1 ", 5) == 0 &&
             setLedCommand(line + 5, LED1, led1State)) {
    printStatus();
  } else if (strncmp(line, "led2 ", 5) == 0 &&
             setLedCommand(line + 5, LED2, led2State)) {
    printStatus();
  } else {
    Serial.println("ERR: unknown command; type help");
  }
}

void receiveCharacter(char c) {
  if (c == '\r' || c == '\n') {
    // Treat CRLF as one line ending; CR or LF alone also works.
    if (c == '\n' && previousWasCR) {
      previousWasCR = false;
      return;
    }
    previousWasCR = c == '\r';

    if (discardLine) {
      Serial.println("ERR: line too long; discarded");
    } else if (lineLength != 0) {
      line[lineLength] = '\0';
      executeCommand();
    }
    lineLength = 0;
    discardLine = false;
    Serial.print("> ");
    return;
  }

  previousWasCR = false;
  if (discardLine) {
    return; // Never execute a valid-looking suffix of an oversized line.
  }
  if (c == '\b' || c == 127) {
    if (lineLength != 0) {
      --lineLength;
    }
  } else if (c >= ' ' && c <= '~') {
    if (lineLength < LINE_SIZE - 1) {
      line[lineLength++] = c;
    } else {
      discardLine = true;
    }
  }
}

void setup() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, led1State);
  digitalWrite(LED2, led2State);
  Serial.begin(115200);
  Serial.println("SoCLab LED console, 115200 baud, 8N1");
  printHelp();
  Serial.print("> ");
}

void loop() {
  while (Serial.available() > 0) {
    int received = Serial.read();
    if (received >= 0) {
      receiveCharacter((char)received);
    }
  }
}
