/////////////////////////////////////////////////////////////////////////
//
// Andromeda front panel controller by Laurence Barker G8NJJ
// this sketch provides a knob and switch interface through USB and CAT
// copyright (c) Laurence Barker G8NJJ 2019
//
// LED.c
// this file holds the code to control 8 LED indicators
/////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include "globalinclude.h"
#include "led.h"
#include "iopins.h"
#include "types.h"

byte I2CLEDBits;                  // 3 bits data for LEDs, in bits 2:0 (init to zero by button.cpp)
#define VLEDBITMASK 0b00000111    // bit mask for LED bits that are allowed to be set

//
// struct to hold an LED definition
// holds a pin number, or indicates which I2C bit
//
struct LEDType
{
  byte PinNumber;                 // I/O pin number or bit number
  bool IsI2C;                     // true if I2C connected
};

//
// array of I/O pins
// NOT THE RIGHT VALUES YET!
LEDType LEDPinList[] = 
{
  {VPININDICATOR1, false},
  {VPININDICATOR2, false},
  {VPININDICATOR3, false},
  {VPININDICATOR4, false},
  {VPININDICATOR5, false},
  {VPININDICATOR6, false},
  {VPININDICATOR7, false},
  {VPININDICATOR8, false},
  {VPININDICATOR9, false},
  {0, true},
  {1, true},
  {2, true}
};



//
// note LEDs numbered 0-(N-1) here!
//
void SetLED(byte LEDNumber, bool State)
{
  byte IOPin;
  byte BitPosition;
  
  if (LEDNumber < VMAXINDICATORS)
  {
    if(LEDPinList[LEDNumber].IsI2C== false)                 // if it is a GPIO pin
    {
      IOPin = LEDPinList[LEDNumber].PinNumber;
      if (State == true)
        digitalWrite(IOPin, HIGH);
      else
        digitalWrite(IOPin, LOW);
    }
    else                                                    // if it is connected to I2C
    {
      BitPosition = 1 << LEDPinList[LEDNumber].PinNumber;
      if (State == true)
        I2CLEDBits |= BitPosition;                          // set LED bit
      else
        I2CLEDBits &= ~BitPosition;                         // cancel LED bit

      I2CLEDBits &= VLEDBITMASK;                            // double check no others set
    }
  }
}


//
// clear all LEDs
//
void ClearLEDs(void)
{
  byte Cntr;

  for (Cntr = 0; Cntr < VMAXINDICATORS; Cntr++)
    SetLED(Cntr, false);
}
