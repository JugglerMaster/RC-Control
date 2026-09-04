//ATtiny85 port of RC_Car_FNG_v2 - minimal 4-pin speed controller for the FNG (TB6612FNG)
//Firmware logic (median filter, ranges, scaling, clamps) identical to RC_Car_FNG_v2.ino,
//but stripped to a 4-pin build to run on an ATtiny85 (8KB flash / 512B SRAM).
//
//Wiring (ATtiny85 DIP-8, ATTinyCore pin numbering = port bit):
//  D0 (PB0, pin5) -> receiver throttle channel (PCINT0, interrupt input)
//  D1 (PB1, pin6) -> FNG PWMA (Timer1 OC1A - the hardware PWM pin)
//  D2 (PB2, pin7) -> FNG AIN1
//  D3 (PB3, pin2) -> FNG AIN2
//  D4 (PB4, pin3) -> unused (free)
//  D5 (PB5, pin1) -> RESET - do NOT disable (RSTDISBL) or in-circuit ISP dies
//  FNG STBY tied directly to 5V (always enabled)
//
//Things dropped vs v2: Serial debug, Wire, lights (front/back/bottom), gear channel,
//battery voltage monitor, and the EnableInterrupt library (replaced with a native
//pin-change ISR - same mechanism, smaller build).
//
//Compile/flash with ATTinyCore (http://drazzy.com/package_drazzy.com_index.json):
//  arduino-cli compile --fqbn ATTinyCore:avr:attinyx5:chip=85,clock=8internal
//  avrdude -c avrisp -P COM9 -b 19200 -p t85 -e -U flash:w:<hex>:i -U lfuse:w:0xE2:m -U hfuse:w:0xDD:m -U efuse:w:0xFF:m
//(fusion of an Arduino-as-ISP programmer on COM9; 8MHz internal clock)

#define THROTTLE_IN_PIN 0   //PB0
#define out_A_PWM 1         //PB1 OC1A
#define out_A_IN1 2         //PB2
#define out_A_IN2 3         //PB3

#define THROTTLE_FLAG 1

volatile uint8_t bUpdateFlagsShared;
volatile uint16_t unThrottleInShared;
uint32_t ulThrottleStart;

double tempSpeed;

void setup() {
  pinMode(out_A_IN1, OUTPUT);
  pinMode(out_A_IN2, OUTPUT);
  pinMode(out_A_PWM, OUTPUT);
  pinMode(THROTTLE_IN_PIN, INPUT);
  setDirection(0, 0);

  //pin change interrupt on ATtiny85, PB0 = PCINT0
  PCMSK |= (1 << PCINT0);   //watch PB0
  GIMSK |= (1 << PCIE);     //enable pin change interrupts
}

void loop() {
  //vars that change per loop
  static uint16_t unThrottleIn;
  static uint8_t bUpdateFlags;

  if(bUpdateFlagsShared) {
    noInterrupts(); //take a local copy of the shared variables

    bUpdateFlags = bUpdateFlagsShared;
    if(bUpdateFlags & THROTTLE_FLAG) {
      unThrottleIn = unThrottleInShared;
    }

    bUpdateFlagsShared = 0; //all updates have been taken
    interrupts();
  }
  if(bUpdateFlags & THROTTLE_FLAG) {
    setSpeed(unThrottleIn);
    bUpdateFlags = 0;
  }
}

//pin change interrupt service routine (replaces EnableInterrupt on the tiny85)
ISR(PCINT0_vect) {
  if(digitalRead(THROTTLE_IN_PIN) == HIGH) {
    ulThrottleStart = micros(); //rising edge, record start
  } else {
    //falling edge, pulse duration = time between edges
    unThrottleInShared = (uint16_t)(micros() - ulThrottleStart);
    bUpdateFlagsShared |= THROTTLE_FLAG;
  }
}

void setSpeed(uint16_t unThrottleIn){
  //dirty data filter: median of last 3 readings removes one-off corrupt samples
  //while still tracking genuine fast throws (a new stable value becomes the median)
  static uint16_t m1 = 0, m2 = 0, m3 = 0;
  if(unThrottleIn < 500 || unThrottleIn > 2500){
    return; //out of RC pulse band, ignore
  }
  m3 = m2;
  m2 = m1;
  m1 = unThrottleIn;
  //only use the reading once we have a full 3-sample window
  if(m3 != 0){
    uint16_t a = m1, b = m2, c = m3;
    //simple median of three
    if(a > b){ uint16_t t = a; a = b; b = t; }
    if(b > c){ uint16_t t = b; b = c; c = t; }
    if(a > b){ uint16_t t = a; a = b; b = t; }
    unThrottleIn = b;
  }
  if(unThrottleIn <= 1439 & unThrottleIn >= 1060){
    //backwards: 1060 = full reverse, 1439 = dead zone edge
    //1.96-style scale: (1468-1072)/255 = 1.55 -> full reverse = 255
    tempSpeed = (1468 - unThrottleIn) / 1.55;
    if(tempSpeed<0){
      tempSpeed=0;
    }
    if(tempSpeed>255){
      tempSpeed=255;
    }
    setDirection(1, (int)tempSpeed);
   }else{
    if(unThrottleIn >= 1500 & unThrottleIn <= 1900){
    //forwards: 1500 = just above neutral, 1900 = full forward
    //1.96-style scale: (1884-1468)/255 = 1.63 -> full forward = 255
    tempSpeed = (unThrottleIn - 1468) / 1.63;
    if(tempSpeed<0){
      tempSpeed=0;
    }
    if(tempSpeed>255){
      tempSpeed=255;
    }
    setDirection(2, (int)tempSpeed);
   }else{
    //release the power to the motor and let the car roll
     setDirection(0, 0);
   }
 }
}
void setDirection(int direction, int PWM){
  if(direction==0){
    digitalWrite(out_A_IN1,LOW);
    digitalWrite(out_A_IN2,LOW);
    digitalWrite(out_A_PWM,HIGH);
  } else{
    if(direction==1){
     digitalWrite(out_A_IN1,LOW);
     digitalWrite(out_A_IN2,HIGH);
     analogWrite(out_A_PWM,PWM);
    } else {
     digitalWrite(out_A_IN1,HIGH);
     digitalWrite(out_A_IN2,LOW);
     analogWrite(out_A_PWM,PWM);
    }
  }
}