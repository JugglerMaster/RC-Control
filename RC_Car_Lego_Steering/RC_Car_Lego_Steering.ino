//Lego Powered-Up style steering car - Arduino Mini Pro (ATmega328P)
//Built from the RC_Car_FNG_v2 board wiring (existing car hardware):
//  out_STBY 7, out_A_PWM 11, out_A_IN1 2, out_A_IN2 4, throttle 5
//Light circuits (pins 9, 10) were physically removed and are NOT used here.
//
//The drive motor runs on FNG channel A (this board's single channel). Steering
//uses the same TB6612FNG's channel B plus a Lego multi-pole switch (3 pins)
//that reports the steering rack position in 4 discrete zones.
//
//LEGO steering rack position encoder - physical part connections:
//The encoder has a sliding wiper (pin 0) that travels over fixed contacts
//(pins 1, 2, 3). The start and end of travel make SOLID contacts:
//   Travel start (origin = hard right / stowed): pin 0 connects to pin 3
//   Travel end   (hard left limit):             pin 0 connects to 1+2+3
//Between sections the wiper passes blank gaps where pin 0 makes NO contact,
//so no sense pin reads active there (7 positions total: 4 contacts + 3 gaps).
//
//Wiring to the Arduino: pin 0 (wiper) -> GND, pins 1/2/3 -> A0/A1/A2 with
//INPUT_PULLUP. Wiping a contact shorts the pin to GND = "active" (LOW).
//Moving the rack LEFT from the origin reads:
//   Zone 0: pin3(wiper) only     -> 100   (origin / hard right)
//          (blank gap: no contact -> 000)
//   Zone 1: pin1 + pin3          -> 101
//          (blank gap: no contact -> 000)
//   Zone 2: pin2 only            -> 010   (steering center)
//          (blank gap: no contact -> 000)
//   Zone 3: all of 1,2,3         -> 111   (hard left limit)
//Blank gaps and any other undefined code keep the last known zone
//(hysteresis) so the rack never jumps while crossing a dead spot.
//NOTE: all-3-active means the rack is AT the left end-stop, so we must NOT
//drive further left in that zone (only allow returning right).
//
//Steering center calibration: the RC stick's real neutral may sit a few us off
//1500 (radio/trim dependent). At boot we measure and store it in EEPROM, then
//use that measured value as the zero point for the deadband and throw mapping.
//Short pin 13 to GND during boot to force a fresh calibration.
//
//Throttle + steering channels come from an RC receiver via interrupts (same
//read logic as RC_Car_FNG_v2).
//
//Wiring (Arduino Mini Pro):
//  Throttle  channel  -> 5   (EnableInterrupt)
//  Steering  channel  -> 3   (EnableInterrupt)
//  Steering pos p1     -> A0
//  Steering pos p2     -> A1
//  Steering pos p3     -> A2 (common wiper, reads active-low via pullup)
//  FNG STBY            -> 7
//  FNG channel A (drive): AIN1 -> 2, AIN2 -> 4, PWMA -> 11
//  FNG channel B (steer): BIN1 -> 6, BIN2 -> 8, PWMB -> 12
//  CAL pin             -> 13  (short to GND during boot to recalibrate)

#include <EnableInterrupt.h>
#include <EEPROM.h>

//---------------------- RC input pins -------------------------
#define THROTTLE_IN_PIN 5
#define STEER_IN_PIN    3

//---------------------- FNG driver pins ------------------------
//Channel A = drive motor (original board wiring)
//Channel B = steering motor
#define out_STBY 7
#define A_AIN1 2
#define A_AIN2 4
#define A_PWMA 11
#define B_BIN1 6
#define B_BIN2 8
#define B_PWMB 12

//---------------------- Steering position pins -----------------
#define STEER_P1 A0
#define STEER_P2 A1
#define STEER_P3 A2

//---------------------- RC flags -------------------------------
#define THROTTLE_FLAG 1
#define STEER_FLAG    2

//---------------------- Steering position zones -----------------
#define ZONE0 0   //origin, hard right
#define ZONE1 1   //just left of origin
#define ZONE2 2   //center region
#define ZONE3 3   //hard left end-stop

#define STEER_DEADBAND 40      //us around center that counts as "center"
#define STEER_FULL     400     //us throw from center for full lock

//Steering center calibration. The RC stick's true neutral may sit a little off
//1500us depending on the radio/trim. We auto-measure it during the first N
//samples at boot (stick assumed at rest/center), then load from EEPROM on
//later boots unless a recal is requested. CAL_STEER_PIN shorts a pin to GND
//to force a fresh calibration on next boot.
#define CAL_STEER_PIN  13      //short to GND during boot to recalibrate
#define CAL_SAMPLES    40      //samples averaged at boot to find neutral
#define STEER_CAL_ADDR 0       //EEPROM address storing the calibrated neutral

//-------------------- state --------------------------------------
double tempSpeed;

volatile uint8_t bUpdateFlagsShared;
volatile uint16_t unThrottleInShared;
volatile uint16_t unSteerInShared;
uint32_t ulThrottleStart;
uint32_t ulSteerStart;

uint8_t steerZone = ZONE0;

int16_t steerCenter = 1500;   //calibrated RC steering neutral (us)

void setup() {
  Serial.begin(9600);
  enableInterrupt(THROTTLE_IN_PIN, calcThrottle, CHANGE);
  enableInterrupt(STEER_IN_PIN, calcSteer, CHANGE);

  pinMode(out_STBY, OUTPUT);
  pinMode(A_AIN1, OUTPUT);
  pinMode(A_AIN2, OUTPUT);
  pinMode(A_PWMA, OUTPUT);
  pinMode(B_BIN1, OUTPUT);
  pinMode(B_BIN2, OUTPUT);
  pinMode(B_PWMB, OUTPUT);
  motor_standby(false);

  //steering position inputs (pullups; pin reads LOW when bridged to wiper)
  pinMode(STEER_P1, INPUT_PULLUP);
  pinMode(STEER_P2, INPUT_PULLUP);
  pinMode(STEER_P3, INPUT_PULLUP);

  pinMode(CAL_STEER_PIN, INPUT_PULLUP);

  setDirection(0, 0);
  steerStop();

  calibrateSteerCenter();
}

//Measure + store the RC steering neutral. Called at boot. Assumes the stick
//is at rest/center. If the calibration pin is shorted to GND it forces a
//fresh measure; otherwise it reuses the value saved to EEPROM.
void calibrateSteerCenter() {
  if (digitalRead(CAL_STEER_PIN) == LOW) {
    //forced recal: sample while the stick sits at center
    long sum = 0;
    int n = 0;
    for (int i = 0; i < CAL_SAMPLES; i++) {
      if (bUpdateFlagsShared & STEER_FLAG) {
        noInterrupts();
        sum += unSteerInShared;
        bUpdateFlagsShared &= ~STEER_FLAG;
        interrupts();
        n++;
      }
      delay(20);
    }
    if (n > 0) {
      steerCenter = sum / n;
      EEPROM.put(STEER_CAL_ADDR, steerCenter);
      Serial.print(F("CAL steer center: "));
      Serial.println(steerCenter);
    }
  } else {
    //load stored neutral, default to 1500 if none saved (fresh EEPROM = 0xFFFF)
    int16_t v;
    EEPROM.get(STEER_CAL_ADDR, v);
    steerCenter = (v >= 900 && v <= 2100) ? v : 1500;
    Serial.print(F("Loaded steer center: "));
    Serial.println(steerCenter);
  }
}

void loop() {
  static uint16_t unThrottleIn = 1500;
  static int16_t  unSteerIn = 1500;
  static uint8_t bUpdateFlags = 0;

  if (bUpdateFlagsShared) {
    noInterrupts();
    bUpdateFlags = bUpdateFlagsShared;
    if (bUpdateFlags & THROTTLE_FLAG) unThrottleIn = unThrottleInShared;
    if (bUpdateFlags & STEER_FLAG)    unSteerIn = unSteerInShared;
    bUpdateFlagsShared = 0;
    interrupts();
  }

  readSteerZone();

  if (bUpdateFlags & THROTTLE_FLAG) {
    setSpeed(unThrottleIn);
  }
  if (bUpdateFlags & STEER_FLAG) {
    setSteer(unSteerIn);
    bUpdateFlags = 0;
  }
}

//---------------------- Steering position encoder ----------------
void readSteerZone() {
  int p1 = digitalRead(STEER_P1);
  int p2 = digitalRead(STEER_P2);
  int p3 = digitalRead(STEER_P3);

  //active-low: 0 = bridged to wiper/active, 1 = open
  //code bits: bit2 = p1, bit1 = p2, bit0 = p3
  uint8_t code = ((!p1) ? 4 : 0) | ((!p2) ? 2 : 0) | ((!p3) ? 1 : 0);

  //debug: print on raw-state change so the 7 physical positions can be mapped
  static uint8_t lastCode = 0xFF;
  if (code != lastCode) {
    lastCode = code;
    Serial.print(F("STEER raw="));
    Serial.println(code, BIN);
  }

  //Zone0 origin: wiper on pin3  -> 001
  switch (code) {
    case 0x1: steerZone = ZONE0; break;   //001 right origin  (p3 linked)
    case 0x5: steerZone = ZONE1; break;   //101 p1+p3
    case 0x2: steerZone = ZONE2; break;   //010 p2 only (center)
    case 0x7: steerZone = ZONE3; break;   //111 all active (left end-stop)
    default:
      //blank gap or transitory state: no contact made; keep the last known
      //zone (hysteresis) instead of jittering while crossing dead spots
      break;
  }
}

//Map analog RC steering throw to a steering output. Target only ever
//drives the motor AWAY from / toward zones; zone3 clamps so we never go
//past the left end stop, and zone0 clamps the right end.
void setSteer(int16_t unSteerIn) {
  int err = unSteerIn - steerCenter;

  //left = negative err, right = positive err (adjust sign to wiring)
  if (err < -STEER_DEADBAND) {
    //wants to go left
    if (steerZone == ZONE3) {
      //already at the left end-stop: all 3 pins active, stop motion
      steerStop();
    } else {
      steerLeft(mapSpeed(err));
    }
  } else if (err > STEER_DEADBAND) {
    //wants to go right
    if (steerZone == ZONE0) {
      steerStop();
    } else {
      steerRight(mapSpeed(err));
    }
  } else {
    //center stick: hold position, no drive
    steerStop();
  }
}

int mapSpeed(int err) {
  int mag = abs(err);
  if (mag > STEER_FULL) mag = STEER_FULL;
  //scale 0..STEER_FULL -> 60..255 for a bit of headroom
  return map(mag, 0, STEER_FULL, 60, 255);
}

void steerLeft(int pwm)  { motorB(1, pwm); }
void steerRight(int pwm) { motorB(2, pwm); }
void steerStop()         { motorB(0, 0); }

//---------------------- FNG channel B (steering) ------------------
void motorB(int dir, int pwm) {
  if (dir == 0) {
    digitalWrite(B_BIN1, LOW);
    digitalWrite(B_BIN2, LOW);
    digitalWrite(B_PWMB, HIGH);
  } else if (dir == 1) {
    digitalWrite(B_BIN1, LOW);
    digitalWrite(B_BIN2, HIGH);
    analogWrite(B_PWMB, pwm);
  } else {
    digitalWrite(B_BIN1, HIGH);
    digitalWrite(B_BIN2, LOW);
    analogWrite(B_PWMB, pwm);
  }
}

void motor_standby(boolean state) {
  if (state == true) digitalWrite(out_STBY, LOW);
  else digitalWrite(out_STBY, HIGH);
}

//---------------------- Drive channel (FNG channel A, v2 scaling) --
void setSpeed(uint16_t unThrottleIn) {
  //median-of-3 dirty data filter
  static uint16_t m1 = 0, m2 = 0, m3 = 0;
  if (unThrottleIn < 500 || unThrottleIn > 2500) return;
  m3 = m2; m2 = m1; m1 = unThrottleIn;
  if (m3 != 0) {
    uint16_t a = m1, b = m2, c = m3;
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    if (a > b) { uint16_t t = a; a = b; b = t; }
    unThrottleIn = b;
  }

  if (unThrottleIn <= 1439 && unThrottleIn >= 1060) {
    //backwards: 1060 = full reverse, 1439 = dead zone edge
    tempSpeed = (1468 - unThrottleIn) / 1.55;
    if (tempSpeed < 0) tempSpeed = 0;
    if (tempSpeed > 255) tempSpeed = 255;
    setDirection(1, (int)tempSpeed);
  } else if (unThrottleIn >= 1500 && unThrottleIn <= 1900) {
    //forwards: 1500 = just above neutral, 1900 = full forward
    tempSpeed = (unThrottleIn - 1468) / 1.63;
    if (tempSpeed < 0) tempSpeed = 0;
    if (tempSpeed > 255) tempSpeed = 255;
    setDirection(2, (int)tempSpeed);
  } else {
    //neutral, let the car roll
    setDirection(0, 0);
  }
}

void setDirection(int direction, int PWM) {
  if (direction == 0) {
    digitalWrite(A_AIN1, LOW);
    digitalWrite(A_AIN2, LOW);
    digitalWrite(A_PWMA, HIGH);
  } else {
    if (direction == 1) {
      digitalWrite(A_AIN1, LOW);
      digitalWrite(A_AIN2, HIGH);
      analogWrite(A_PWMA, PWM);
    } else {
      digitalWrite(A_AIN1, HIGH);
      digitalWrite(A_AIN2, LOW);
      analogWrite(A_PWMA, PWM);
    }
  }
}

//---------------------- RC interrupt handlers ---------------------
void calcThrottle() {
  if (digitalRead(THROTTLE_IN_PIN) == HIGH) {
    ulThrottleStart = micros();
  } else {
    unThrottleInShared = (uint16_t)(micros() - ulThrottleStart);
    bUpdateFlagsShared |= THROTTLE_FLAG;
  }
}
void calcSteer() {
  if (digitalRead(STEER_IN_PIN) == HIGH) {
    ulSteerStart = micros();
  } else {
    unSteerInShared = (uint16_t)(micros() - ulSteerStart);
    bUpdateFlagsShared |= STEER_FLAG;
  }
}