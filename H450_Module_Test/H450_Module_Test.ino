//H450 (TB67H450AFNG) module bench test for Arduino Nano
//
//Wiring (TB67H450AFNG <-> Nano):
//  VM   <- 6x AA battery pack (7.2-9V), battery minus also to Nano GND
//  OUT1/OUT2 -> motor
//  RS   -> 0.22 ohm sense resistor to GND (already on module)
//  VREF -> Nano 3.3V via 1k resistor (sets ~1.5A constant-current limit)
//  IN1  -> Nano D5 (PWM)
//  IN2  -> Nano D6 (PWM)
//
//Controls over Serial (9600 baud):
//  f  ramp forward then coast
//  r  ramp reverse then coast
//  b  brake (short)
//  s  stop / coast
//  x  auto sweep forward then reverse

#define out_A_IN1 5
#define out_A_IN2 6

const int RAMP_STEP_MS = 100;

void setup() {
  Serial.begin(9600);
  pinMode(out_A_IN1, OUTPUT);
  pinMode(out_A_IN2, OUTPUT);
  stopMotor();
  Serial.println("H450 test ready - f/r ramp, b brake, s stop, x sweep");
}

void loop() {
  if (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case 'f': rampForward(); break;
      case 'r': rampReverse(); break;
      case 'b': motorBrake(); break;
      case 's': stopMotor(); break;
      case 'x': sweep(); break;
    }
  }
}

void rampForward() {
  Serial.println("ramp forward");
  for (int pwm = 0; pwm <= 255; pwm++) {
    drive(1, pwm);
    delay(RAMP_STEP_MS);
  }
  stopMotor();
}

void rampReverse() {
  Serial.println("ramp reverse");
  for (int pwm = 0; pwm <= 255; pwm++) {
    drive(-1, pwm);
    delay(RAMP_STEP_MS);
  }
  stopMotor();
}

void sweep() {
  rampForward();
  delay(1000);
  rampReverse();
  delay(1000);
  motorBrake();
}

void motorBrake() {
  Serial.println("brake");
  digitalWrite(out_A_IN1, HIGH);
  digitalWrite(out_A_IN2, HIGH);
  delay(500);
  stopMotor();
}

void stopMotor() {
  digitalWrite(out_A_IN1, LOW);
  digitalWrite(out_A_IN2, LOW);
}

void drive(int direction, int pwm) {
  if (direction > 0) {
    analogWrite(out_A_IN1, pwm);
    digitalWrite(out_A_IN2, LOW);
  } else {
    digitalWrite(out_A_IN1, LOW);
    analogWrite(out_A_IN2, pwm);
  }
}