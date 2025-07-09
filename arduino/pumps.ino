// Define pin assignments for each motor
struct StepperMotor {
  uint8_t dirPin;
  uint8_t stepPin;
};

StepperMotor motors[4] = {
  {5, 2},   // X-axis
  {6, 3},   // Y-axis
  {7, 4},   // Z-axis
  {12, 13}  // A-axis
};

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 4; i++) {
    pinMode(motors[i].dirPin, OUTPUT);
    pinMode(motors[i].stepPin, OUTPUT);
    digitalWrite(motors[i].stepPin, LOW);
    digitalWrite(motors[i].dirPin, LOW);
  }
}

String inputString = "";
bool stringComplete = false;

void loop() {
  if (stringComplete) {
    processCommand(inputString);
    inputString = "";
    stringComplete = false;
  }
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() < 3) return;

  char dirChar = cmd.charAt(0);
  char pumpChar = cmd.charAt(1);

  int motorIndex = -1;
  if (pumpChar == 'x') motorIndex = 0;
  else if (pumpChar == 'y') motorIndex = 1;
  else if (pumpChar == 'z') motorIndex = 2;
  else if (pumpChar == 'a') motorIndex = 3;

  if (motorIndex == -1) return;

  int dir = (dirChar == 'h') ? HIGH : LOW;
  digitalWrite(motors[motorIndex].dirPin, dir);

  // Parse the two integers: cycles and delay in microseconds
  int firstSpace = cmd.indexOf(' ', 2);
  if (firstSpace == -1) return;
  int secondSpace = cmd.indexOf(' ', firstSpace + 1);
  if (secondSpace == -1) secondSpace = cmd.length();

  int cycles = cmd.substring(firstSpace + 1, secondSpace).toInt();
  int delayMicros = cmd.substring(secondSpace + 1).toInt();

  for (int i = 0; i < cycles; i++) {
    digitalWrite(motors[motorIndex].stepPin, HIGH);
    delayMicroseconds(delayMicros); 
    digitalWrite(motors[motorIndex].stepPin, LOW);
    delayMicroseconds(delayMicros); 
  }
}

// Serial input handling
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
      Serial.println(inputString);
    } else {
      inputString += inChar;
    }
  }
}
