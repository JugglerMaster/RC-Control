
//Not all this code is mine
//it was taken from http://rcarduino.blogspot.com/2012/01/how-to-read-rc-receiver-with.html

#include <Wire.h>
#include <EnableInterrupt.h>

//FNG chip
#define out_STBY 4
#define out_A_PWM 10

#define out_A_IN1 8
#define out_A_IN2 7

//
#define voltage A3
#define THROTTLE_IN_PIN 3
#define THROTTLE_FLAG 1
#define GearFlag 2
//#define Direction_A 5
//#define Direction_B 3
#define Back 6
#define front 9
#define bottom 5
#define Gear 2

int lightStatus = 0;

int battLow = 0;
float error = 0.18;
double tempSpeed;
//int batteryLV = 0;
volatile uint8_t bUpdateFlagsShared;

volatile uint16_t unThrottleInShared;
volatile uint16_t unGearInShared;
volatile uint16_t ulGearStart;

uint32_t ulThrottleStart;


    
void setup() {
  Serial.begin(9600);
  enableInterrupt(THROTTLE_IN_PIN, calcThrottle, CHANGE);
  enableInterrupt(Gear, lights1, CHANGE);
  //pinMode(Direction_A, OUTPUT);
  //pinMode(Direction_B, OUTPUT);
  pinMode(out_A_IN1, OUTPUT);
  pinMode(out_A_IN2, OUTPUT);
  pinMode(out_STBY, OUTPUT);
  pinMode(out_A_PWM, OUTPUT);
  
  pinMode(Back, OUTPUT);
  pinMode(front, OUTPUT);
  pinMode(bottom, OUTPUT);
  setDirection(0, 0);
  setLight(0);

  motor_standby(false);
  
}

void loop() {
  //Serial.println("lights flag");
  //delay(100);
//vars that change per loop
static uint16_t unThrottleIn;
static uint16_t unGearIn;
 static uint8_t bUpdateFlags;
//Serial.println("test");

   
 if(bUpdateFlagsShared)
  {
    noInterrupts(); // turn interrupts off quickly while we take local copies of the shared variables

    // take a local copy of which channels were updated in case we need to use this in the rest of loop
    bUpdateFlags = bUpdateFlagsShared;
   
    // in the current code, the shared values are always populated
    // so we could copy them without testing the flags
    // however in the future this could change, so lets
    // only copy when the flags tell us we can.
   
    if(bUpdateFlags & THROTTLE_FLAG)
    {
      unThrottleIn = unThrottleInShared;
    }
    if(bUpdateFlags & GearFlag){
      unGearIn = unGearInShared;
    }
    // clear shared copy of updated flags as we have already taken the updates
    // we still have a local copy if we need to use it in bUpdateFlags
    bUpdateFlagsShared = 0;
   
    interrupts(); // we have local copies of the inputs, so now we can turn interrupts back on
    // as soon as interrupts are back on, we can no longer use the shared copies, the interrupt
    // service routines own these and could update them at any time. During the update, the
    // shared copies may contain junk. Luckily we have our local copies to work with :-)
  }
 
  // do any processing from here onwards
  // only use the local values unAuxIn, unThrottleIn and unSteeringIn, the shared
  // variables unAuxInShared, unThrottleInShared, unSteeringInShared are always owned by
  // the interrupt routines and should not be used in loop
 
  // the following code provides simple pass through
  // this is a good initial test, the Arduino will pass through
  // receiver input as if the Arduino is not there.
  // This should be used to confirm the circuit and power
  // before attempting any custom processing in a project.
 
  // we are checking to see if the channel value has changed, this is indicated 
  // by the flags. For the simple pass through we don't really need this check,
  // but for a more complex project where a new signal requires significant processing
  // this allows us to only calculate new values when we have new inputs, rather than
  // on every cycle.
  if(bUpdateFlags & GearFlag)
  {
    setLights(unGearIn);
 //Serial.println("lights flag");
  
}
  if(bUpdateFlags & THROTTLE_FLAG)
  {
    setSpeed(unThrottleIn);
 
  bUpdateFlags = 0;
}
}
void lights1(){
  if(digitalRead(Gear) == HIGH)
  {
    //lightStatus = 1;
    //Serial.println("lights");
    ulGearStart = micros();
  }else{
        unGearInShared = (uint16_t)(micros() - ulGearStart);
    // use set the throttle flag to indicate that a new throttle signal has been received
    bUpdateFlagsShared |= GearFlag;
  }
}
// simple interrupt service routine
void calcThrottle()
{
  // if the pin is high, its a rising edge of the signal pulse, so lets record its value
  if(digitalRead(THROTTLE_IN_PIN) == HIGH)
  {
    ulThrottleStart = micros();
    
  }
  else
  {
    // else it must be a falling edge, so lets get the time and subtract the time of the rising edge
    // this gives use the time between the rising and falling edges i.e. the pulse duration.
    unThrottleInShared = (uint16_t)(micros() - ulThrottleStart);
    // use set the throttle flag to indicate that a new throttle signal has been received
    bUpdateFlagsShared |= THROTTLE_FLAG;
  }
}
void setLights(uint16_t unGearIn){
  //Serial.println(unGearIn);
  if(unGearIn>1500){
    lightStatus = 1;
  }else{
    lightStatus = 0;
  }
  
}
void setSpeed(uint16_t unThrottleIn){
  if(unThrottleIn<1440 & unThrottleIn>999){
    //min speed 1000
    //forwards
    //calculate the speed to a number within 0 and 255 for going up and down on the stick
    tempSpeed=(255-((unThrottleIn-1000)/3));
    //Serial.println(unThrottleIn);
    //Serial.println(275-(unThrottleIn-1000)/1.96);
    //if the number is below zero, something is wrong with the receiver/calculation and set it to zero
    if(tempSpeed<0){
      tempSpeed=0;
    }
    if(tempSpeed>234){
      tempSpeed=255;
    }
    //just for printing speed but disabled during non-debugging
    //Serial.println((int)tempSpeed);
    
    //set the speed and then tell the shield to go "forward" but this is arbitrary
    
    //analogWrite(Enable_PWM, (int)tempSpeed)
    //myMotor->setSpeed((int)tempSpeed);
    setDirection(1, (int)tempSpeed);
    setLight(2);
    //max 1440
   }else{
   if(unThrottleIn>1560 & unThrottleIn<2100){
    //backwards
    // max 2000 but remove anything way over

    //set the speed from 0 to 255 again 
    tempSpeed=((unThrottleIn-1500)/1.96);

    //check if the speed comes to above 255 for receiver/calculation problems and change that to 255
    if(tempSpeed>255){
      tempSpeed=255;
    }

    //debug printing
    //Serial.println((int)tempSpeed);

    //set the speed and then tell the shield to go "backward" but this is arbitrary
    //analogWrite(Enable_PWM, (int)tempSpeed)
    //myMotor->setSpeed((int)tempSpeed);
    setDirection(2, (int)tempSpeed);
    setLight(1);
   }else{
    //release the power to the motor and let the car roll
     setDirection(0, 0);
     setLight(0);
     if(battLow == 0){
   int value = analogRead(voltage);
   
   float voltages = (value*(5.0/1023.0) - error);
   //Serial.println(voltages);
   if(voltages<3.10){
    //digitalWrite(battLight, HIGH);

    //Need to write battery low light code!!!
    
    battLow = 1;
    //Serial.println("test");
   }
}
   }
}
}
void motor_standby(boolean state) { //low power mode
 if (state == true)
   digitalWrite(out_STBY,LOW);
 else
   digitalWrite(out_STBY,HIGH);
}
void setDirection(int direction, int PWM){
  if(direction==0){
    //analogWrite(Direction_A, LOW);
    //analogWrite(Direction_B, LOW);
   digitalWrite(out_A_IN1,LOW);
   digitalWrite(out_A_IN2,LOW);
   digitalWrite(out_A_PWM,HIGH);
  } else{
    if(direction==1){
      //Serial.println(PWM);
     digitalWrite(out_A_IN1,LOW);
     digitalWrite(out_A_IN2,HIGH);
     analogWrite(out_A_PWM,PWM);
    } else {
        //Serial.println(PWM);
     digitalWrite(out_A_IN1,HIGH);
     digitalWrite(out_A_IN2,LOW);
     analogWrite(out_A_PWM,PWM);
    }
  }
}
void motor_brake(boolean motor) {
   digitalWrite(out_A_IN1,HIGH);
   digitalWrite(out_A_IN2,HIGH);
}
void setLight(int i){
  if(lightStatus ==1){
  //Serial.println(i);
  if(i==1){
  analogWrite(front, 255);
  analogWrite(Back, 255);
  analogWrite(bottom, 50);
  }else{
    if(i==2){
        analogWrite(front, 150);
        analogWrite(Back, 150);
        analogWrite(bottom, 150);
    }else{
        analogWrite(front, 150);
        analogWrite(Back, 150);
        analogWrite(bottom, 50);
    }

  }
  }else{
    analogWrite(front, 50);
    analogWrite(Back, 50);
    analogWrite(bottom, 0);
  }
}
