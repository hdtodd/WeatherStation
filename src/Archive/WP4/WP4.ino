/*
 Weather_Probe V4.2
 
 In response to queries from a USB-connected Raspberry Pi, gather and report back
 meteorological data using the DS18B20 temp sensor, the DHT22 humidity/temp sensor, 
 and the MPL3115A2 altitude/pressure/temperature sensor.

 Date, time and temperature functions also collected from a Chronodot RTC.

 OneWire library used for communication with the DS18B20 sensor.
 
 MPL3115A2 and Chronodot communications handled by the I2C library of Wayne Truchsess
 to avoid the lost bits and unsynchronized byte frames that were common when
 using the Wire library, apparently caused by the "repeat-start" method used by 
 Freescale but not supported well by Wire.  Highly stable with I2C library.
 MPL3115A2 and Chronodot device handlers rewritten to use the I2C library.
 
 MPL3115A code based on code by: A.Weiss, 7/17/2012, changes by 
 Nathan Seidle Sept 23rd, 2013 (SparkFun), and Adafruit's TFT Library

 Revisions by HDTodd, August, 2016 and February, 2016:
 V4, 2016\02\05 - 2016\08\10
 Add "WhoRU" command response to identify WeatherProbe software
 Restructure and extend WP code in V4:
   -  Modularize code so program flow is easier to understand and modify
   -  Modify code so that it fails gracefully when a device is not there
   -  Incorporate code to collect and report data from DS18B20 One-Wire 
      connected thermal sensors

V3.1, 2016\01\26
   -  Revise to use pseudo-clock (based on millis() ) in revised 
   -  ChronodotI2C code if hardware Chronodot clock not installed.  
   -  Use internal clock for time-stamping data if a RTC device isn't
      installed

V3, 2015\10
      Reliable working version with CSV, XML, and printout output modes functioning

V2, 2015\08
   Prototyping of basic probe code with just clock and DHT22 supported

-----------------------------------------------------------------------------------------------
Setup and Code Management

 Uses the F() macro to move strings to PROGMEN rather than use from SRAM; see
    https://learn.adafruit.com/memories-of-an-arduino/optimizing-sram
 
 License: This code is public domain but you buy those original authors a beer if you use this 
 and meet them someday (Beerware license).
 
 Edit .h file if you change connection pins!!!
 
 Hardware Connections to MPL3115A2 (Breakoutboard to Arduino):
 -VCC = 3.3V
 -SDA = A4    Add 330 ohm resistor in series if using 5v VCC
 -SCL = A5    Add 330 ohm resistor in series if using 5v VCC
 -GND = GND
 -INT pins can be left unconnected for this demo
 
 Hardware connections to the DS18B20
 -VCC (red)     = 3.3V DOES NOT USE PARASITIC DATA/POWER 
 -GND (black)   = GND 
 -DATA (yellow) = A5 with 4K7 ohm pullup resistor to VCC
 
 Usage:
 -Serial terminal at 9600bps
 -Times various sensor measurements
 -Examines status flags used to poll device for data ready
 */
 
#include <stdio.h>
 
#include <I2C.h>                   // for IIC communication

#include "ChronodotI2C.h"
Chronodot RTC;                     // Set up a clock (Chronodot) instance
RTC_Millis Timer;                  //   and set up a pseudo clock in case no hardware

#include "MPL3115A2.h"
MPL3115A2 baro;                    // This is the barometer we'll be sampling
uint8_t sampleRate=S_128;          // Sample 128 times for each MPL3115A2 reading

#include <TFT.h>                   // Arduino LCD library for TFT display using SPI comm
#include <SPI.h>
#define cs   10                    // TFT pin definitions for SPI on the Uno
#define dc   9
#define rst  8
#define TFT_BACKGROUND 0,0,0
TFT TFTscreen = TFT(cs, dc, rst);   // create an instance of the TFT

#include "DHT.h"                    // Connect to pin 2 on the Arduino Uno
#define DHTPIN 2                    // connect a 4.7K - 10K resistor between VCC and data pin
#define DHTTYPE DHT22               // the model of our sensor -- could be DHT11 or DHT21                                    
DHT myDHT22(DHTPIN, DHTTYPE);       // create the sensor object                                    
                                    
#define V "4.1"
#define UPDATE_FREQ 60000           // update TFT every 60 sec if no Pi command comes in
#define MY_ALTITUDE 1520              // Set this to your actual GPS-verified altitude in meters
                                    // if you want altitude readings to be corrected
                                    // Bozeman 1520, Williston 40
#define FT_PER_METER 3.28084        // conversion

typedef enum rptModes {none=0, printout, csv, xml};
rptModes rptMode=none;              // by default we don't print sensor data, just update TFT 
boolean haveDisplayed=false, gotCmd=true, timedOut=false;
boolean haveRTC, noDHT22=false, noMPL3115=false, noDS18=false;

void(* restartFunc) (void) = 0;	   //declare restart function at address 0

void setup(void)
{
  // This delay is required by the crock that is called systemd
  // If developers ever come to their senses and abandon it, remove the delay
  //  delay(15000);
  
  // start serial port
  Serial.begin(9600);
  Serial.println("WP running");
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

  baro = MPL3115A2();               // create barometer
  if (!baro.begin()) {
    Serial.println(F("Can't init MPL3115A2!"));
    delay(1000);
    noMPL3115=true;
  } else {
  baro.setOversampleRate(sampleRate);
  baro.setAltitude(MY_ALTITUDE);   // try with MY_ALTITUDE set to 0 and see if altimeter is
  };                                 // accurate; if not, set MY_ALTITUDE to GPS-provided altitude in meters
                                 
  myDHT22.begin();                 // Create temp/humidity sensor
  delay(2000);                     // Specs require at least at 2 sec delay before first reading  

// following line sets the RTC to the date & time this sketch was compiled
// uncomment if Chronodot hasn't been set previously
// RTC.adjust(DateTime(__DATE__, __TIME__));

// And finally, announce ourselves in an XML-compatible manner
// Note that an automated data collector should be prepared to clear the input buffer in 
//   order to ignore extraneous inputs like this
//   Serial.println(F("<!-- Arduino-Based I2C Weather Probe for Raspberry Pi -->"));   
}


/*  In main loop, make a first pass to collect data from the sensors and
    display it on the TFT.  Then loop for at most 'UPDATE_FREQ' Msec, waiting for a 
    command from the Pi.  If we get a command, act on it.  If it causes
    a data sampling (and TFT update), reset the timer.  It it doesn't cause
    a data sampling, go back into the loop to wait for the 60-sec timer.  
    If the 60-sec timer expires (with no command having come in), just
    go through the loop to collect sensor data and update the TFT
*/
void loop(void)
{ 
  DateTime now;
  float tf, tc, rh, baroAlt, baroPress, baroTemp;
  long startTime, marker, elapsedTime=0;
  char ts[21];                     // temp string for conversions
  // char arrays to print to the TFT and then erase from the TFT
  char timeDisplay[9], tempDisplay[17], rhDisplay[8], pDisplay[10];
  char stc[5], stf[5];
  String cmdNames[] = {"sample", "printout", "csv", "xmlstart", "xmlstop", "help", "whoRU", "restart", "unrecognized"};
  typedef enum cmds {csample=0, cprintout, ccsv, cxmlstart, cxmlstop, chelp, cwhoru, crestart, noCmd};
  int i=0,j=0;
  String cmd;
  
  marker = startTime = millis();            // Time the loop & note time for elapsed
  if (!haveDisplayed) while (Serial.available() > 0) Serial.read();           // clear incoming buffer

  if ( haveRTC ) 
    now = RTC.now();
  else
    now = Timer.now();
  sprintf(ts, "%4d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),
      now.hour(),now.minute(), now.second() );
  
  if(noMPL3115) {
    baroPress=0.0;
    baroAlt=0.0;
    baroTemp=0.0;
  }
  else {
    baroPress=baro.readPressure();   // Get MPL3115A2 data
    baroAlt=baro.readAltitude();
    baroTemp=baro.readTemp();
  };
  
  tc=myDHT22.readTemperature(false);      // Get DHT22 data
  tf=myDHT22.readTemperature(true);
  rh=myDHT22.readHumidity();
//  Serial.print("gotCmd="); Serial.print(gotCmd?"true":"false"); Serial.print("\ttimedOut="); Serial.println(timedOut?"true":"false");  
  if (gotCmd  && !timedOut) switch (rptMode) {      // If we didn't time out, it was a 'sample' command -- do we report?
    case none:
      break;
      
    case printout:                 // report-style printout
      Serial.print(ts);
      Serial.print("\tAltitude: ");
      Serial.print(baroAlt, 1);
      Serial.print(" m\t");
      Serial.print("Pressure: ");
      Serial.print(baroPress, 0);
      Serial.print(" Pa\t");
      Serial.print("Temp: ");
      Serial.print(baroTemp, 1);
      Serial.print(" C\t");
      Serial.print("DHT22: ");
      Serial.print(tc,1);
      Serial.print("C = ");
      Serial.print(tf,1);
      Serial.print("F @ ");
      Serial.print(rh,0);
      Serial.print("% RH\t");
      Serial.print("Loop time: ");
      Serial.print(millis() - startTime);
      Serial.println(" ms");
      break;

    case csv:                    // CSV printout for database or spreadsheet
      Serial.print("(\'");       
      Serial.print(ts);          Serial.print("\',");
      Serial.print(baroPress,0); Serial.print(",");
      Serial.print(baroTemp,1);  Serial.print(",");
      Serial.print(tc,1);        Serial.print(",");
      Serial.print(rh,0);        
      Serial.println(")");
      break;
         
    case xml:                    // encapsulate data sample in XML wrapper
      Serial.println(F("<sample>"));
      Serial.print(F("<date_time>'"));                Serial.print(ts);          Serial.println(F("'</date_time>"));
      Serial.println(F("<MPL3115A2>"));
      Serial.print(F("<mpl_press p_unit=\"Pa\">")); Serial.print(baroPress,0); Serial.println(F("</mpl_press>"));
      Serial.print(F("<mpl_temp t_scale=\"C\">"));  Serial.print(baroTemp,1);  Serial.println(F("</mpl_temp>"));
      Serial.println(F("</MPL3115A2>"));  
      Serial.println(F("<DHT22>"));    
      Serial.print(F("<dht_temp t_scale=\"C\">"));  Serial.print(tc,1);        Serial.println(F("</dht_temp>"));
      Serial.print(F("<dht_rh unit=\"\%\">"));      Serial.print(rh,0);        Serial.println(F("</dht_rh>")); 
      Serial.println(F("</DHT22>"));       
      Serial.println(F("</sample>"));      
      break;
  }

  if (haveDisplayed) {                    // If there's something on the TFT clear it
    TFTscreen.stroke(TFT_BACKGROUND);     // same color as background
    TFTscreen.text(timeDisplay, 0, 0);
    TFTscreen.text(tempDisplay, 0, 30);
    TFTscreen.text(rhDisplay, 0, 60);
    TFTscreen.text(pDisplay, 0, 90);
  }

  // print the new values to the TFT
  sprintf(timeDisplay, "%02d:%02d:%02d", now.hour(), now.minute(), now.second() );   
  dtostrf(baroPress, 6, 0, ts);
  sprintf(pDisplay, "%s Pa", ts); 
  dtostrf(tc, 3, 1, stc);
  dtostrf(tf, 3, 1, stf);
  sprintf(tempDisplay, "%s C=%s F", stc, stf);
  tempDisplay[4] = tempDisplay[11] = 0xF7;  // degree char on TFT
  dtostrf(rh, 2, 0, stc);
  sprintf(rhDisplay, "%s %%RH", stc);
  TFTscreen.stroke(127, 255, 255);      // set the font color
  TFTscreen.text(timeDisplay, 0, 0);
  TFTscreen.text(pDisplay, 0, 90);
  TFTscreen.stroke(255,127,255);        // diff color for temp/humidity
  TFTscreen.text(tempDisplay, 0, 30);
  TFTscreen.text(rhDisplay, 0, 60);
  haveDisplayed = true;

  timedOut = false;
  gotCmd = false;
  while (!timedOut && !gotCmd) {       // get response or timeout
    while (!(gotCmd=(Serial.available() > 0)) 
           && !(timedOut=((millis()-marker) > UPDATE_FREQ))) {delay(1);};
    if (gotCmd) {                       //  we received a command: get and parse it
      cmd = Serial.readStringUntil('\n');
      if (cmd.equalsIgnoreCase("?")) cmd="help";
      for (i = (int) csample; i < (int) noCmd; i++) {
        if (cmd.equalsIgnoreCase(cmdNames[i])) break;
      }
      switch (i) {
        case csample:
          break;                       // leave cmd/delay loop and go do sample
        case cprintout:
          rptMode = printout;
          gotCmd = false;              // only the "sample" command exits this delay loop unless we time out
          break;
        case ccsv:
          rptMode = csv;
          gotCmd = false;
          break;
        case cxmlstart:
          rptMode = xml;
          Serial.println(F("<?xml version=\"1.0\" ?>"));
          Serial.println(F("<!DOCTYPE samples SYSTEM \"weather_data.dtd\">"));
          Serial.println(F("<samples>"));
          gotCmd = false;
          break;
        case cxmlstop:
          rptMode = none;
          Serial.println(F("</samples>"));
          gotCmd = false;
          break;
        case cwhoru:
	  Serial.print("WP V");
          Serial.println(V);
          gotCmd = false;
          break;
        case chelp:
          Serial.print("Command, one of: ");
          for (int j=(int) csample; j < ((int) noCmd); j++) {Serial.print(cmdNames[j]); Serial.print(" | ");};
          Serial.println("?");
          gotCmd = false;
          break;
	case crestart:
	  restartFunc();
	  break;
      };
    };
  };
}

