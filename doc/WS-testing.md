#Testing WS

Create the WeatherStation executable (filename 'ws') by connecting to the '.../src' directory and issuing the command `make`.  Then issuing the command `./ws ?` generates the output 

	WeatherStation v5.0: program to collect and record meteorological data
	ws <mode> where <mode> = prt | sql | xml
	

So `./ws prt` collects data from the Arduino weather probe at regular intervals (compile-time parameter set at 60 seconds as distributed) and generates to the controlling terminal a nicely-formatted report of the data.  And `./ws prt > myreport.rpt` generates that report to the file myreport.rpt.

Similarly, `./ws xml` generates an xml-formatted report to the terminal, and `./ws xml > live-data.xml` generates that report to the file live-data.xml.  [This package includes xml tree-parsing routines that will display the xml data elements or convert the data to comma-separated-variable, csv, format.]

The command `./ws sql` differs in that it creates (if necessary) a MySQL table or sqlite3 file and table and appends the periodic data to that file.  By default, ws compiles to use sqlite3, and the database file is stored in /var/databases/weather_data.db.  If you intend to use MySQL to store the data, you must set the database username/password in the WS.h file prior to doing the `make` that creates ws.  If you intend to use sqlite3, you must have write privileges to /var/databases or you must run ws as root, as in `sudo ./ws sql`.

You can use ^C (CNTL-C) to stop execution of ws in test mode.  It halts execution as you would expect, except that in xml mode it issues the <\\samples> text that closes the xml file correctly.  As a result, that xml file can be fed directly into the xml parsing programs to validate or extract data.

