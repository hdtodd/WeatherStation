# WeatherProbe/WeatherStation</br>Principles of Operation
### H. David Todd, hdtodd@gmail.com, January, 2018

**WeatherProbe** (WP) is a program that runs on the Arduino single-board microcontroller to manage and read data from various meterological sensors.

**WeatherStation** (WS) is a program that runs on the Raspberry Pi or other Linux/Macintosh system to store that meterological data and report or present it.

They are the Arduino and Raspberry Pi software components, respectively, of a meterological station **system**. They work together to collect+display (Arudino) and record+present (Pi) meterological data from a set of sensors connected to the Arduino. This document describes the operation of that system -- how the individual components function and how they function as a system.  This PO manual may help others add components to the Arduino or functionality to the system.

## Basic Operation

### System Data Flow
The WS program on the Raspberry Pi/Linux controller manages the WP probe program on the Arduino over the USB serial port that connects the two (and powers the Arduino).  In response to commands coming over the USB serial port from WS on the Pi, the WP program performs actions, including sampling of data from its sensors, and reports the results via the USB serial connection back to the WS program if it has been commanded to do so.  

The WS program then performs the reporting action *it* was commanded to do at startup.  WS reporting options include:

1. Reporting results on the terminal controlling the WS program, in a report format with well-formatted lines and with labels for data;
1. Appending the results to a sqlite3 (default) or MySQL database;
1. Appending the results to an XML file, with appropriate XML tags in conformance with a standard DTD template for the reported data.

Operating independently, an Apache web page with PHP code can be used to access the sqlite3 or MySQL database to show current meteorologial conditions and display graphically a history (default 1 week) of temperature and pressure.  

The WP program primarily performs actions only in response to WS commands over the USB serial port.  The exception is that it periodically (default 1 minute) samples the sensors and updates the TFT display, if there is one, with current conditions.

## WeatherProbe (WP -- Arudino Code)

Arduino code has two major procedures as the main program: setup() and loop().  One-time initialization tasks are performed in setup(), and the program then enters loop(), which executes repeatedly until powerdown or restart.

### Startup
Upon upload, powerup, or restart, the WP code in the setup() procedure in the Aruindo:

1. Initializes the USB Serial port (default 9600 baud)
1. Attempts to verify active electrical connections to the TFT LCD display, the real-time-clock, and then to each of the sensors for which it has support code.  It marks as absent those devices that don't respond or don't respond correctly during this setup phase, and no further attempt is made to communicate with them.  
1. Announces its presence over the USB Serial port.
1. Enters a loop in which it continuously performs the following steps.
	1. Periodically (default 1 minute) updates weather information on the TFT display attached to the Arduino, if there is one.
	1. Looks for a command string coming over the USB serial port from the controlling device or person.
	1. In response to commands, performs the appropriate action, perhaps with text response back to the controlling device/person over the USB serial port.  The most common command operation would be `sample`, which causes WP to sample the data from each of the sensors; update the display (if there is one); and, if it has been requested to do so, report the sampled data with date-time stamp in the appropriate format (report, CSV, or XML).
	1. Repeat the loop to await further commands, while continuing to update the display periodically, if there is one. 

There is no "exit" from this code.  Once it has initialized in setup(), it enters the loop() procedure and loops until it receives a command to `restart`, at which point it simply restarts the process from step 1 above, or until the Arduino is reloaded or powered down.

### Device Support on the Arduino
WP has support for the following devices connected to the Arduino:

1. Chronodot real-time-clock, connected via I2C 
1. SainSmart 1.8" TFT LCD Display connected via SPI
1. MPL3115A2 altitude/barometer/thermometer sensor connected via Arduino pins
1. DHT22 temperature/humidity sensor connected via Arduino pins
1. DS18-class thermal sensors (up to 4, as a compile-time parameter) connected via OneWire interface to Arduino pins

If a Chronodot is not present, WP uses the internal clock of the Arduino to timestamp events.  That clock can be set by command from the controlling host over the USB serial line, which WeatherStation does when it starts up.  You'll need to do that manually if you're running WP in terminal monitor mode.

If a TFT display is present, it is used to show the date-time stamp of the latest sample, the temperatures reported on the first two DS18 temperature probes in that sampling, the relative humidity, and the **unadjusted** (see MPL below) barometric pressure in Pascals (divide by 100 to get millibars). **Barometric pressure is adjusted by the MPL3115A2 code for the altitude of the device, which is a compile-time parameter set at 40m.  It is otherwise reported from the raw data read and not adjusted for temperature or humidity.** 

If any of the other devices is absent or not sensed correctly, its absence is reported at startup over the USB serial port and it is marked absent internally.  No further attempt is made to gather data from that device, and any reported values are reported as zeros.

### WP Commands
Command processing within WP is very restricted.  The command processor does not handle line editing such as backspace character deletion or line deletion.  Input that is not recognized as a correctly-formed command is discarded: the complete line is ignored, no action is taken, and a prompt is sent over the USB serial line to indicate the commands WP is prepared to process (equivalent to having sent a "?" or "help" command).

WP commands include the following set:

*  **help** or **?** or simply **\<CR\>** (blank line)</br>returns a string over the USB serial connection to the controlling program/terminal with this list of its commands

*  **version** or **whoru**</br>returns a string identifying the version of WP and of the database for which WP is providing data (number of sensor readings and sequence of returned values must correlate with what the controlling program is expecting to store in its database)

*  **report**</br> sets WP to be in *report* mode so that subsequent `sample` commands return the timestamped, collected sensor-data string as a formatted line of labeled data

*  **csv**</br>sets WP to be in *csv* mode so that subsequent sample> commands return the timestamped, collected sensor-data string in comma-separated-variable format.  As compiled, the string has the format "(val,val,val, ...)" [see Reports, below].

*  **xmlstart**</br>causes WP to immediately return the XML header string over the USB serial port and then sets WP to be in *xml* reporting mode so that subsequent sample commands return the timestamped, collected sensor-data as XML-tagged data samples.

*  **xmlstop**</br>causes WP to immediately return the XML termination string over the USB serial port and disable xml reporting mode (reporting is disabled until another command is issued that again enables a reporting mode)

*  **sample**</br>causes WP to sample each of the known sensors and update the TFT display, if there is one.  If a reporting mode is enabled, WP also returns, over the USB serial connection to the controlling program/terminal, a string containing the date-time stamp and sampled data in the format required by the active reporting mode.</br></br>With a full set of devices, the time required for one sampling is about 1400 milliseconds (1.4 sec).

*  **settime yyyy-mn-dd hh:mm:ss** (all digits, 24-hour clock, must be formatted exactly in this way) causes WP to set the Chrondot real-time clock (if there is one) or the date-time offset for the internal Arduino interval timer, so that subsequent date-time stamps are synchronized with the host computer system.

*  **restart**</br>causes the Arduino to reboot and restart WP.

### WP Device Details

Most of the devices supported by WP are straightforward: one device, one connection, one set of data per sampling.  The exception is the DS18-class thermal sensors connected via OneWire: there may be many DS18 devices (limit of 4 as compiled).

#### **Chronodot RTC**
The Chronodot is a battery-maintained real-time-clock (RTC) that provides the date-time string used as timestamp for data records sent to the controlling program/terminal and used to display on the TFT display (if there is one).  Other RTC devices could be substituted for the Chronodot with minor changes to the code in WP.  WP assumes that the Chronodot date+time have been set before WP starts up, but the Chronodot's time can be changed with the `settime` command.  In the absence of a Chronodot, WP uses the the elapsed time since WP started as the current date+time for its date-time stamp, but, again, its base time can be set with the `settime` command.
#### **TFT Display**
The SainSmart 1.8" TFT LCD display is a color 128x160-pixel display connected via SPI interface.  The SainSmart version differs from the Adafruit version in its pinouts.  The implementation here (ST7335) uses the SainSmart pin layout for "high display speed" [[https://www.tweaking4all.com/hardware/arduino/sainsmart-arduino-color-display/]()].  WP supports the ST7735R and  ILI9163C models. The WP code is compiled to display in portrait mode but could be easily modified for landscape. The code is highly modular, with display initialization in separate section within setup() and with the display code itself in a separate procedure called from within loop().  
#### **MPL3115A2**
The MPL3115A2 is a sophisticated altitude/barometer/temperature sensor that provides the ability to read each type of sensor data independently.  Internally, the MPL reads pressure relative to an internal vacuum chamber.  It provides the ability to add a "trim" to pressure and altitude readings to compensate for any internal inaccuracies.  In setup(), WP sets that "trim" value for altitude to the difference between the MPL-measured value and a reading you might make from a GPS system, and the MPL applies that trim to any altitude reading it makes.  (The MPL code does not provide for applying pressure "trim").  Set `MY_ALTITUDE` in `WP.h` to your GPS-measured altitude, in meters, if the altitude reading is important to you and you intend to move the location of the Arduino probe with its MPL.  During sampling, WP collects altitude, pressure, and temperature from the sensor.  Using a terminal connection, put WP in `report` mode and command a `sample` to obtain the MPL readings for all three sensor values.</br>
#### **DHT22**
The DHT22 device senses temperature and relative humidity.  No settings or adjustments are possible.  During sampling, WP simply requests current values for temperature and relative humidity.  A DHT11 or DHT21 device may also be used (compile-time parameter).
#### **DS18B20**
The DS18B20 is one specific example of the OneWire DS18-class thermal sensors, and it is the device with which WP was developed.  WP *should* support the other DS18 devices (DS18S20, DS1822, DS18B20) with no code changes or recompilation required, but it should be validated with other devices before putting into production.

WP samples the two-character DS18 device label along with the temperature and reports those pairs for all connected devices to the controlling program or terminal.  WP assumes that the 2-byte Tl/Th (low/high temperature trigger settings) that are stored in DS18 EEPROM are two-character labels. (The DS18 github distribution includes a DS18 labeling program, but two bytes of the OneWire address might be used as an alternative, with minor coding changes, with some chance that labels wouldn't be unique).  Absent devices are reported as a (label,temp) pair value of ('\*\*',0.0)</br></br>By default, temperatures are measured with 10-bit precision to 0.25C (compilation parameter).</br></br>WP uses the  concurrent-sampling capability of the DS18 device: sampling is initiated concurrently across *all* DS18's, data is collected from other sensors, and then data is collected from the DS18's.  This sampling parallelism speeds up the sampling loop considerably if multiple DS18s are attached.  **As a result, the DS18's must be connected to VCC for power and cannot operate in parasitic mode.**

### WP Report Strings
WP reports sample results, if commanded to do so, as either line-printer style reports (labeled data values), csv-formatted lines (no labels), or as XML-formatted data.
#### **report**
Samples are reported as one line returned with labeled data in the format:
```
2017-09-18 16:46:59  MPL: Altitude=1650.9m Pressure=83661Pa Temp=69.8F  DHT22: Temp=70.5F @ 35% RH  DS18: IN=70.7F OU=58.1F **=0.0°F **=0.0°F 
```
DS18 labels for missing devices (in the list of 4 maximum) are given as `**` and the corresponding temperatures are `00.0`.

#### **csv**
Samples are reported as one line returned in the format "(val,val,val, ...)", with the following values in this sequence and format:

1. 'date-time' (in quotes),  in the format `'2017-09-18 16:22:05'`, suitable for reading as a string and passing to sqlite3 or MySQL
2. MPL3115A2 pressure, integer in Pascals
3. MPL3115A2 temperature reading, float in Fahrenheit
4. DHT22 temperature reading, float in Fahrenheit
5. DHT22 relative humidity reading, integer in %
6. #1 DS18 label, in the format 'xx' (in quotes)
7. #1 DS18 temperature reading, float in Fahrenheit
8. #2 DS18 label, in the format 'xx' (in quotes)
9. #2 DS18 temperature reading, float in Fahrenheit
10. #3 DS18 label, in the format 'xx' (in quotes)
11. #3 DS18 temperature reading, float in Fahrenheit
12. #4 DS18 label, in the format 'xx' (in quotes)
13. #4 DS18 temperature reading, float in Fahrenheit

#### **xml**
Sample data, in the order listed for `csv`, are reported in conformance with the XML DTD template provided with the source code (weather_data.dtd: see [Appendix](appendix-0) ).  Each sampling is marked by a \<sample\>\</sample\> begin-end pair and includes the date-time stamp and all available data in the sample, labeled and with units specified.</br>

### WP Error Processing
The WP code compiles to nearly 32K, and there is little room for additional functionality or error detection.  Attempts are made to report most errors that can WP detect, but the code is not "bullet proof".  Error and warning messages are sent over the USB serial line:

*  Informational and warning messages are preceeded by the text string "[%WP]""
*  Error messages indicating failures are preceeded by the text string "[?WP]""

A program controlling the Arduino running WP via the USB serial port can use these leading text string to identify messages that should be sent to a controlling terminal screen and/or logged to an error file.

## WeatherStation (WS -- Raspberry Pi/Linux Code)

The WS program executes on a Raspberry Pi or other Linux/Mac OSX system to collect and record data from the Arduino weather probe program, WP, over the USB serial connection between the Pi and and Arduino.

### Startup

WS starts by: 

*  processing command-line arguments (see below); 
*  verifying that it can access database files, if needed;
*  verifying that there is an Arduino connected to the Raspberry Pi with which it can communicate;
*  issuing the appropriate command to the Arduino needed to obtain data in the format that has been requested.

WS then enters a loop in which it commands the Arudino to sample data from the Arduino's sensors, receives the sample data in reply, records or prints that data as requested, then delays (default 5 minutes) before repeating.  This continues until the program is terminated.

### WS Device Support

WS was designed to connect to a single Arduino Uno via USB serial port, and it was tested only in that mode.  However, other modes of operation should be possible with relatively minor modifications or additions to the WS code:

*  Serial connection might be made by other means, such as Bluetooth Serial or WiFi or Pi/Arduino serial ports.  A wireless connection to a remote Arduino would be one of the more interesting and useful approaches and should be relatively easy to implement.
*  Multiple Arduinos could be supported from one instance of WS, or by running multiple instances, each connecting through different serial connections to different Arduinos.

### WS Commands

WS is not an interactive program.  It accepts one of three commands on the command line at startup and continues operation until terminated: 

*  `ws prt`, to indicate that WS should generate report-style printouts to the controlling terminal;
*  `ws sql`, to indicate that WS should append sample data to the database file (either sqlite3 or MySQL, depending upon compilation parameters); or 
*  `ws xml` or `ws xml filename` to indicate that WS should write data in XML format to either the controlling terminal or to the file specified as `filename`.

Any other argument on the command line, or no argument on the command line, results in a "help" response that shows what `ws` does and what it is expecting on the command line.  Any additional arguments on the command line are ignored (though redirects for `stdout` and `stderr` work as expected).

WS can be terminated with a CNTL-C (^C) from the controlling terminal or stopped with the command</br> 
`sudo systemctl stop WeatherStation.service` 
</br>if WS is operating as a `systemd` service.  As a result, the XML file is properly closed with `</sample>` under normal circumstances (but check and append that closing string to the .xml file if the system has crashed in a power failure).


### WS Databases

WS can record the date-time stamped sample data it receives from WP in either a sqlite3 (default) or MySQL database.

#### **sqlite3 Database Operations**

The sqlite3 database file name, by default in the code, is `~/WeatherData.db` so that it can be created and written to by the user during initial testing.   But in Makefile, that definition is overridden and the database filename is set to be `/var/databases/WeatherData.db` so that the system is set up for production operation.  **Note that that file will normally be protected, so either WS must run as root (or as a systemd service) or the file must be created and protections set to enable writing by the user.**

On startup, if the sqlite3 database *file* `/var/databases/WeatherData.db` doesn't exist, WS creates it.  The sqlite3 code uses a table named `Probedata` in that database file.  If it doesn't exist in the file, WS creates it with the command:

	CREATE TABLE if not exists ProbeData (date_time TEXT PRIMARY KEY,
	mpl_press INT, mpl_temp REAL, dht22_temp REAL, dht22_rh INT,
	ds18_1_lbl TEXT, ds18_1_temp REAL, ds18_2_lbl TEXT, 
	ds18_2_temp, REAL,ds18_3_lbl TEXT, ds18_3_temp REAL,
	ds18_4_lbl TEXT, ds18_4_temp REAL)

During operation, WS receives sample data from WP over the USB serial port in CSV format, with data in the order and of the types indicated in the `CREATE TABLE` command above.  It appends the received data to the sqlite3 database file with the command:

	INSERT INTO ProbeData (date_time, mpl_press, mpl_temp, dht22_temp, dht22_rh,
	ds18_1_lbl, ds18_1_temp,ds18_2_lbl, ds18_2_temp,ds18_3_lbl, ds18_3_temp,
	ds18_4_lbl, ds18_4_temp) VALUES (val, val, val, ...)
	
where the `VALUES` list is a direct copy of the string sent by WP in response to a `sample` command while in CSV mode.

The sqlite3 database file is opened and then immediately closed when recording each individual sampling, so that the file is minimally vulnerable to corruption in case of system crash.

The sqlite3 database can be examined as a normal sqlite3 database table, for example, with the command:

	$sqlite3 /var/databases/WeatherData.db
	sqlite> select * from Probedata where date_time > datetime('now', '-4 hours');
	...
	2017-09-19 08:06:57|83808|65.9|66.7|40|IN|70.3|OU|36.5|**|0.0|**|0.0
	2017-09-19 08:12:07|83813|65.6|66.4|37|IN|69.8|OU|36.0|**|0.0|**|0.0
	sqlite> .quit

WS can use a MySQL database to store its sampled data, too: see the WS-WP-install.md file for compilation instructions.

Operation of WS using a MySQL is very similar to its operation using sqlite3.  However:

*  mysql server must be running (generally done as a startup service, likely with systemctl)
*  username and password for a MySQL account that can append to the database must have been provided (edit WS.h, `make clean` and recompile with `USE_MYSQL=1 make`)
*  A `weather` database and `Probedata` table in that database must have been created under that user account with the correct field names and data types, per installation instructions.

Create the database and table with the command:
	
	$mysql -u <username here> -p
	<provide password here>
	create database weather;
	use weather;
	CREATE TABLE `ProbeData` (
  		`date_time` datetime NOT NULL,
		`mpl_press` int(6) unsigned NOT NULL,
  		`mpl_temp` float NOT NULL,
  		`dht22_temp` float NOT NULL,
  		`dht22_rh` smallint(5) unsigned NOT NULL,
  		`ds18_1_lbl` char(2) COLLATE ascii_bin DEFAULT NULL,
  		`ds18_1_temp` float DEFAULT NULL,
  		`ds18_2_lbl` char(2) COLLATE ascii_bin DEFAULT NULL,
  		`ds18_2_temp` float DEFAULT NULL,
  		`ds18_3_lbl` char(2) COLLATE ascii_bin DEFAULT NULL,
  		`ds18_3_temp` float DEFAULT NULL,
  		`ds18_4_lbl` char(2) COLLATE ascii_bin DEFAULT NULL,
  		`ds18_4_temp` float DEFAULT NULL,
  		UNIQUE KEY `date_time` (`date_time`)
		);
	show columns from ProbeData;
	quit;

In operation, WS again opens and closes database access just to record data: the connection to the database is not kept open during operation.

### WS Error Processing

There is limited error handling in WP, but WS reports the errors that occur in its side of the processing and forwards the error messages it receives from WP.  Nevertheless, the code is not "bullet proof", particularly with regard to errors that might occur in the communication between the Raspberry Pi and the Arduino.  However, the USB serial connection appears to be very stable and reliable, running for months at a time without disruption.

Similar to error messaging in WP, WS error messages have the following formats:

*  Informational and warning messages are preceeded by the text string `"[%WS]"`
*  Error messages indicating failures are preceeded by the text string `"[?WS]"`

Informational messages from WS are printed to `stdout` (the connected terminal).

Informational messages from WP and error messages from both WS and WP are printed to `stderr` by WS.  These will normally be printed to the connected terminal but can be redirected to error logging files, for example with the command:

	$./ws sql 2>wserrors.log



## Appendix:  weather_data.dtd
The listing below is the DTD template to which the WP xml output conforms.  XML output files can be validated or converted to CSV format with tools supplied with the WS/WP source code.

	<!ELEMENT samples (sample*)>
	<!ELEMENT sample (source_loc?, date_time, MPL3115A2?, DHT22?, DS18*) >
	<!ELEMENT source_loc (#PCDATA) >
	<!ELEMENT date_time (#PCDATA) >
	<!ELEMENT MPL3115A2 (mpl_alt?, mpl_press, mpl_temp) >
  		<!ELEMENT mpl_alt (#PCDATA)>
     		<!ATTLIST mpl_alt
        		alt_unit (ft|m) "m">
  		<!ELEMENT mpl_press (#PCDATA) >
     		<!ATTLIST mpl_press
				p_unit (Pa|mb|inHg) "Pa" >
  		<!ELEMENT mpl_temp (#PCDATA) >
     		<!ATTLIST mpl_temp
				t_scale (C|F|K) "F">
	<!ELEMENT DHT22 (dht_temp, dht_rh) >
   		<!ELEMENT dht_temp (#PCDATA) >
     		<!ATTLIST dht_temp
				t_scale (C|F|K) "F">
   		<!ELEMENT dht_rh (#PCDATA) >
     		<!ATTLIST dht_rh
				unit CDATA #IMPLIED>
	<!ELEMENT DS18 (ds18_lbl, ds18_temp) >
   		<!ELEMENT ds18_lbl (#PCDATA) >
   		<!ELEMENT ds18_temp (#PCDATA) >
     		<!ATTLIST ds18_temp
        		t_scale (C|F|K) "F">
