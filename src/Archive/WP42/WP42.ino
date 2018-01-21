/* Weather_Probe V4.2
 
   In response to queries from a USB-connected Raspberry Pi, gather
   and report back atmospheric data using the DS18B20 temp sensor,
   the DHT22 humidity/temp sensor,  and the MPL3115A2 
   altitude/pressure/temperature sensor.  Date, time and temperaturec
   functions are also collected using a Chronodot RTC.

   The OneWire library is used for communication with the DS18B20 
   sensor.  MPL3115A2 and Chronodot communications handled by the 
   I2C library of Wayne Truchsess to avoid the lost bits and 
   unsynchronized byte frames that were common when using the 
   Wire library, apparently caused by the "repeat-start" method used
   by Freescale but not supported well by Wire.  Highly stable with 
   the I2C library.

   The MPL3115A2 and Chronodot device handlers rewritten to use the
   I2C library.  The MPL3115A2 library is based on the MPLhelp3115A code
   by A.Weiss, 7/17/2012, with changes by Nathan Seidle Sept 23rd, 
   2013 (SparkFun).

   For the ST7735-based TFT display, Adafruit's TFT Library is used.

   Code and revisions to this program by HDTodd:
 
  v4.2  2017\06
     Finish modularizing code, verify correct operation, eliminate
     compilation warning messages, reduce code size as much as 
     possible, prepare for DB18 incorporation

  v4.1  2016\08
     Revised Feb-Aug 2016 to modularize code

   V4, 2016\02\17
     Restructuring and extending WP code in V4:
       -  Modularize code so program flow is easier to understand 
          and modify
       -  Modify code so that it fails gracefully if device absent
       -  Incorporate code to collect and report data from DS18B20
          One-Wire connected thermal sensors

   V3, 2015\10
     Reliable working version with CSV, XML, and report output modes
     functioning
   V3.1, 2016\01\26
     Revise to use pseudo-clock (based on millis() ) in revised 
     ChronodotI2C code if hardware Chronodot clock not installed.  
     Use internal clock for time-stamping data if a RTC device isn't
     installed
   V2, 2015\08
     Prototyping of basic probe code with just clock and DHT22 
     supported

----------------------------------------------------------------------
Setup and Code Management

 Uses the F() macro to move strings to PROGMEN rather than use from SRAM; see
    https://learn.adafruit.com/memories-of-an-arduino/optimizing-sram
 
   License: This code is public domain but you buy those original
     authors (Weiss, Seidle) a beer if you use this and meet them 
     someday (Beerware license).
 
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
     - Serial terminal at 9600bps
     - Times various sensor measurements
     - Examines status flags used to poll device for data ready
 */

#include <stdio.h>
#include <I2C.h>                   // for IIC communication
#include <SPI.h>
#include <TFT.h>                   // TFT display using SPI comm
#include "ChronodotI2C.h"          // Real-Time Clock
#include "MPL3115A2.h"             // MPL3115 alt/baro/temp
#include "DHT.h"                   // DHT22 temp/humidityc
#include "WP4.h"                   // and our own defs of pins etc
                                    
Chronodot  RTC;                    // Define the devices
RTC_Millis Timer;                  //   and software clock in case
MPL3115A2  baro;
TFT TFTscreen = TFT(cs, dc, rst);   // create an instance of the TFT
DHT myDHT22(DHTPIN, DHTTYPE);       // create the sensor object                                    

rptModes   rptMode=report;         // default to report mode
boolean    haveDisplayed=false, gotCmd=true, timedOut=false;
boolean    haveRTC, haveDHT22, haveMPL3115, haveDS18, haveTFT;
void reportOut(struct recordValues *rec);

/*
 * setup() procedure
 -----------------------------------------------------------------
*/
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
    Serial.println(F("RTC is NOT connected!  Emulating clock."));
    Timer.adjust(DateTime(__DATE__, __TIME__));  // set pseudo clock
  };

  haveTFT = ! ( findTFT() == tftMISSING );
  if (! haveTFT) Serial.println("TFT not found");

  haveTFT = true;
  if (haveTFT) {
    TFTscreen.begin();

    /*
     * clear the screen with a black background & set orientation
     * 0 ==> 0 Deg rotation, 1==> 90 Deg rot, etc.
     */
    TFTscreen.background(TFT_BACKGROUND);
    TFTscreen.setRotation(0);

    /*
     * set the font color to white & set the font size
     */
    TFTscreen.stroke(255, 255, 255);
    TFTscreen.setTextSize(2);
  }; 

  baro = MPL3115A2();               // create barometer
  if (baro.begin()) {               // is device there?
    haveMPL3115 = true;             // yes, set parameters
    baro.setOversampleRate(sampleRate);
    baro.setAltitude(MY_ALTITUDE);  // try with MY_ALTITUDE set to 0;
                                    // see if altimeter is accurate;
                                    // if not, set MY_ALTITUDE to 
                                    // GPS-provided altitude in meters
  } else {
    haveMPL3115 = false;            // device not detected
    Serial.println(F("MPL3115A2 is NOT connected!"));
  };
                                 
  myDHT22.begin();                 // Create temp/humidity sensor
  delay(2000);                     // Specs require at least at 2 sec 
                                   // delay before first reading  
  haveDHT22 = myDHT22.read();      // see if it's there/operating
  if ( !haveDHT22 ) Serial.println(F("DHT22 is NOT connected!"));  
  /* And finally, announce ourselves in an XML-compatible manner
   * Note that an automated data collector should be prepared to clear 
   * the input buffer in order to ignore extraneous inputs like this
   */
   Serial.println(F("<!-- Arduino-Based Weather Probe for Raspberry Pi -->"));   
  delay(2000);                     // Specs require at least at 2 sec delay before first reading  

// following line sets the RTC to the date & time this sketch was compiled
// uncomment if Chronodot hasn't been set previously
// RTC.adjust(DateTime(__DATE__, __TIME__));

// And finally, announce ourselves in an XML-compatible manner
// Note that an automated data collector should be prepared to clear the input buffer in 
//   order to ignore extraneous inputs like this
//   Serial.println(F("<!-- Arduino-Based I2C Weather Probe for Raspberry Pi -->"));   
}


/*
 * loop() procedure
 -----------------------------------------------------------------
 * In main loop, make a first pass to collect data from the sensors
 * and display it on the TFT.  Then wait for at most 'UPDATE_FREQ' 
 * msec, waiting for a command from the Pi.  If we get a command, 
 * act on it.  If it causes a data sampling (and TFT update), reset 
 * the timer.  It it doesn't cause a data sampling, go back into the 
 * loop to wait for the timer expiration.  If the timer expires 
 * (with no command having come in), just go through the loop to 
 * collect sensor data and update the TFT
 */
void loop(void) { 
  DateTime now;
  long startTime, marker;
  char ts[21];                     // temp string for conversions
  // char arrays to print to the TFT and then erase from the TFT
  char timeDisplay[9], tempDisplay[17], rhDisplay[8], pDisplay[10];
  char stc[5], stf[5];
/*  String cmdNames[] = {"sample", "printout", "csv", "xmlstart", "xmlstop", "help", "whoRU", "restart", "unrecognized"};
  typedef enum cmds {csample=0, cprintout, ccsv, cxmlstart, cxmlstop, chelp, cwhoru, crestart, noCmd};
*/
  int i=0;
  String cmd;
  struct recordValues rec;
  
  marker = startTime = millis();   // Time the loop & note time for elapsed
  if ( !haveDisplayed ) 
    while (Serial.available() > 0) Serial.read();   // clear incoming buffer

  if ( haveRTC ) 
    now = RTC.now();
  else
    now = Timer.now();
    sprintf(rec.cd.dt, "%4d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),
      now.hour(),now.minute(), now.second() );
  
  if ( haveMPL3115 ) {
    rec.mpl.press = baro.readPressure();   // Get MPL3115A2 data
    rec.mpl.alt   = baro.readAltitude();
    rec.mpl.temp  = baro.readTemp();
  }
  else {
    rec.mpl.press = 0.0;
    rec.mpl.alt   = 0.0;
    rec.mpl.temp  = 0.0;
  };
  
  if ( haveDHT22) {
    rec.dht.tempf = myDHT22.readTemperature(false); // Get DHT22 data
    rec.dht.tempc = myDHT22.readTemperature(true);
    rec.dht.rh    = myDHT22.readHumidity();
  } else {
    rec.dht.tempf = 0.0;
    rec.dht.tempc = 0.0;
    rec.dht.rh    = 0.0;
  };
  /* Serial.print("gotCmd="); Serial.print(gotCmd?"true":"false"); 
     Serial.print("\ttimedOut="); Serial.println(timedOut?"true":"false");
   */
  if ( gotCmd  && !timedOut )      // If we didn't time out, it was a 
    switch (rptMode) {             // 'sample' command -- do we report?
    case none:
      break;
    case report:                 // report-style printout
      reportOut(&rec);
      Serial.print("Loop time: ");
      Serial.print(millis() - startTime);
      Serial.println(" ms");
      break;

    case csv:                    // CSV printout for database or spreadsheet
      Serial.print("(\'");       
      Serial.print(rec.cd.dt);          Serial.print("\',");
      Serial.print(rec.mpl.press,0); Serial.print(",");
      Serial.print(rec.mpl.temp,1);  Serial.print(",");
      Serial.print(rec.dht.tempc,1); Serial.print(",");
      Serial.print(rec.dht.rh,0);        
      Serial.println(")");
      break;
         
    case xml:                    // encapsulate data sample in XML wrapper
      Serial.println(F("<sample>"));
      Serial.print(F("<date_time>'"));
      Serial.print(rec.cd.dt);
      Serial.println(F("'</date_time>"));
      Serial.println(F("<MPL3115A2>"));
      Serial.print(F("<mpl_press p_unit=\"Pa\">"));
      Serial.print(rec.mpl.press,0); 
      Serial.println(F("</mpl_press>"));
      Serial.print(F("<mpl_temp t_scale=\"C\">"));
      Serial.print(rec.mpl.temp,1);
      Serial.println(F("</mpl_temp>"));
      Serial.println(F("</MPL3115A2>"));  
      Serial.println(F("<DHT22>"));    
      Serial.print(F("<dht_temp t_scale=\"C\">"));
      Serial.print(rec.dht.tempc,1);
      Serial.println(F("</dht_temp>"));
      Serial.print(F("<dht_rh unit=\"\%\">"));
      Serial.print(rec.dht.rh,0);
      Serial.println(F("</dht_rh>")); 
      Serial.println(F("</DHT22>"));       
      Serial.println(F("</sample>"));      
      break;
  }

  if (haveTFT) {
    if (haveDisplayed) {                 // If there's something on the
      TFTscreen.stroke(TFT_BACKGROUND);  // TFT clear it by reprinting 
      TFTscreen.text(timeDisplay, 0, 0); // with same color as background
      TFTscreen.text(tempDisplay, 0, 30);
      TFTscreen.text(rhDisplay, 0, 60);
      TFTscreen.text(pDisplay, 0, 90);
    }

    // print the new values to the TFT
    sprintf(timeDisplay, "%02d:%02d:%02d", now.hour(), now.minute(), now.second() );   
    dtostrf(rec.mpl.press, 6, 0, ts);
    sprintf(pDisplay, "%s Pa", ts); 
    dtostrf(rec.dht.tempc, 3, 1, stc);
    dtostrf(rec.dht.tempf, 3, 1, stf);
    sprintf(tempDisplay, "%s C=%s F", stc, stf);
    tempDisplay[4] = tempDisplay[11] = 0xF7;  // degree char on TFT
    dtostrf(rec.dht.rh, 2, 0, stc);
    sprintf(rhDisplay, "%s %%RH", stc);
    TFTscreen.stroke(127, 255, 255);      // set the font color
    TFTscreen.text(timeDisplay, 0, 0);
    TFTscreen.text(pDisplay, 0, 90);
    TFTscreen.stroke(255,127,255);        // diff color for temp/humidity
    TFTscreen.text(tempDisplay, 0, 30);
    TFTscreen.text(rhDisplay, 0, 60);
    haveDisplayed = true;
  };

  timedOut = gotCmd = false;
  while (!timedOut && !gotCmd) {
    while (!(gotCmd=(Serial.available() > 0))    // get response or timeout
           && !(timedOut=((millis()-marker) > UPDATE_FREQ))) delay(1);  
    if (gotCmd) {                       //  we received a command: get and parse it
      cmd = Serial.readStringUntil('\n');
      if (cmd.equalsIgnoreCase("?")) cmd="help";
      for (i = (int) vers; i < (int) noCmd; i++) {
        if (cmd.equalsIgnoreCase(cmdNames[i])) break;
        }
      switch (i) {
        case vers:
          Serial.println(Vers);
          gotCmd = false;              // only the "sample" command exits this delay loop unlesswe time out
          break;
        case csample:
          break;                       // leave cmd/delay loop and go do sample
        case creport:
          rptMode = report;
          gotCmd = false;
    TFTscreen.text(pDisplay, 0, 90);
  }

  // print the new values to the TFT
  sprintf(timeDisplay, "%02d:%02d:%02d", now.hour(), now.minute(), now.second() );   
  dtostrf(rec.mpl.press, 6, 0, ts);
  sprintf(pDisplay, "%s Pa", ts); 
  dtostrf(rec.dht.tempc, 3, 1, stc);
  dtostrf(rec.dht.tempf, 3, 1, stf);
  sprintf(tempDisplay, "%s C=%s F", stc, stf);
  tempDisplay[4] = tempDisplay[11] = 0xF7;  // degree char on TFT
  dtostrf(rec.dht.rh, 2, 0, stc);
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
      for (i = (int) vers; i < (int) noCmd; i++) {
        if (cmd.equalsIgnoreCase(cmdNames[i])) break;
      }
      switch (i) {
        case csample:
          break;                       // leave cmd/delay loop and go do sample
        case creport:
          rptMode = report;
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
          Serial.println(Vers);
          gotCmd = false;
          break;
        case chelp:
          Serial.print("Command, one of: ");
          for (int j=(int) vers; j < ((int) noCmd); j++) { 
	    Serial.print(cmdNames[j]); Serial.print(" | "); };
          Serial.println("?");
          gotCmd = false;
          break;
	case crestart:
	  restartFunc();
	  break;
      }; // end switch(i)
    };  // end for (i=vers)
  };  // end while (!TimedOut) && (!GotCmd)
}; // end loop()
  }
}

/*  Write 8-bit command to TFT, receive back 0-32 bits as a result word.
 *   
 *  SPI 4-wire TFT connection; uses SPI mode #0; see ST7735 data sheet
 *  Code taken from avrfreaks posting by David Prentice, Dec, 2015.
 */
uint32_t readwrite8(uint8_t cmd, uint8_t bits, uint8_t dummy) {
  uint32_t ret = 0;
  uint8_t val = cmd;

  digitalWrite(cs, LOW);
  digitalWrite(dc, LOW);
  pinMode(MOSI, OUTPUT);
  for (int i = 0; i < 8; i++) {   //send command
    digitalWrite(MOSI, (val & 0x80) == 0 ? LOW : HIGH);  // set next bit on SDA
    digitalWrite(SCK, HIGH);   // Tell slave to read that bit
    digitalWrite(SCK, LOW);    //  on high->low transition
    val <<= 1;
  }
    
  if (bits == 0) {               // nothing to get back, so return
    digitalWrite(cs, HIGH);
    digitalWrite(dc, HIGH);
    return 0;
  }
    
  pinMode(MOSI, INPUT_PULLUP);    // switch SDA pin mode so slave can send
  digitalWrite(dc, HIGH);        // switch to data mode [hdt: is this needed?]  
  for (int i = 0; i < dummy; i++) {  //any dummy clocks ?
    digitalWrite(SCK, HIGH);   // transition to HIGH on SCK but ignore data
    digitalWrite(SCK, LOW);
  }
  for (int i = 0; i < bits; i++) {  // read results
    ret <<= 1;
    if (digitalRead(MOSI)==HIGH) ret |= 1;;  // record that bit as 1 if signal HIGH
    digitalWrite(SCK, HIGH);    // SCK transition to HIGH asks for bit on SDA
    digitalWrite(SCK, LOW);
  }
  pinMode(MOSI, OUTPUT);
  digitalWrite(cs, HIGH);
  Serial.print("In readwrite8, ret = "); Serial.println(ret,HEX);
  return ret;
}

/*
 * findTFT()
 * Identifies the type of TFT we have, or flags it as missing if 
   none there or can't identify type.
*/
tftTYPE findTFT(void) {
  uint32_t ID = 0;

  // select the SPI slave, setup IO on digital pins
  pinMode(cs, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT_PULLUP);
  pinMode(dc, OUTPUT);
  pinMode(rst, OUTPUT);

  digitalWrite(rst, HIGH);
  digitalWrite(rst, LOW);           // do a hardware reset on the TFT
  delay(50);
  digitalWrite(rst, HIGH);
  ID = readwrite8(0x04, 24, 1);    // 0x04 ==> RDDID op code to ST7735
                                   // get 3 bytes back; ignore 1 dummy
  Serial.print("TFT ID = "); Serial.println(ID,HEX);                          
  if (ID == 0x7C89F0uL) return(tftST7735R);
  if (ID == 0x548066uL) return(tftILI9163C);
  return(tftMISSING);
};

void reportOut(struct recordValues *rec) {
      Serial.print(rec->cd.dt);
      Serial.print("\tAltitude: ");
      Serial.print(rec->mpl.alt, 1);
      Serial.print(" m\t");
      Serial.print("Pressure: ");
      Serial.print(rec->mpl.press, 0);
      Serial.print(" Pa\t");
      Serial.print("Temp: ");
      Serial.print(rec->mpl.temp, 1);
      Serial.print(" C\t");
      Serial.print("DHT22: ");
      Serial.print(rec->dht.tempc,1);
      Serial.print("C = ");
      Serial.print(rec->dht.tempf,1);
      Serial.print("F @ ");
      Serial.print(rec->dht.rh,0);
      Serial.print("% RH\t");
      return;
}
