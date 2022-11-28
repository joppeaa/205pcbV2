#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_SERIAL if(DEBUG)Serial

#include "pinDeclarations.h"

#define amountOfTimers 7

unsigned long prevFastLoopMillis;                                             //Main loop timers
const unsigned fastLoopMax = 10;  

unsigned long prevSlowLoopMillis;
const unsigned slowLoopMax = 1000;

unsigned long prevWindowMillis;
const unsigned int windowDriveTime = 4000;

bool windowRunningDown = false;
bool windowRunningUp = false;

const int LONG_PRESS_MIN  = 1300; // 1300 milliseconds

#define amountOfCurrentReadings 10
int currentReadings[amountOfCurrentReadings];
int currentReadingIndex;
int currentSenseAverage;

struct remoteModule
{
  byte id;
  byte modulePin;
  int currentState;
  int lastState = LOW;
  unsigned long pressedTime;
  unsigned long releasedTime;
  bool isPressing = false;
  bool isLongDetected = false;
  bool shortpressFlag = false;
  bool longpressFlag = false;
} buttonNr[4];

struct decayOutput
{
  bool isRunning = false;
  unsigned long prevTimerMillis;
  int decayTime;
  byte repeatTimes = 1;
  byte repeatCounter = 0;
  byte outputPin;
  bool run = false;
} decayFunction[amountOfTimers];

void setup() {
  // put your setup code here, to run once:
  DEBUG_SERIAL.begin(115200);
  
  for (int i; i <= amountOfCurrentReadings; i ++)
  {
    currentReadings[i] = 4095;
  }
  
  pinMode(rcIn3Pin, INPUT_PULLDOWN);                                //433 MHz module inputs
  pinMode(rcIn2Pin, INPUT_PULLDOWN);
  pinMode(rcIn1Pin, INPUT_PULLDOWN);
  pinMode(rcIn0Pin, INPUT_PULLDOWN);

  attachInterrupt(digitalPinToInterrupt(rcIn3Pin), interruptA, CHANGE);  
  attachInterrupt(digitalPinToInterrupt(rcIn2Pin), interruptB, CHANGE);  
  attachInterrupt(digitalPinToInterrupt(rcIn1Pin), interruptC, CHANGE);  
  attachInterrupt(digitalPinToInterrupt(rcIn0Pin), interruptD, CHANGE);  
  
  pinMode(pwmOutPin, OUTPUT);
  pinMode(keyInPin, INPUT);
  pinMode(rpmInPin, INPUT);
  pinMode(auxIn1Pin, INPUT);
  pinMode(auxIn2Pin, INPUT);
  pinMode(auxMotorAPin, OUTPUT);
  pinMode(auxMotorBPin, OUTPUT);
  pinMode(hBridgeAPin, OUTPUT);
  pinMode(hBridgeBPin, OUTPUT);
  pinMode(hornOutPin, OUTPUT);
  pinMode(AuxLSOut2Pin, OUTPUT);
  pinMode(lockOutPin, OUTPUT);
  pinMode(unlockOutPin, OUTPUT);
  pinMode(heaterOutPin, OUTPUT);
  pinMode(mirrorheatOutPin, OUTPUT);
  pinMode(engineOnRelayOutPin, OUTPUT);
  pinMode(cabinOutPin, OUTPUT);
  pinMode(windowGNDInterruptOutPin, OUTPUT);
  pinMode(lightingOutPin, OUTPUT);
  pinMode(indicatorOutPin, OUTPUT);
  pinMode(AuxLSOut1Pin, OUTPUT);

  pinMode(LEDPin, OUTPUT);

  pinMode(hBridgeCurrentSensInPin, INPUT);

  writeoutputsLOW();                                          //Writing all outputs LOW
  delay(400);                                                  
  
  buttonNr[0].modulePin = rcIn0Pin;         //D
  buttonNr[1].modulePin = rcIn1Pin;         //C
  buttonNr[2].modulePin = rcIn2Pin;         //B
  buttonNr[3].modulePin = rcIn3Pin;         //A
  
  buttonNr[0].id = 1;   //D
  buttonNr[1].id = 2;   //C
  buttonNr[2].id = 3;   //B
  buttonNr[3].id = 4;   //A

  decayFunction[0].outputPin = lightingOutPin;        
  decayFunction[1].outputPin = indicatorOutPin;        
  decayFunction[2].outputPin = lockOutPin;         
  decayFunction[3].outputPin = unlockOutPin;           
  decayFunction[4].outputPin = cabinOutPin;
  decayFunction[5].outputPin = LEDPin;
  decayFunction[6].outputPin = hornOutPin;
  
  
  flashLED(10);
}

void loop() 
{
  // put your main code here, to run repeatedly:

  if ((millis() - prevFastLoopMillis) > fastLoopMax)     //Main high speed loop 
  {
    for (int i = 0; i<4; i++)                            //looping through all remote buttons
    {
      checkremoteInput(buttonNr[i]);
    }
    
    for (int i = 0; i < amountOfTimers; i++) 
    {      
      checkOutputTimers(decayFunction[i]);
    } 
    
    checkforLock();
    checkforUnlock();
    //windowControl();

    carFinderHandler();

    prevFastLoopMillis = millis();                        //resetting maindelay timer
  }
          
  if (millis() - prevSlowLoopMillis > slowLoopMax)          
  {         
    engineStateHandler();         
    prevSlowLoopMillis = millis();                  //Resetting slow timer          
  }         
  

}

void checkremoteInput(remoteModule &buttonToCheck)
{
  buttonToCheck.currentState = digitalRead(buttonToCheck.modulePin);
  if(buttonToCheck.lastState == LOW && buttonToCheck.currentState == HIGH)                    //button is pressed
  {
    buttonToCheck.pressedTime = millis();
    buttonToCheck.isPressing = true;
    buttonToCheck.isLongDetected = false;
  }
  else if(buttonToCheck.lastState == HIGH && buttonToCheck.currentState == LOW)               //button is released
  { 
    buttonToCheck.isPressing = false;
    buttonToCheck.releasedTime = millis();
    //pressDurationA = buttonToCheck.releasedTime - buttonToCheck.pressedTime;
    if(buttonToCheck.releasedTime - buttonToCheck.pressedTime < LONG_PRESS_MIN)
    {
  DEBUG_SERIAL.print("A short press is detected on input: ");                       //Short press detected
      DEBUG_SERIAL.println(buttonToCheck.id);
      buttonToCheck.shortpressFlag = true;
      flashLED(1);
    }
  }
  else if(buttonToCheck.isPressing == true && buttonToCheck.isLongDetected == false)
  {
    //pressDurationA = millis() - buttonToCheck.pressedTime;
    if(millis() - buttonToCheck.pressedTime > LONG_PRESS_MIN) 
    {
      DEBUG_SERIAL.print("A long press is detected on input: ");                     //Long press detected
      DEBUG_SERIAL.println(buttonToCheck.id);
      buttonToCheck.isLongDetected = true;
      buttonToCheck.longpressFlag = true;
      flashLED(5);
    }
  }
  buttonToCheck.lastState = buttonToCheck.currentState;
}

void flashLED(int timesToFlash)
{
  decayFunction[5].repeatTimes = timesToFlash;                
  decayFunction[5].decayTime = 40;
  decayFunction[5].prevTimerMillis = millis();
  decayFunction[5].run = true;
}

void checkforLock()
{
  DEBUG_SERIAL.println("Checking for lock");
  if (buttonNr[buttonA].shortpressFlag == true)
  {
    DEBUG_SERIAL.println("Lockrequest detected");
    lockCar();
  }
}

void checkforUnlock()
{
  DEBUG_SERIAL.println("Checking for unlock");
  if (buttonNr[buttonB].shortpressFlag == true)
  {
    DEBUG_SERIAL.println("Unlockrequest detected");
    unlockCar();
  }
}

void checkOutputTimers(decayOutput &timerToCheck)
{
  if (timerToCheck.run)
  {
    if (timerToCheck.repeatCounter < timerToCheck.repeatTimes)
    {
      if (millis() - timerToCheck.prevTimerMillis < timerToCheck.decayTime)
      {
        digitalWrite(timerToCheck.outputPin, HIGH);
      }

      else if (millis() - timerToCheck.prevTimerMillis > timerToCheck.decayTime && (millis() - timerToCheck.prevTimerMillis < (timerToCheck.decayTime * 2)))
      {
        digitalWrite(timerToCheck.outputPin, LOW);      
      }
      
      else if (millis() - timerToCheck.prevTimerMillis > (timerToCheck.decayTime * 2))
      {
        digitalWrite(timerToCheck.outputPin, LOW);
        timerToCheck.prevTimerMillis = millis();
        timerToCheck.repeatCounter ++;
        DEBUG_SERIAL.print("repeatCounter");
        DEBUG_SERIAL.println(timerToCheck.repeatCounter);      
      }
      
    }
    else
    {
      digitalWrite(timerToCheck.outputPin, LOW);
      timerToCheck.repeatCounter = 0;
      timerToCheck.run = false;
    }
  }
  
}

void lockCar()
{
  decayFunction[2].repeatCounter = 0;
  decayFunction[2].repeatTimes = 1;                     //Writing solenoid
  decayFunction[2].decayTime = 400;
  decayFunction[2].prevTimerMillis = millis();
  decayFunction[2].run = true;

  decayFunction[6].repeatCounter = 0;
  decayFunction[6].repeatTimes = 1;                     //Writing horn
  decayFunction[6].decayTime = 60;
  decayFunction[6].prevTimerMillis = millis();
  decayFunction[6].run = true;


  writeIndicators(1);
  writeLighting();

  buttonNr[buttonA].shortpressFlag = false;
}

void unlockCar()
{
  decayFunction[3].repeatCounter = 0;
  decayFunction[3].repeatTimes = 1;                     //Writing solenoid
  decayFunction[3].decayTime = 400;
  decayFunction[3].prevTimerMillis = millis();
  decayFunction[3].run = true;
  
  writeIndicators(2);
  writeLighting();

  buttonNr[buttonB].shortpressFlag = false;
}

void writeoutputsLOW()                                //Writing all outputs LOW by default
{
  digitalWrite(pwmOutPin, LOW);
  digitalWrite(keyInPin, LOW);
  digitalWrite(rpmInPin, LOW);
  digitalWrite(auxIn1Pin, LOW);
  digitalWrite(auxIn2Pin, LOW);
  digitalWrite(auxMotorAPin, LOW);
  digitalWrite(auxMotorBPin, LOW);
  digitalWrite(hBridgeAPin, LOW);
  digitalWrite(hBridgeBPin, LOW);
  digitalWrite(hornOutPin, LOW);
  digitalWrite(AuxLSOut2Pin, LOW);
  digitalWrite(lockOutPin, LOW);
  digitalWrite(unlockOutPin, LOW);
  digitalWrite(heaterOutPin, LOW);
  digitalWrite(mirrorheatOutPin, LOW);
  digitalWrite(engineOnRelayOutPin, LOW);
  digitalWrite(cabinOutPin, LOW);
  digitalWrite(windowGNDInterruptOutPin, LOW);
  digitalWrite(lightingOutPin, LOW);
  digitalWrite(indicatorOutPin, LOW);
  digitalWrite(AuxLSOut1Pin, LOW);
}


void carFinderHandler()                               //Long press to find car; audible signal?
{
  if (buttonNr[0].longpressFlag == true)
  {
    decayFunction[1].repeatTimes = 20;                     //Flashing indicators
    decayFunction[1].decayTime = 300;
    decayFunction[1].prevTimerMillis = millis();
    decayFunction[1].run = true;

    decayFunction[0].repeatTimes = 20;                     //Flashing lights
    decayFunction[0].decayTime = 300;
    decayFunction[0].prevTimerMillis = millis();
    decayFunction[0].run = true;

    decayFunction[6].repeatTimes = 4;                     //sounding horn
    decayFunction[6].decayTime = 65;
    decayFunction[6].prevTimerMillis = millis();
    decayFunction[6].run = true;
    
    buttonNr[0].longpressFlag = false;  
  }
}

void engineStateHandler()                             //Checking if engine is running by measuring battery voltage
{
  long measuredVoltage = map(analogRead(keyInPin), 0, 4960, 0, 15);
  DEBUG_SERIAL.print("MeasuredVoltage: ");
  DEBUG_SERIAL.println(measuredVoltage);

  if (measuredVoltage > 13)
  {
    digitalWrite(engineOnRelayOutPin, HIGH);
    digitalWrite(mirrorheatOutPin, HIGH);
  }
  else
  {
    digitalWrite(engineOnRelayOutPin, LOW);
    digitalWrite(mirrorheatOutPin, LOW);
  }
}
  
void writeIndicators(byte timesToBlink)
{
  decayFunction[1].repeatCounter = 0;
  decayFunction[1].repeatTimes = timesToBlink;                     //Writing indicators
  decayFunction[1].decayTime = 500;
  decayFunction[1].prevTimerMillis = millis();
  decayFunction[1].run = true;
}

void writeLighting()
{
  decayFunction[0].repeatCounter = 0;
  decayFunction[0].repeatTimes = 1;                     //Writing Lights
  decayFunction[0].decayTime = 20000;
  decayFunction[0].prevTimerMillis = millis();
  decayFunction[0].run = true;

  decayFunction[4].repeatCounter = 0;
  decayFunction[4].repeatTimes = 1;                     //Writing Cabin
  decayFunction[4].decayTime = 20000;
  decayFunction[4].prevTimerMillis = millis();
  decayFunction[4].run = true;
}

void windowControl()
{
  if (buttonNr[buttonA].longpressFlag == true)
  { 
    windowRunningDown = false;    
    prevWindowMillis = millis();
    windowRunningUp = true;
    buttonNr[buttonA].longpressFlag = false;
  }

  else if (buttonNr[buttonB].longpressFlag == true)
  { 
    windowRunningUp = false;    
    prevWindowMillis = millis();
    windowRunningDown = true;
    buttonNr[buttonB].longpressFlag = false;
  }

  if (windowRunningUp)
  {
    if (millis() - prevWindowMillis < windowDriveTime)
    {
      digitalWrite(hBridgeAPin, LOW);
      digitalWrite(hBridgeBPin, HIGH);
      
      digitalWrite(windowGNDInterruptOutPin, HIGH);
      digitalWrite(AuxLSOut1Pin, HIGH);
      DEBUG_SERIAL.println("Writing Window Up");           
    }
    else 
    {
      digitalWrite(AuxLSOut1Pin, LOW);
      digitalWrite(hBridgeAPin, LOW);
      digitalWrite(hBridgeBPin, LOW);
      digitalWrite(windowGNDInterruptOutPin, LOW);

      buttonNr[buttonA].longpressFlag = false;
      windowRunningUp = false;      
      
    }
  }

  else if (windowRunningDown)
  {
    if (millis() - prevWindowMillis < windowDriveTime)
    {
      digitalWrite(hBridgeAPin, HIGH);
      digitalWrite(hBridgeBPin, LOW);
      
      digitalWrite(windowGNDInterruptOutPin, HIGH);
      digitalWrite(AuxLSOut1Pin, HIGH);
      DEBUG_SERIAL.println("Writing Window Down");           
    }
    else 
    {
      digitalWrite(AuxLSOut1Pin, LOW);
      digitalWrite(hBridgeAPin, LOW);
      digitalWrite(hBridgeBPin, LOW);
      digitalWrite(windowGNDInterruptOutPin, LOW);

      buttonNr[buttonB].longpressFlag = false;
      windowRunningDown = false;
    }
  }

  DEBUG_SERIAL.print("Currentsense: ");
  DEBUG_SERIAL.println(analogRead(hBridgeCurrentSensInPin));
    
  currentReadings[currentReadingIndex] = analogRead(hBridgeCurrentSensInPin);
  
  int currentSenseTotal = 0;
  for (int i = 0; i < amountOfCurrentReadings; i ++)
  {
    currentSenseTotal = currentSenseTotal +  currentReadings[i];
    DEBUG_SERIAL.print(" , ");
    DEBUG_SERIAL.print(currentReadings[i]);
  }
  DEBUG_SERIAL.print(", CurrentSenseTotal: ");
  DEBUG_SERIAL.print(currentSenseTotal);
  
  currentSenseAverage = currentSenseTotal / amountOfCurrentReadings;
  
  DEBUG_SERIAL.print(" , CurrentSenseAverage: ");
  DEBUG_SERIAL.print(currentSenseAverage);
  
  currentReadingIndex ++;
  if (currentReadingIndex > amountOfCurrentReadings) 
  {
    currentReadingIndex = 0;    
  }

  DEBUG_SERIAL.print(" ,currentreadingIndex: ");
  DEBUG_SERIAL.println(currentReadingIndex);
} 

  // buttonNr[0].id = 1;   //D
  // buttonNr[1].id = 2;   //C
  // buttonNr[2].id = 3;   //B
  // buttonNr[3].id = 4;   //A

void interruptA()
{
  buttonNr[3].currentState = digitalRead(buttonNr[3].modulePin);
}

void interruptB()
{
  buttonNr[2].currentState = digitalRead(buttonNr[2].modulePin);
}

void interruptC()
{
  buttonNr[1].currentState = digitalRead(buttonNr[1].modulePin);  
}

void interruptD()
{
  buttonNr[0].currentState = digitalRead(buttonNr[0].modulePin);  
}
  