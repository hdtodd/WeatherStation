/* Weather_Probe V5.1
 
   In response to queries from a USB-connected Raspberry Pi, gather
   and report back meterological  data using the DS18B20 temp sensor,
   the DHT22 humidity/temp sensor, and the MPL3115A2 
   altitude/pressure/temperature sensor.  Date, time and temperature
   values are also collected from a Chronodot RTC.

 MPL3115A2 and Chronodot communications handled by the I2C library of Wayne Truchsess
 to avoid the lost bits and unsynchronized byte frames that were common when
 using the Wire library, apparently caused by the "repeat-start" method used by 
 Freescale but not supported well by Wire.  Highly stable with I2C library.
 MPL3115A2 and Chronodot device handlers rewritten to use the I2C library.
 
 MPL3115A code based on code by: A.Weiss, 7/17/2012, changes by 
 Nathan Seidle Sept 23rd, 2013 (SparkFun), and Adafruit's TFT Library

   The OneWire library is used for communication with the DS18B20 
   sensor.  MPL3115A2 and Chronodot communications handled by the 
   I2C library of Wayne Truchsess to avoid the lost bits and 
   unsynchronized byte frames that were common when using the 
   Wire library, apparently caused by the "repeat-start" method used
   by Freescale but not supported well by Wire.  Highly stable with 
   the I2C library.

   The MPL3115A2 and Chronodot device handlers were rewritten to use the
   I2C library.  The MPL3115A2 library is based on the MPLhelp3115A code
   by A.Weiss, 7/17/2012, with changes by Nathan Seidle Sept 23rd, 
   2013 (SparkFun).

   For the ST7735-based TFT display, Adafruit's TFT Library is used.

   Code and revisions to this program by HDTodd:

  V5.1, 2017\01\17
    Add support for setting and getting time from Arduino from controller

  V5, 2017\08\31
     Add support for DS18B20 Onewire-based temp multi-probes
     OneWire library used for communication with the DS18B20 sensors
       http://www.pjrc.com/teensy/td_libs_OneWire.html
       http://milesburton.com/Dallas_Temperature_Control_Library
     See associated app tools to label sensors and characterize the DS18B20's
 
  V4.3 2017\06\27
     Major restructuring of main Probe loop to simplify structure
     and reduce code size to make room for DS18 addition 

  V4.2  2017\06
     Finish modularizing code, verify correct operation, eliminate
     compilation warning messages, reduce code size as much as 
     possible, prepare for DS18 incorporation

  V4.1  2016\08\10
     Revised Feb-Aug 2016 to modularize code

  V4, 2016\02\17
     Add "WhoRU" command response to identify WeatherProbe software
     Restructure and extend WP code in V4:
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
     - Serial terminal at 9600bps (edit setup() to change speed)
     - Times various sensor measurements
     - Examines status flags used to poll device for data ready

   Author:
     H. D Todd, Williston VT/Bozeman MT
     hdtodd@gmail.com
     2015-2017
 */

#include <stdio.h>
#include <TFT.h>                   // TFT display using SPI comm
#include <SPI.h>
#include <I2C.h>                   // for IIC communication
#include "OneWire.h"
#include "ChronodotI2C.h"          // Real-Time Clock
#include "MPL3115A2.h"             // MPL3115 alt/baro/temp
#include "DHT.h"                   // DHT22 temp/humidity
#include "DS18.h"		   // DS18B20 temp sensors
#include "WP.h"                   // and our own defs of pins etc
                                    
Chronodot  RTC;                    // Define the Chronodot clock
RTC_Millis Timer;                  //   and software clock in case no Chronodot
MPL3115A2  baro;		   // Barometer/altimeter/thermometer
TFT TFTscreen = TFT(cs, dc, rst);  // create an instance of the TFT
DHT myDHT22(DHTPIN, DHTTYPE);      // create the temp/RH object
DS18 ds18(oneWirePin);             // Create the DS18 object.

long startTime;
rptModes   rptMode=report;         // default to report mode
dsInfo dsList[DSMAX+1];		   // max number of DS devices + loop guard

boolean    haveRTC, haveDHT22, haveMPL3115, haveDS18, haveTFT;
int	   dsCount;
uint8_t    dsResMode=1;		   // use 10-bit for precision
void getTime(char dtString[20]);
void setTime(char *dtS);
void readSensors(struct recordValues *rec);
void updateTFT(struct recordValues *rec);
void reportOut(rptModes rptMode, struct recordValues *rec);
tftTYPE findTFT(void);
uint32_t readwrite8(uint8_t tftCmd, uint8_t tftBits, uint8_t TFTDummy);
/* following booleans cause sampling to be triggered on the
   first pass through the main loop */
boolean    haveDisplayed=false, gotCmd=false, timedOut=true;

/*
 * setup() procedure
 -----------------------------------------------------------------
*/
void setup(void) {
  // start serial port
  Serial.begin(9600,SERIAL_8N1);
  Serial.println("WP starting");
/*
  Do not reorder the following module startup sequences.  Some
  signal and timing dependences will cause component failures.
  I2C always before clock.
  The TFT device is particularly sensitive to timing and sequence.
*/
  haveTFT = ! ( findTFT() == tftMISSING );
  if (!haveTFT) Serial.println("[%WP] TFT not found");
  if (haveTFT) {
    TFTscreen.begin();
    /*
     * clear the screen with a black background & set orientation
     * 0 ==> 0 Deg rotation, 1==> 90 Deg rot, etc.
     */
    TFTscreen.background(TFT_BLACK);
    TFTscreen.setRotation(0);
    /*
     * set the font color to white & set the font size
     */
    TFTscreen.stroke(TFT_WHITE);
    TFTscreen.setTextSize(2);
  }; 

  // The following line sets the RTC to the date & time this sketch was compiled
  // uncomment if Chronodot hasn't been set previously
  // RTC.adjust(DateTime(__DATE__, __TIME__));

  // I2c before Chronodot
  I2c.begin();                      // join i2c bus
  I2c.setSpeed(1);                  // go fast
  RTC.begin();                      // init the clock
  haveRTC = RTC.isrunning();
  if (!haveRTC ) {
    Serial.println(F("[%WP] RTC is NOT connected!  Emulating clock."));
    Timer.adjust(DateTime((char *)__DATE__, (char *)__TIME__));  // set pseudo clock
  };

  baro = MPL3115A2();               // create barometer
  haveMPL3115 = baro.begin();	    // is MPL3115 device there?
  if ( haveMPL3115 ) {              // yes, set parameters
    baro.setOversampleRate(sampleRate);
    baro.setAltitude(MY_ALTITUDE);  // Set with known altitude
  } else {
    Serial.println(F("[%WP] MPL3115A2 is NOT connected!"));
  };
                                 
  myDHT22.begin();                 // Create temp/humidity sensor
  delay(2000);                     // Specs require at least at 2 sec 
                                   // delay before first reading  
  haveDHT22 = myDHT22.read();      // see if it's there/operating
  if ( !haveDHT22 ) Serial.println(F("[%WP] DHT22 is NOT connected!"));  

  haveDS18 = ds18.begin();
  if ( !haveDS18 ) {
     Serial.println(F("[%WP] DS18 probes NOT connected!")); }
   else {
     ds18.reset();
     ds18.reset_search();
     delay(dsResetTime);
     // Scan for address of next device on OneWire
     for (dsCount=0; (dsCount<DSMAX) && ds18.search(dsList[dsCount].addr) ; dsCount++) {
       // We have a device & space to store its info; verify it and record it or discount it
       // We'll discount if CRC isn't valid or if it's not a know DS18 type

       // First, confirm CRC of address to make sure we're getting valid data
       boolean validCRC=  (ds18.crc8(dsList[dsCount].addr, 7) == dsList[dsCount].addr[7]);

       // Confirm that it's a DS18 device and assign type
       dsList[dsCount].type = ds18.idDS(dsList[dsCount].addr[0]);

       // Ignore responses with invalid CRCs or from devices we don't know
       if ( !validCRC ||
            ( dsList[dsCount].type == DSNull  ) ||
            ( dsList[dsCount].type == DSUnkwn ) )
           dsList[dsCount--].alive = false;
         else
          dsList[dsCount].alive = true;
       };	  // end for (search for DS18 devices)

  // At this point, we've identified the DS18 devices, identified their types, and
  //   recorded their information
  // Make sure we have at least SOME DS18 devices and that we didn't have too many

  if (dsCount<=0) {
    Serial.println("[%WP] No valid DS18-class devices found!");
    haveDS18 = false;
  };
  if (dsCount>=DSMAX) {
    Serial.println(F("[%WP] Number of OneWire devices exceeds internal storage limit"));
    Serial.print(  F("             Only "));
    Serial.print(--dsCount);
    Serial.print(F(" DS18 devices will be sampled."));
  };

  // set the precisions for DS18 probe samplings
  for (int dev=0; dev<dsCount; dev++) {
    ds18.reset();
    ds18.select(dsList[dev].addr);
    ds18.setPrecision(dsList[dev].addr,dsResMode);
    };

  // clean up after DS18 device search and setup
  ds18.reset_search();          // reset search,
  delay(dsResetTime);           // must wait at least 250 msec for reset search
  };			        // end else (!haveDS)

  /* And finally, announce ourselves in an XML-compatible manner
   * Note that an automated data collector should be prepared to clear 
   * the input buffer in order to ignore extraneous inputs like this
   */
  Serial.println(F("<!-- Arduino-Based Weather Probe for Raspberry Pi -->"));   
  while (Serial.available() > 0) Serial.read();   // clear incoming buffer

}  				 // end setup()

/*
 * loop() procedure
 -----------------------------------------------------------------
 * In main loop, make a first pass to collect data from the sensors
 * and display it on the TFT.  Then wait for at most 'UPDATE_DELAY' 
 * msec, waiting for a command from the Pi.  If we get a command, 
 * act on it.  If it causes a data sampling (and TFT update), reset 
 * the timer.  It it doesn't cause a data sampling, go back into the 
 * loop to wait for the timer expiration.  If the timer expires 
 * (with no command having come in), just go through the loop to 
 * collect sensor data and update the TFT
 */
void loop(void) { 
  long marker;
  DateTime now;
  int cmd;
  String cmdString;
  struct recordValues rec;
  
/* Note start time for loop metrics and for timeout monitoring */
  marker = startTime = millis();   // Time the loop & note time for elapsed

/* Now wait for either timeout or a typed-in command */
  while (!timedOut && !gotCmd) {
    while (!(gotCmd=(Serial.available() > 0))    // get response or timeout
           && !(timedOut=((millis()-marker) > UPDATE_DELAY))) delay(1);  

/* Something happened -- timeout or command received? */
  if (gotCmd) {                       //  we received a command: get and parse it
    cmdString = Serial.readStringUntil('\n');
    cmdString.toLowerCase();
    if (cmdString.equals("?")) cmdString="help";
    for (cmd = (int)vers; cmd < (int)noCmd; cmd++) {
      if (cmdString.startsWith(cmdNames[cmd])) break;
      };
    } else {
      cmd = csample;	             // timedOut almost like sample
    };				     // end if (gotCmd) ... else
  };				     // end while (!gotCmd && !timedOut)		

/* Process that command */
  switch (cmd) {
    case vers:
    case cwhoru:
      Serial.println(Vers);
      break;
    case csample:
      readSensors(&rec);
      updateTFT(&rec);
      if (!timedOut) reportOut(rptMode, &rec);
      break;                       // leave cmd/delay loop and go do sample
    case creport:
      rptMode = report;
      break;
    case ccsv:
      rptMode = csv;
      break;
    case cxmlstart:
      rptMode = xml;
      Serial.println(F("<?xml version=\"1.0\" ?>"));
      Serial.println(F("<!DOCTYPE samples SYSTEM \"weather_data.dtd\">"));
      Serial.println(F("<samples>"));
      break;
    case cxmlstop:
      rptMode = none;
      Serial.println(F("</samples>"));
      break;
    case chelp:
    case noCmd:
      Serial.print(F("Command, one of: "));
      for (cmd=(int)vers; cmd < (int)noCmd; cmd++) { 
      Serial.print(cmdNames[cmd]); Serial.print(" | "); };
      Serial.println("?");
      break;
    case csettime:
      setTime((char *) (cmdString.substring(8)).c_str());
      break;
    case crestart:
      restartFunc();
      break;
    };				// end switch(cmd)

/* Reset trigger conditions and go to next loop iteration */
  timedOut = gotCmd = false;
}; // end loop()

void reportOut(rptModes rptMode, struct recordValues *rec) {

  switch (rptMode) {
  case none:
    return;
    break;
  case report:                 // report-style printout
    Serial.print(rec->cd.dt);
    Serial.print(F("  MPL: Altitude="));
    Serial.print(rec->mpl.alt, 1);
    Serial.print(F("m"));
    Serial.print(F(" Pressure="));
    Serial.print(rec->mpl.press, 0);
    Serial.print(F("Pa"));
    Serial.print(F(" Temp="));
    Serial.print(rec->mpl.tempf, 1);
    Serial.print(F("\xB0""F"));
    Serial.print(F("  DHT22: Temp="));
    Serial.print(rec->dht.tempf,1);
    Serial.print(F("\xb0""F @ "));
    Serial.print(rec->dht.rh,0);
    Serial.print(F("% RH"));
    Serial.print(F("  DS18: "));
    for (int dev=0; dev<DSMAX; dev++) {
      Serial.print(rec->ds18.label[dev]);
      Serial.write('=');
      Serial.print(rec->ds18.tempf[dev],1);
      Serial.print(F("\xB0""F "));
      };
    Serial.println();
    break;

  case csv:                    // CSV printout for database or spreadsheet
    Serial.print("(\'");       
    Serial.print(rec->cd.dt);          Serial.print("\',");
    Serial.print(rec->mpl.press,0);    Serial.print(",");
    Serial.print(rec->mpl.tempf,1);    Serial.print(",");
    Serial.print(rec->dht.tempf,1);    Serial.print(",");
    Serial.print(rec->dht.rh,0);       
    for (int dev=0; dev<dsCount; dev++) {
      Serial.print(",\'"); Serial.print(rec->ds18.label[dev]);
      Serial.print("\',"); Serial.print(rec->ds18.tempf[dev],1);
      };
    // If not DSMAX devices, output dummy 
    for (int dev=dsCount; dev<DSMAX; dev++) {
      Serial.print(","); Serial.print("\'**\',00.0");
      };
    Serial.println(")");
    break;
         
  case xml:                    // encapsulate data sample in XML wrapper
    Serial.println(F("<sample>"));
    Serial.print(F("<date_time>'"));
    Serial.print(rec->cd.dt);
    Serial.println(F("'</date_time>"));
    Serial.println(F("<MPL3115A2>"));
    Serial.print(F("<mpl_press p_unit=\"Pa\">"));
    Serial.print(rec->mpl.press,0); 
    Serial.println(F("</mpl_press>"));
    Serial.print(F("<mpl_temp t_scale=\"F\">"));
    Serial.print(rec->mpl.tempf,1);
    Serial.println(F("</mpl_temp>"));
    Serial.println(F("</MPL3115A2>"));  
    Serial.println(F("<DHT22>"));    
    Serial.print(F("<dht_temp t_scale=\"F\">"));
    Serial.print(rec->dht.tempf,1);
    Serial.println(F("</dht_temp>"));
    Serial.print(F("<dht_rh unit=\"\%\">"));
    Serial.print(rec->dht.rh,0);
    Serial.println(F("</dht_rh>")); 
    Serial.println(F("</DHT22>"));       
 
    for (int dev=0; dev<dsCount; dev++) {
      Serial.println(F("<DS18>"));
      Serial.print(F("<ds18_lbl>")); 
      Serial.print(rec->ds18.label[dev]);
      Serial.println(F("</ds18_lbl>"));
      Serial.print(F("<ds18_temp t_scale=\"F\">"));
      Serial.print(rec->ds18.tempf[dev],1);
      Serial.println(F("</ds18_temp>"));
      Serial.println(F("</DS18>"));
      };
    // If not DSMAX devices, output dummy 
    for (int dev=dsCount; dev<DSMAX; dev++) {
      Serial.println(F("<DS18>\n<ds18_lbl>**</ds18_lbl>\n<ds18_temp t_scale=\"F\">00.0</ds18_temp>\n</DS18>"));
      };


    Serial.println(F("</sample>"));      
    break;
  };				// end switch (rptMode)
};				// end void reportOut()

// sets the Chronodot or RTC clock based on the supplied 20-character date-time string
// input string must be formatted as "yyyy-mm-dd hh:mm:ss"
void setTime(char *dtS) {
  uint8_t yy,mn,dd,hh,mm,ss;
  yy = xconv2d(dtS+2);
  mn = xconv2d(dtS+5);
  dd = xconv2d(dtS+8);
  hh = xconv2d(dtS+11);
  mm = xconv2d(dtS+14);
  ss = xconv2d(dtS+17);
  if (yy*mn*dd==0) { 
    Serial.print(F("[%WP] setTime called with yy*mon*dd = 0 and input string = '"));
    Serial.print(dtS);
    Serial.println("'");
    }
  else
    if ( haveRTC )
      RTC.adjust(DateTime(yy,mn,dd,hh,mm,ss,0.0,0.0));
    else
      Timer.adjust(DateTime(yy,mn,dd,hh,mm,ss,0.0,0.0));
}

// returns the current date & time, as recorded by the Chronodot or Arduino RTC, 
// as a null-terminated, 20-character date-time string in the format "yyyy-mm-dd hh:mm:ss"
void getTime(char dtString[20]) {
  DateTime now;
  now = haveRTC ? RTC.now() : Timer.now();
  sprintf(dtString, "%4d-%02d-%02d %02d:%02d:%02d%c", now.year(), now.month(), now.day(),
      now.hour(),now.minute(), now.second(), '\0' );;
};

void readSensors(struct recordValues *rec) {
  uint8_t data[9];

  // time stamp this record
  getTime(rec->cd.dt);

  // if we have DS18s, start reading temps now, in parallel
  if ( haveDS18 ) ds18.readAllTemps();
  
  if ( haveMPL3115 ) {
    rec->mpl.press = baro.readPressure();   // Get MPL3115A2 data
    rec->mpl.alt   = baro.readAltitude();
    rec->mpl.tempf  = baro.readTempF();
  }
  else {
    rec->mpl.press = 0.0;
    rec->mpl.alt   = 0.0;
    rec->mpl.tempf  = 0.0;
  };
  
  if ( haveDHT22 ) {
    rec->dht.tempf = myDHT22.readTemperature(true); // Get DHT22 data with temp in Fahrenheit
    rec->dht.rh    = myDHT22.readHumidity();
  } else {
    rec->dht.tempf = 0.0;
    rec->dht.rh    = 0.0;
  };

  if ( haveDS18 ) {
    ds18.waitForTemps(convDelay[(int)dsResMode]);
    for (int dev=0; dev<dsCount; dev++) {
      rec->ds18.tempf[dev] = CtoF(ds18.getTemperature(dsList[dev].addr, data, false));
      rec->ds18.label[dev][0] = data[2];
      rec->ds18.label[dev][1] = data[3];
      rec->ds18.label[dev][2] = 0x00;
    };
  } else {
    for (int dev=0; dev<DSMAX; dev++) {
      rec->ds18.tempf[dev] = 0.0;
      rec->ds18.label[dev][0] = rec->ds18.label[dev][1] = '*';
      rec->ds18.label[dev][2] = 0x00;
    };
  };
};                            // end readSensors

void updateTFT(struct recordValues *rec) {
  static char ts[21];                     // temp string for conversions
  // char arrays to print to the TFT and then erase from the TFT
  static char timeDisplay[9], temp1Display[17], temp2Display[17], rhDisplay[8], pDisplay[10];
  static char stc[5], stf[5];

  if ( !haveTFT ) return;

  if (haveDisplayed) {                 // If there's something on the
    TFTscreen.stroke(TFT_BLACK);       // TFT clear it by reprinting 
    TFTscreen.text(timeDisplay, 0, 0); // with same color as background
    TFTscreen.text(temp1Display, 0, 30);
    TFTscreen.text(temp2Display, 0, 60);
    TFTscreen.text(rhDisplay, 0, 90);
    TFTscreen.text(pDisplay, 0, 120);
  };

  // print the new values to the TFT
  // Arduino's sprintf doesn't handle %f formatting; use dtostrf() hack
    for (int i=0; i<8; i++) timeDisplay[i]= rec->cd.dt[i+11]; timeDisplay[8] = (char)0x00;
    dtostrf(rec->mpl.press, 6, 0, ts);
    sprintf(pDisplay, "%s Pa", ts); 

    // Display the labels and temps from just the first two DS18 probes
    dtostrf(rec->ds18.tempf[0], 3, 1, stf);
    sprintf(temp1Display, "%s=%s  F",rec->ds18.label[0],stf);
    dtostrf(rec->ds18.tempf[1], 3, 1, stf);
    sprintf(temp2Display, "%s=%s  F",rec->ds18.label[1],stf);
    temp1Display[7] = temp2Display[7] = 0xF7;  // degree char on TFT

    dtostrf(rec->dht.rh, 2, 0, stc);
    sprintf(rhDisplay, "%s %%RH", stc);
    TFTscreen.stroke(127, 255, 255);      // set the font color
    TFTscreen.text(timeDisplay, 0, 0);
    TFTscreen.text(pDisplay, 0, 120);
    TFTscreen.stroke(255,127,255);        // diff color for temp/humidity
    TFTscreen.text(temp1Display, 0, 30);
    TFTscreen.text(temp2Display, 0, 60);
    TFTscreen.text(rhDisplay, 0, 90);
    haveDisplayed = true;
};					  // end void updateTFT()

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
  delay(50);
  ID = readwrite8(0x04, 24, 1);    // 0x04 ==> RDDID op code to ST7735
                                   // get 3 bytes back; ignore 1 dummy
  if (ID == 0x7C89F0uL) return(tftST7735R);
  if (ID == 0x548066uL) return(tftILI9163C);
  return(tftMISSING);
};				   // end findTFT()


/*  Write 8-bit command to TFT, receive back 0-32 bits as a result word.
 *   
 *  SPI 4-wire TFT connection; uses SPI mode #0; see ST7735 data sheet
 *  Code taken from avrfreaks posting by David Prentice, Dec, 2015.
 */
uint32_t readwrite8(uint8_t tftCmd, uint8_t tftBits, uint8_t tftDummy) {
  uint32_t ret = 0;
  uint8_t val = tftCmd;

  digitalWrite(cs, LOW);
  digitalWrite(dc, LOW);
  pinMode(MOSI, OUTPUT);
  for (int i = 0; i < 8; i++) {   //send command
    digitalWrite(MOSI, (val & 0x80) == 0 ? LOW : HIGH);  // set next bit on SDA
    digitalWrite(SCK, HIGH);   // Tell slave to read that bit
    digitalWrite(SCK, LOW);    //  on high->low transition
    val <<= 1;
  }
    
  if (tftBits == 0) {               // nothing to get back, so return
    digitalWrite(cs, HIGH);
    digitalWrite(dc, HIGH);
    return 0;
  }
    
  pinMode(MOSI, INPUT_PULLUP);    // switch SDA pin mode so slave can send
  digitalWrite(dc, HIGH);        // switch to data mode [hdt: is this needed?]  
  for (int i = 0; i < tftDummy; i++) {  //any dummy clocks ?
    digitalWrite(SCK, HIGH);   // transition to HIGH on SCK but ignore data
    digitalWrite(SCK, LOW);
  }
  for (int i = 0; i < tftBits; i++) {  // read results
    ret <<= 1;
    if (digitalRead(MOSI)==HIGH) ret |= 1;;  // record that bit as 1 if signal HIGH
    digitalWrite(SCK, HIGH);    // SCK transition to HIGH asks for bit on SDA
    digitalWrite(SCK, LOW);
  }
  pinMode(MOSI, OUTPUT);
  digitalWrite(cs, HIGH);
  return ret;
}
