# <center>WeatherStation/WeatherProbe</center>
### <center>A Raspberry Pi Weather Datalogger </br>with Arduino WeatherProbe Data Collector v 5.1</center>
<center>Written by David Todd(hdtodd@gmail.com)</br>
August 2015-January 2018, Bozeman MT/Williston VT</center>
</br></br>
This project implements a weather data logging system comprised of:

1.  a Raspberry Pi program to collect meteorological data from an attached Arduino probe and 
store the data in a MySQL or sqlite3 database, or print out as simple
labeled data fields, or print out or store as XML-formatted data;

2.  an Arduino Uno program to collect the meterological data from connected devices and present the data for viewing or storing.  </br>The following devices are supported but optional.  None are required for operation -- the Uno program samples and displays data from only the devices that are attached and ignores missing devices.
  * MPL3115A altimeter/barometric pressure/temperature sensor
  * DHT22 temperature/relative humidity sensor
  * DS18-class temperature probes (zero or multiple)
  * Chronodot real-time clock
  * A 1.8" TFT color LCD display.

4.  A <tt>Makefile</tt> to create and install the Raspberry Pi code, including a <tt>systemctl WeatherStation.service</tt> file for automatic operation upon reboot;

5.  Web service support with PHP and Python code examples used with an Apache server to create web-page graphical displays using Google Charts

6.  XML DTD file and XML file parser to validate the XML data file and extract data from
a validated, conforming XML file.

The Arduino is connected to the Pi or other Linux system (or Mac OSX) via USB cable.  The supplied Arduino code
is compiled on the Pi or OSX and downloaded over USB cable to the Uno (which also
receives its power from the USB cable).  The WeatherStation program operating on the Pi or OSX issues commands to the Uno
and receives data in return, which is then displayed to the user or stored in a database.

The included PHP and Python files offer prototypes for displaying data collected in the database (in sqlite3 format in this case) and/or presenting via an Apache2 server.

This distribution includes the source code for WeatherStation (Pi) and WeatherProbe (Arduino), Makefiles, installation guide, principles-of-operation manual, and circuit diagrams.

