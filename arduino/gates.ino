#include <Servo.h>
#include <math.h>

Servo gate1;
Servo gate2;
Servo gate3;

const int closedPulse = 1000;
const int openPulse = 1600;
const int duration = 2000;

struct GateState {
  Servo* servo;
  int startPulse = closedPulse;
  int endPulse = openPulse;
  int currentPulse = closedPulse;
  unsigned long startTime = 0;
  bool isMoving = false;
};

GateState gates[3];

String inputBuffer = "";

void setup() {
  Serial.begin(9600);

  gates[0].servo = &gate1;
  gates[1].servo = &gate2;
  gates[2].servo = &gate3;

  gate1.attach(7);
  gate2.attach(9);
  gate3.attach(11);

  for (int i = 0; i < 3; ++i) {
    gates[i].servo->writeMicroseconds(gates[i].currentPulse);
  }
}

void loop() {
  // Read serial input into buffer
  while (Serial.available()) {
    char ch = Serial.read();
    if (isAlpha(ch) || isDigit(ch)) {
      inputBuffer += ch;
    }
  }

  // Process buffered commands in pairs
  while (inputBuffer.length() >= 2) {
    char action = inputBuffer.charAt(0);
    char gateChar = inputBuffer.charAt(1);
    inputBuffer = inputBuffer.substring(2); // Remove processed command

    if ((gateChar >= '1') && (gateChar <= '3')) {
      int gateIndex = gateChar - '1';
      GateState& g = gates[gateIndex];

      if ((action == 'o' || action == 'O') && g.currentPulse != openPulse) {
        g.startPulse = g.currentPulse;
        g.endPulse = openPulse;
        g.startTime = millis();
        g.isMoving = true;
      } else if ((action == 'c' || action == 'C') && g.currentPulse != closedPulse) {
        g.startPulse = g.currentPulse;
        g.endPulse = closedPulse;
        g.startTime = millis();
        g.isMoving = true;
      }
    }
  }

  // Animate each gate
  unsigned long now = millis();
  for (int i = 0; i < 3; ++i) {
    GateState& g = gates[i];
    if (g.isMoving) {
      unsigned long elapsed = now - g.startTime;

      if (elapsed <= duration) {
        float t = (float)elapsed / duration;
        float easedT = 0.5 * (1 - cos(PI * t));
        int pulse = g.startPulse + (g.endPulse - g.startPulse) * easedT;
        g.servo->writeMicroseconds(pulse);
        g.currentPulse = pulse;
      } else {
        g.servo->writeMicroseconds(g.endPulse);
        g.currentPulse = g.endPulse;
        g.isMoving = false;
      }
    }
  }
}
