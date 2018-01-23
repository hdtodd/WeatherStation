#<center>WeatherProbe/WeatherStation</br>Installation Guide
###<center>[H. David Todd](hdtodd@gmail.com), January, 2018</center></br>

*WeatherProbe* (WP) is a program that runs on the Arduino single-board microcontroller to read data from various meterological sensors and display current conditions on a connected TFT LCD display, if there is one connected.  *WeatherStation* (WS) is a program that runs on the Raspberry Pi or other Linux/Macintosh system to collect that meterological data from WP and to store and report or present it via web page. They are the Arduino and Raspberry Pi (or "Pi" for short) software components, respectively, of a meterological station *system*. They work together to collect+display (Arudino) and record+present (Pi) meterological data from a set of sensors connected to the Arduino. This document describes the installation of the programs and some of the compilation parameters that can be used to tailor their operation.

#Quickstart

After all of the software components have been installed and the Arduino has been connected to the Pi, a simple `make; sudo make install` will cause the system to begin collecting meteorlogical data into a sqlite3 table `ProbeData`, in the file `/var/databases/WeatherData.db`.

The system will collect data even if you have no sensors, no TFT display, and no real-time-clock connected to the Arduino!  You'll get records with a date-time stamp based on the date-time of your controllin Pi and with zeros for the rest of the data.  That's an easy way to test your software setup before wiring in sensors.  Then power-off and add the sensors, display, or RTC that you do have available and restart: the system will append real data to your database table.

You can check that the data are being collected with the commands:

	$sqlite3 /var/databases/WeatherData.db
	sqlite> select * from Probedata where date_time > datetime('now', '-8 hours');
	...
	2017-09-23 13:40:45|84724|67.9|67.8|34|IN|69.8|OU|48.7|**|0.0|**|0.0
	2017-09-23 13:45:55|84728|67.8|67.5|34|IN|69.8|OU|47.3|**|0.0|**|0.0
	2017-09-23 13:51:05|84722|67.9|68.0|33|IN|69.8|OU|47.7|**|0.0|**|0.0
	sqlite> .quit
	

If you installed the file `/var/www/index.php` **being careful not to overwrite any current index.php file there!** and if Apache2 is running, you can point your web browser to your host computer to see current conditions and (after a period of time to collect data) a graphical representation of the last week of temperature and barometric pressure.

Even if you don't have or don't intend to install the various devices WP is prepared to support, you'll need to download the libraries in order to compile WP.  Unavailable devices are ignored in operation.  

Here are the steps:

1. Install the Arduino IDE and `make` tools if you don't have them: `sudo apt-get update` then `sudo apt-get install arduino arduino-core arduino-mk`.
1. Connect your Arduino to your Raspberry Pi with a USB cable.  Start the Arduino IDE by double-clicking the Arduino icon.  Load one of the example programs, e.g., 'blink', and test it to make sure you have a working Arduino IDE environment that can compile and link code and upload it to the Arduino.
1. Set the baud rate for your USB serial interface to 9600 (else you'll need to change the speed in the WP code); 9600 seems fast enough to work with but slow enough for the serial interface to be reliable.
1. You may find it helpful to use a terminal emulation program to connect your host system to the Arduino via the USB serial line for testing purposes.  `screen` should work just fine if you have experience with it, and `make monitor` from the Weather_Probe directory invokes `screen` as a terminal connection to the Arduino.  However, I had trouble getting `screen` to communicate well, and I found that `minicom` also works well for this purpose.  It's easy to install: `sudo apt-get update` then `sudo apt-get install minicom`.  But see the Appendix for configuration information, especially changing the line-termination character (which may have been my unresolved problem with `screen`).
1. If you don't already have an Arduino directory under your account, create one and connect to it: `mkdir ~/Arduino; cd ~/Arduino`.  Then `mkdir libraries`.
1. Download and unpack into `~/Arduino` the WeatherStation package from Github. Go to [https://github.com/hdtodd/WeatherStation]() to do a `git clone` or with your browser to obtain the zip file, `WeatherStation.zip`, and unzip it into your `~/Arduino` directory.
1. If not already installed, install the Arduino TFT library (see [https://www.arduino.cc/en/Guide/TFT]() for details).  This installs the  Adafruit GFX and Adafruit ST7735 libraries extended for the Arduino TFT display.
1.  If not already installed, install the Arduino SPI library (see [https://www.arduino.cc/en/Reference/SPI]() for details).
1. Download and install via IDE or unpack into `/usr/share/arduino/libraries` the OneWire package from [https://github.com/PaulStoffregen/OneWire]().
1. Download and install via IDE or unpack into `/usr/share/arduino/libraries` the I2C package from [https://github.com/rambo/I2C]().
1. Download and install via IDE or unpack into `/usr/share/arduino/libraries` the DHT package from [https://github.com/adafruit/DHT-sensor-library]().
1. Download and install via IDE or unpack into `~/Arduino/libraries` the ChronodotI2C package from [https://github.com/hdtodd/ChronodotI2C]().
1. Download and install via IDE or unpack into `~/Arduino/libraries` the DS18 package from [https://github.com/hdtodd/DS18]().
1. Download and install via IDE or unpack into `~/Arduino/libraries` the MPL3115A2 package from [https://github.com/hdtodd/MPL3115A2]().
1. If your Arduino is something other than a Uno, go to `~/Arduino/WeatherStation/src/Weather_Probe` and edit the `Makefile` entry for `BOARD_TAG` to be your model of Arduino.  See Appendix for my functioning `Makefile`.
1. `cd ~/Arduino/WeatherStation/src` and `make`.  The WP code will be compiled, linked, and uploaded to the Arduino.  If a TFT LCD display is connected to the Arduino, it will begin displaying current time and conditions.  The WS code will be compiled and linked, resulting in an executable program, `ws` in the `src` directory.


#Overview

WeatherProbe is an Arduino .ino program -- essentially C++ with some constraints and with support for the Arduino devices.  WeatherStation is a C program.  Both programs are compiled and linked on the host system.  

The "host" is referenced here as a Raspberry Pi, but it could be any other Linux or Mac OSX system that offers the Arduino IDE environment.  The installation procedure in `Makefile` assumes that the host is a Pi running systemd.  That procedure may succeed on other Linux systems, but you'll need to perform equivalent steps manually if you're running on OSX.


The Arduino program is compiled and linked on the host to create a binary file that is executable by the Arduino.  The Arduino binary code is uploaded to the Arduino over the USB serial port *and stays resident and active until replaced, even through power cycling*.


#Database Options

WS can record the date-time stamped sample data it receives from WP in either a sqlite3 (default) or MySQL database.  

Use of sqlite3 has the advantage that WS creates the database file and table automatically (if permissions allow) and can be set up in the user's directory with no special privileges.  Use of MySQL requires that a server be installed and in operation, with appropriate permissions for the user to create a database and table, and the WS program must be compiled with MySQL hostname, username, and user password edited in the WS.h file.

The PHP code that can be used as a model for a web reporting page assumes the use of a sqlite3 database file in `/var/databases`: you'll need to modify that code if you choose to use MySQL.

</br>**sqlite3 Database Setup**</br>

The sqlite3 database file name, by default in the code, is `~/WeatherData.db` so that it can be created and written to by the user during initial testing.   But in Makefile, that definition is overridden and the database filename is set to be `/var/databases/WeatherData.db` so that the system is set up for production operation.  **Note that that file will normally be protected, so either WS must run as root (or as a systemd service) or the file must be created and protections set to enable writing by the user.**

On startup, if the sqlite3 database *file* `/var/databases/WeatherData.db` doesn't exist, WS creates it (if permissions allow; if this fails, check permissions and/or run as `sudo`).  The sqlite3 code uses a table named `Probedata` in that database file.  Again, if it doesn't exist in the file, WS creates it with the command:

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

</br>**MySQL Database Setup**</br>


Operation of WS using a MySQL is very similar to its operation using sqlite3.  However, the setup is a bit more complicated:

* mysql server must be running (generally done as a startup service, likely with systemctl)
* username and password for a MySQL account that can append data to the database table must have been provided (edit WS.h, `make clean` and recompile with `USE_MYSQL=1 make`)
* A `weather` database and `Probedata` table in that database must have been created under that user account with the correct field names and data types, per installation instructions.

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

At this point, you'll have an empty table, `ProbeData`, in a database `weather` that has only that one table in it.

But if you've compiled WS with the MySQL username/password set and using the `make` commands `make clean; USE_MYSQL=1 make`, you're ready for operation.  

</br>**Either SQL Option**</br>

Whether you've chosen to run with sqlite3 or with MySQL, if you start WS with `sudo ./ws sql`, it will now append the meterological data it receives from the Arduino WP program to the sqlite3 or MySQL table using an `insert` command string in the sqlite3 example above.  And, again, you can examine that data using sqlite3 or MySQL commands similar to the sqlite3 command examples given above.

If you move your newly-compiled version of `ws` to `/usr/local/bin` with `make install`, and if you are running systemd to manage services, then upon reboot or by issuing a `sudo systemctl start WeatherStation.service` command, your sqlite3 or MySQL version of `ws` will begin autonomous operation to collect and archive meterological data to its SQL table.



#Appendix -- minicom setup

If you use minicom to test `wp` on the Arduino from a host program, you'll need to set up the minicom configuration.  You can type `minicom -s` and set parameters as below, or you can create/edit the file `/etc/minicom/minicom.dfl`,

	/etc/minicom/minirc.dfl 
	# Machine-generated file - use "minicom -s" to change parameters.
	pu port             /dev/ttyACM0
	pu baudrate         9600
	pu rtscts           No
	pu convf            /etc/minicom/miniconv.tbl
	pu localecho        Yes
	pu addcarreturn     Yes
	
Note the use of the character translation table, miniconv.tbl.  Minicom and wp running on the Arduino have differing ideas of what terminates a line.  Rather than encounter problems with the IDE when I do need to use it, I taught minicom to change its CR line termination character to a LF character.  Since the change is in the CNTL character section of the ASCII codes, just putting that table here wouldn't be helpful.  You'll need to make that one change yourself.  Here's how:

1. On your host, `sudo minicom -s` (must be privileged to save to /etc/minicom)
1. Downarrow to the `screen and keyboard` menu entry and press enter
1. Type `O` to get to character conversion table
1. Type `E` to get to the screen with characters that start with a 0 (zero) -- these are the CNTL characters and digits
1. Type `C` to edit a character
1. Type `13` to say you want to edit character 13 (the CR character); leave input to be `13` by pressing return; change output to be `10` (the LF character) and press return
1. Type `B` to save the table and save it as `/etc/minicom/miniconv.tbl`.
1. ESC and exit from `minicom`.  You're done.


#Appendix -- arduino.mk Makefile

	$cat Makefile 
	ARDMK_DIR = /usr/share/arduino
	BOARD_TAG = uno
	include $(ARDMK_DIR)/Arduino.mk
	
	
