/////////////////////////////////////////////////////////////////////////
//
// Andromeda front panel controller by Laurence Barker G8NJJ
//
// this sketch provides a knob and switch interface through USB and CAT
//
// button.c
// this file holds the code to debounce pushbutton inputs scanned from a matrix
/////////////////////////////////////////////////////////////////////////

#include "globalinclude.h"
#include "types.h"
#include "button.h"
#include "iopins.h"
#include "cathandler.h"
#include "configdata.h"
#include "encoders.h"
#include "led.h"
#include <Wire.h>                             // for Andromeda h/w MCP23017


//
// enum for the states of the matrix scan sequencer
//
enum EScanStates 
{
  eIdle,                            // no button pressed
  eWaitPressed,                     // single button pressed - debounce
  eButtonPressed,                   // single button has been pressed
  eWaitReleased,                    // release state from a single press - debounce
  eMultiPressed,                    // more thna one pressed - wait until released
  eWaitMultiReleased                // debounce state for release from multiple buttons pressed
};



//
// switch matrix
// the matrix has 5 column outputs driven by GPIOA(4:0)
// the remaining 3 o/p bits are LEDs: GPIOA(7:5)
// the 8 row inputs are read on GPIOB(7:0)
//
#define VNUMROWS 8
#define VNUMCOLS 5
#define VCOLUMNMASK 0b00011111;
EScanStates GScanState;
byte GScanColumn;                   // scanned column number, 0...4
byte GFoundRow;                     // row where a bit detected
byte GDebounceTickCounter;          // delay counter (units of 2ms)


//
// defines for the MCP23017 address and registers within it
// MCP23017 operated with IOCON.BANK=0
//
#define VMCPMATRIXADDR 0x21         // MCP23017 address in I2C
#define VGPIOAADDR 0x12             // GPIO A (column and LED out)
#define VGPIOBADDR 0x13             // GPIO B (row input)
#define VIODIRAADDR 0x0             // direction A
#define VGPPUA 0x0C                 // pullup control for GPIO A
#define VGPPUB 0x0D                 // pullup control for GPIO B


//
// function to read 8 bit input from MCP23017
// returns register value
//
byte ReadPushbuttonRowMCP(void)
{
  byte Input;                                   // becomes the new bit sequence for an input
  Wire.beginTransmission(VMCPMATRIXADDR);
  Wire.write(VGPIOBADDR);                               // point to GPIOB register
  Wire.endTransmission();
  Wire.requestFrom(VMCPMATRIXADDR, 1);                      // read 1 byte
  Input=Wire.read();                                    // GPIOB
  return Input;
}


//
// function to process row input and see if only 1 bit is set
// output 0: no bits set; 1-8: row bit 0-7 is set; FF: more than one bit set
//
//
byte AnalyseRowInput(byte RawInput)
{
  byte Result = 0;
  byte Cntr;

  for(Cntr = 0; Cntr < VNUMROWS; Cntr++)      // step through all row inputs
  {
    if ((RawInput & 1) == 0)                  // test bottom bit. If we have a zero input, something is pressed
    {
      if (Result == 0)                        // if we haven't found a bit set, set the current one
        Result = Cntr+1;
      else
        Result = 0xFF;                        // else set more than 1 pressed
    }
    RawInput = RawInput >> 1;                 // move onto mext bit
  }
  return Result;
}


//
// function to write 8 bit value to MCP23017
// returns register value
//
void WriteMCPRegister(byte ChipAddress, byte Address, byte Value)
{
  Wire.beginTransmission(ChipAddress);
  Wire.write(Address);                                  // point to register
  Wire.write(Value);                                    // write its data
  Wire.endTransmission();
}



//
// function to drive new column output
// this should be at the END of the code to allow settling time
// this works by having fixed GPIO data, and selectively enabling bits as outputs. 
// For column outputs this makes them like open drain so they are never driven to a 1.
//
void AssertMatrixColumn()
{
  byte Column;

  Column = 1 << GScanColumn;                            // get a 1 in the right bit position
  Column = Column & VCOLUMNMASK;                        // now have a 1 in the right bit position
  Column |= (I2CLEDBits << VNUMCOLS);                   // add in LED bits at the top
  WriteMCPRegister(VMCPMATRIXADDR, VIODIRAADDR, ~Column);       // drive 0 to enable output bits to pre-defined state
}




//
// initialise
// init all scanning variables, and assert 1st column
//
void GButtonInitialise(void)
{
  GScanState = eIdle;                                 // initialise the sequencer
  GScanColumn = 0;                                    // initial column
  GFoundRow = 0;
  GDebounceTickCounter = 0;
  WriteMCPRegister(VMCPMATRIXADDR, VIODIRAADDR, 0xFF);                // make Direction register A = FF (all input)
  WriteMCPRegister(VMCPMATRIXADDR, VGPIOAADDR, 0b11100000);           // make GPIO register A assert LEDS to 1, columns to 0
  WriteMCPRegister(VMCPMATRIXADDR, VGPPUB, 0xFF);                     // make row inputs have pullup resistors
  I2CLEDBits = 0;                                     // I2C wired LEDs off
  AssertMatrixColumn();
}


//
// arrays to look up the report code from the software scan code
// s/w scan code begins 0
// reported code begins 1-7 (encoders) then 21-49 (pushbuttons)
//
int ReportCodeLookup[] = 
{
  11,      // scan code 0
  21,
  22,
  23,
  24,
  25,
  1, 
  26, 
  5,
  7, 
  29,     // scan code 10
  30,
  31,
  32,
  33,
  34,
  27,
  3,
  35,
  36,
  37,     // scan code 20
  38,
  39,
  40,
  41,
  42,
  43,
  9,
  44,
  45,
  46,     // scan code 30
  47, 
  48,
  0,
  49
};



//
// get a scan code from column and row number
// returns 0xFF if an invalid result
// 0...N-1
//
byte GetScanCode()
{
  byte Result;

  if (GFoundRow == 0xFF)                            // invalid scan if more than one button pressed
    Result = 0xFF;
  else
    Result = (GScanColumn << 3) + GFoundRow - 1;
  return Result;
}


//
// called when a button is pressed
// the parameter is the button matrix number (0..N-1)
//
void ButtonPressed()
{
  
  byte ButtonCode;             // message report code
  byte ScanCode;
  
  ScanCode = GetScanCode();
  if (ScanCode != 0xFF)
  {
    ButtonCode = ReportCodeLookup[ScanCode];
    if (ButtonCode != 0)
      CATHandlePushbutton(ButtonCode, true, false); 
  }
}



//
// called when a button is released
// the parameter is the button matrix number (0..N-1)
//
void ButtonReleased()
{
  byte ButtonCode;             // message report code
  byte ScanCode;

  ScanCode = GetScanCode();
  if (ScanCode != 0xFF)
  {
    ButtonCode = ReportCodeLookup[ScanCode];
    if (ButtonCode != 0)
      CATHandlePushbutton(ButtonCode, false, false); 
  }
}


#define VDEBOUNCETICKS 5

//
// Tick
// read row input, then run the sequencer. 
// The sequencer advances to next column only when no buttons pressed. 
// only accept a "pressed" indication if one input only is asserted
//
void ButtonTick(void)
{

  byte Row;                                 // row input read from matrix
  Row = ReadPushbuttonRowMCP();             // read raw row value
  Row = AnalyseRowInput(Row);               // check whether none, one or more than one button pressed


  if (GDebounceTickCounter != 0)            // if delay counter isn't 0, count down delay
    GDebounceTickCounter--;
  else                                      // else step the sequencer
  {
    switch(GScanState)
    {
      case eIdle:                           // no button pressed
        if (Row == 0)                       // still not pressed - advance column                
        {
          if (++GScanColumn >= VNUMCOLS)
            GScanColumn = 0;
        }
        else if (Row == 0xFF)               // more than one button pressed
        {
          GScanState = eMultiPressed;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        else                                // single button pressed
        {
          GScanState = eWaitPressed;
          GFoundRow = Row;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        break;
  
      case eWaitPressed:                    // single button pressed - debounce
        if (Row == GFoundRow)               // same button pressed
        {
          GScanState = eButtonPressed;
          ButtonPressed();                  // action the button press
        }
        else                                // single button pressed
        {
          GScanState = eMultiPressed;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        break;
  
      case eButtonPressed:                  // single button has been pressed
        if (Row == 0)                       // 1st detect of button released
        {
          GScanState = eWaitReleased;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        else if (Row != GFoundRow)          // multiple or different button pressed
        {
          GScanState = eMultiPressed;
          GDebounceTickCounter = VDEBOUNCETICKS;
          ButtonReleased();
        }
        break;
      
      case eWaitReleased:                   // release state from a single press - debounce
        if (Row == 0)                       // button now released after debounce
        {
          GScanState = eIdle;
          GDebounceTickCounter = VDEBOUNCETICKS;
          ButtonReleased();
        }
        else if (Row != GFoundRow)          // multiple or different button pressed
        {
          GScanState = eMultiPressed;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        break;
      
      case eMultiPressed:                   // more thna one pressed - wait until released
        if (Row == 0)                       // 1st detect of button released
        {
          GScanState = eWaitMultiReleased;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        break;
      
      case eWaitMultiReleased:              // debounce state for release from multiple buttons pressed
        if (Row == 0)                       // button confirmed released after debounce
        {
          GScanState = eIdle;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        else                                // there is still something pressed during debounces, so go back to waiting
        {
          GScanState = eMultiPressed;
          GDebounceTickCounter = VDEBOUNCETICKS;
        }
        break;
    }
  }
}
