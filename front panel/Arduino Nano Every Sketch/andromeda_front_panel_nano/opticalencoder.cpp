/////////////////////////////////////////////////////////////////////////
//
// opticalencoder.cpp
// this is an interrupt driven optical encoder driver written for
// the new Arduino Nano Every processor ATMEGA4809. 
// copyright (c) Laurence Barker G8NJJ 2019
//
// See "iom4809.h" for register definitions, bit maps etc
//
// the encoder is attached to D0 and D1 (PC04, PC05)
// and PORTC pin change will need to be enabled. 
/////////////////////////////////////////////////////////////////////////

#include <arduino.h>
#include "opticalencoder.h"
#include "iopins.h"



// global variables

signed char GDeltaCount;                    // count stored since last retrieved
byte GPinState;
byte GDivisor;                              // number of edge events per declared click


#define VENCODERPINS 0b00110000             // bitmap to select the two encoder inputs


//
// lookup table to get number of steps from 4 bits:
// bit 1,0 = old position
// bits 3:2 = new position
//
signed char StepsLookup[] = 
{
  0, 1, -1, 2,
  -1, 0, -2, 1,
  1, -2, 0, -1,
  2, -1, 1, 0
};


//
// set divisor
// this sets whether events are generated every 1, 2 or 4 edge events
// legal parameters are 1, 2 or 4 and this MUST be called!
//
void SetOpticalEncoderDivisor(byte EncoderDivisor)
{
  GDivisor = EncoderDivisor;
}


//
// initialise optical encoder.
// attach interrupt handler; set input pin modes; read initial state
//
void InitOpticalEncoder(void)
{
  pinMode(VPINVFOENCODERA, INPUT_PULLUP);               // VFO encoder
  pinMode(VPINVFOENCODERB, INPUT_PULLUP);               // VFO encoder
  delayMicroseconds(1000);                              // allow pins to settle
  GPinState = (PORTC.IN & VENCODERPINS) >> 2;           // bits 3:2
  PORTC.PIN4CTRL = (PORT_PULLUPEN_bm | 0b1);            // pullup, both edges interrupt
  PORTC.PIN5CTRL = (PORT_PULLUPEN_bm | 0b1);            // pullup, both edges interrupt
}



//
// VFO encoder pin interrupt handler
//
ISR(PORTC_PORT_vect)
{
  byte InputValue;
  signed char Increment;
  InputValue = (PORTC.IN & VENCODERPINS) >> 2;            // bits 3:2
  PORTC.INTFLAGS = 0b00110000;                            // clear interrupt flags
  GPinState = (GPinState >> 2) | InputValue;              // now have new bits in 3:2, old bits in 1:0
  Increment = StepsLookup[GPinState];
  GDeltaCount += Increment;
}


//
// read the optical encoder. Return the number of steps turned since last called.
// read back the count since last asked, then zero it for the next time
// if Divisor is above 1: leave behind the residue
//
signed char ReadOpticalEncoder(void)
{
  signed char Result;

  Result = GDeltaCount / GDivisor;                         // get count value
  GDeltaCount = GDeltaCount % GDivisor;                    // remaining residue for next time
  return Result;
}
