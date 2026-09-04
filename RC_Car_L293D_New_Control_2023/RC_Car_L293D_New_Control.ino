
//Not all this code is mine
//it was taken from http://rcarduino.blogspot.com/2012/01/how-to-read-rc-receiver-with.html

#include <Wire.h>
#include <EnableInterrupt.h>

#define voltage A6
#define THROTTLE_IN_PIN A1
#define THROTTLE_FLAG 1
#define GearFlag 2
#define Direction_A 5
#define Direction_B 3
#define battLight 4
#define frontLeft 9
#define frontRight 10
#define backlight 6
#define Gear A2

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
  pinMode(Direction_A, OUTPUT);
  pinMode(Direction_B, OUTPUT);
  pinMode(battLight, OUTPUT);
  pinMode(frontLeft, OUTPUT);
  pinMode(frontRight, OUTPUT);
  pinMode(backlight, OUTPUT);
  setDirection(0, 0);
  digitalWrite(battLight, HIGH);
  setLight(0);
  analogWrite(frontLeft, 255);
  analogWrite(frontRight, 0);
  analogWrite(backlight, 0);
  delay(1000);
  analogWrite(frontLeft, 0);
  analogWrite(frontRight, 255);
  analogWrite(backlight, 0);
  delay(1000);
  analogWrite(frontLeft, 0);
  analogWrite(frontRight, 0);
  analogWrite(backlight, 255);
  delay(1000);
  analogWrite(frontLeft, 0);
  analogWrite(frontRight, 0);
  analogWrite(backlight, 0);
  digitalWrite(battLight, LOW);  
}

void loop() {
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
  if(unGearIn>1900){
    lightStatus = 1;
  }else{
    lightStatus = 0;
  }
  
}
void setSpeed(uint16_t unThrottleIn){
  if(unThrottleIn<1440 & unThrottleIn>999){
    //min speed 1000
    //backwards
    //calculate the speed to a number within 0 and 255 for going up and down on the stick
    tempSpeed=(255-((unThrottleIn-1000)/1.96));

    //if the number is below zero, something is wrong with the receiver/calculation and set it to zero
    if(tempSpeed<0){
      tempSpeed=0;
    } else{
      if(tempSpeed< 50){
      tempSpeed =50;
    }else{
    if(tempSpeed> 200){
      tempSpeed =255;
    }else{
    if(tempSpeed> 255){
      tempSpeed =255;
    }}}}
    //just for printing speed but disabled during non-debugging
    //Serial.println((int)tempSpeed);
    
    //set the speed and then tell the shield to go "forward" but this is arbitrary
    
    //analogWrite(Enable_PWM, (int)tempSpeed)
    //myMotor->setSpeed((int)tempSpeed);
    setDirection(2, (int)tempSpeed);
    setLight(2);
    //max 1440
   }else{
   if(unThrottleIn>1560 & unThrottleIn<2100){
    //forwards
    // max 2000 but remove anything way over

    //set the speed from 0 to 255 again 
    tempSpeed=((unThrottleIn-1500)/1.96);

    //check if the speed comes to above 255 for receiver/calculation problems and change that to 255
    if(tempSpeed>210){
      tempSpeed=255;
    }else{
      if(tempSpeed>255){
        tempSpeed=255;
    }
    }

    //debug printing
    //Serial.println((int)tempSpeed);

    //set the speed and then tell the shield to go "backward" but this is arbitrary
    //analogWrite(Enable_PWM, (int)tempSpeed)
    //myMotor->setSpeed((int)tempSpeed);
    setDirection(1, (int)tempSpeed);
    setLight(1);
   }else{
    //release the power to the motor and let the car roll
     setDirection(0, 0);
     setLight(0);
     if(battLow == 0){
   int value = analogRead(voltage);
   
   float voltages = (value*(5.0/1023.0) - error);
   //Serial.println(voltages);
   if(voltages<3.00){
    digitalWrite(battLight, HIGH);
    battLow = 1;
    //Serial.println("test");
   }
}
   }
}
}
void setDirection(int direction, int PWM){
  if(direction==0){
    analogWrite(Direction_A, LOW);
    analogWrite(Direction_B, LOW);
  } else{
    if(direction==1){
      //Serial.println(PWM);
      analogWrite(Direction_A, LOW);
      analogWrite(Direction_B, PWM);
    } else {
        //Serial.println(PWM);
      analogWrite(Direction_A, PWM);
      analogWrite(Direction_B, LOW);
    }
  }
}
void setLight(int i){
  if(lightStatus ==1){
  //Serial.println(i);
  if(i==1){
  analogWrite(frontLeft, 255);
  analogWrite(frontRight, 255);
  analogWrite(backlight, 150);
  }else{
    if(i==2){
        analogWrite(frontLeft, 150);
        analogWrite(frontRight, 150);
        analogWrite(backlight, 150);
    }else{
        analogWrite(frontLeft, 150);
        analogWrite(frontRight, 150);
        analogWrite(backlight, 150);
    }

  }
  }else{
    analogWrite(frontLeft, 0);
    analogWrite(frontRight, 0);
    analogWrite(backlight, 0);
  }
}
