/*
  Test TFT display
 */
 
#include <stdio.h>
#include <I2C.h>                   // for IIC communication

#include "ChronodotI2C.h"
Chronodot RTC;                     // Set up a clock (Chronodot) instance
RTC_Millis Timer;                  //   and set up a pseudo clock in case no hardware

#include <TFT.h>                   // Arduino LCD library for TFT display using SPI comm
#include <SPI.h>
#define cs   10                    // TFT pin definitions for SPI on the Uno
#define dc   9
#define rst  8
#define TFT_BACKGROUND 0,0,0
TFT TFTscreen = TFT(cs, dc, rst);   // create an instance of the TFT


boolean haveDisplayed=false, gotCmd=true, timedOut=false;
boolean haveRTC;

void(* restartFunc) (void) = 0;	   //declare restart function at address 0

void setup(void)
{

  Serial.begin(9600);
  I2c.begin();                      // join i2c bus
  I2c.setSpeed(1);                  // go fast

  RTC.begin();                      // init the clock
  if ( RTC.isrunning()) {
    haveRTC=true;
  }
  else {
    haveRTC=false;
    Serial.println(F("RTC is NOT running!"));
    Timer.adjust(DateTime(__DATE__, __TIME__));  // set starting time for pseudo clock
  };

  TFTscreen.begin();
  TFTscreen.background(TFT_BACKGROUND);  // clear the screen with a black background
  TFTscreen.setRotation(0);              // 0 ==> 0 Deg rotation, 1==> 90 Deg rot, etc.

  // write the static text to the screen
  TFTscreen.stroke(255, 255, 255);  // set the font color to white
  TFTscreen.setTextSize(2);         // set the font size
}


void loop(void) { 
  DateTime now;
  char ts[21];                     // temp string for conversions
  char timeDisplay[9];  // char arrays to print to the TFT and then erase from the TFT

  if ( haveRTC ) 
    now = RTC.now();
  else
    now = Timer.now();
  sprintf(ts, "%4d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),
      now.hour(),now.minute(), now.second() );

  if (haveDisplayed) {                    // If there's something on the TFT clear it
    TFTscreen.stroke(TFT_BACKGROUND);     // same color as background
    TFTscreen.text(timeDisplay, 0, 0);
    TFTscreen.text(timeDisplay, 0, 30);
    TFTscreen.text(timeDisplay, 0, 60);
  }

// print the new values to the TFT
  sprintf(timeDisplay, "%02d:%02d:%02d", now.hour(), now.minute(), now.second() );   

  TFTscreen.stroke(127, 255, 255);      // set the font color
  TFTscreen.text(timeDisplay, 0, 0);
  TFTscreen.stroke(255,127,255);        // diff color for temp/humidity
  TFTscreen.text(timeDisplay, 0, 30);
  TFTscreen.text(timeDisplay, 0, 60);
  haveDisplayed = true;
  delay(5000);
}


