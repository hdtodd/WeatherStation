/**************************************************
Weather_Station v4.1
Raspberry Pi-based program for weather station management.

This management program:
  o  connects to an Arduino programmed to be a "weather probe"
     to collect meteorological data
  o  periodicly instructs that weather probe to sample its data
     sensors and report back its observations
  o  records the reported data, with date_time stamp, into a
     sqlite3 or MySQL database or into an XML data file, 
     or simply reports the data as a formatted printout.

Written by HDTodd, Bozeman Montana & Williston Vermont, August, 2015.

  V4.1  Revised Feb-Aug 2016 to modularize code 

  v4.0  Clean up code: reduce duplication, use procedures to 
        make code flow more transparent, introduce structured
        approach to collecting data from multiple devices that
        might or might not actually be present, make database
        storage more flexible for future modification.

  v3.0  Revised January, 2016, to use sqlite3 database as alternative to MySQL

  v2.0  Extend to support XML recording format; add support for
        MPL3115A2 altimeter/barometer/thermometer.

  v1.0  Initial feasibility version with Chronodot, DHT22 support
        in a MySQL database (CSV & Printout reporting modes).

USB serial connection modeled after file: demo_rx.c using the 
  rs232.c and .h files that were written by Teunis van Beelen and 
  available at http://www.teuniz.net/RS-232/ and 
  licensed under GPL version 2.

Link/load with the file rs232.c, included with the package and
  automatically linked if the Makefile is used.

**************************************************/
#ifndef USE_SQLITE3
  #ifndef USE_MYSQL
    #define USE_SQLITE3            // Unless otherwise specified, use sqlite3 db
  #endif
#endif

#define Version "4.1"
#define SAMPLE_PERIOD 288             // 5 min between samples with 12 sec for processing

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "rs232.h"
#include "ws.h"
#include "WeatherStation.h"

/* Global variables, used by ancillary procedures */
FILE *xmlout;                         // must be global; used by setStoreMode()
storeModes storeMode=prtMode;         // by default, just print out
boolean keepReading=true;
int cport_nr=24;                      // /dev/ttyS0 (COM1 on windows)
                                      // 24 = /dev/ttyACM0 on RaspPi
			              // try 25 = /dev/ttyACM1 if that doesn't work

/* Intercepts ^C to terminate read loops */
void intHandler(int sigType) {
  keepReading = false;
}

int main(int argc, char *argv[]) 
{
  char *ch;
  unsigned char lbuf[4096];           // buffer for a line of data from the probe
  boolean gotLine;

  storeModes setStoreMode(int argc, char *argv[]);
  void initDBMgr();
  boolean connectToWP();
  boolean getDataLine(unsigned char lbuf[]);

struct WPCmds {
  char *cmdString; 
  int cmdLen; 
  } startWPCmds[] = {        // same order as enum typedef
    {"printout\n",9}, 
    {"csv\n",4},
    {"xmlstart\n",9}
    };

  struct sigaction act;                 // catch CNTL-C to stop program & terminate XML file
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);

  storeMode = setStoreMode(argc, argv);          // set the storage mode for sampled data
  if (storeMode == sqlMode) initDBMgr();    // test database connection if necessary

  if (! connectToWP() ) {               // verify connection to Weather Probe
        fprintf(stderr, "\n?WeatherStation: Cannot open comport\n");
	exit(EXIT_FAILURE);
  };

  /* Finally, get down to work.  Tell the probe how we want to see the data   */
  RS232_SendBuf(cport_nr, startWPCmds[storeMode].cmdString, startWPCmds[storeMode].cmdLen);
  sleep(1);                                    // give the probe time to set up

  /* This loops "forever" -- or until a ^C is typed */
  while (keepReading) {                        // exit if ^C received, or other trigger in future
    RS232_SendBuf(cport_nr, "sample\n", 7);    // tell the probe to take a sample
    sleep(2);                                  // wait a bit for the probe to do its work
                                               // and then start reading lines from the serial port
    while ( getDataLine(lbuf) ) {
      if ( (storeMode==sqlMode && (lbuf[0]!='(' || lbuf[1]!='\''))  // csv lines start ('
            || (storeMode==xmlMode && lbuf[0]!=('<')) )  {          // xml lines start <
	fprintf(stderr, "%WS: Data line formatted incorrectly:\n\t%s", lbuf);
      };
      switch (storeMode) {
        case prtMode:
	  printf("%s", lbuf);                 // for printout mode, just print the line
	  break;
        case xmlMode:                         // if xml mode, just copy the line to the file
	  if (xmlout!=stdout) xmlout = fopen(argv[2], "a");
	  fprintf(xmlout,"%s", lbuf);
	  if (xmlout!=stdout) fclose(xmlout);
	  break;
        case sqlMode:                         // if sql mode, add row to the table
	  appendToDB(lbuf);
	  break;                               // end sql processing
      };                                     // close switch for processing
    };                                       // end getDataLine -- process all lines for this sample
    if (keepReading) sleep(SAMPLE_PERIOD);       // sleep for specified period before sampling again

  };                                         // end while keepReading -- terminate recording
                                             // if there's ever a time when we don't
                                             // keepReading, we'll exit here
  if (storeMode==xmlMode) {                  // if we're doing xml mode, end the document
    if (xmlout!=stdout) xmlout = fopen(argv[2], "a");  
    fprintf(xmlout,"</samples>\n");
    if (xmlout!=stdout) fclose(xmlout);
  };
  exit(EXIT_SUCCESS);
};

/* This procedure processes any command-line arguments to "ws" to determine
   the mode of reporting (rpt, sql, or xml) and if it's in xml mode, initiates
   the xml output data file if one is given
*/
storeModes setStoreMode(int argc, char *argv[]) {
#include "ws.h"
  storeModes mode;
  char *ch;

  mode  = prtMode;                          // unless explicitly stated, assume prt
  if (argc>1) {                             // what mode was requested?  prt, sql, or xml?
    if (strcasecmp(argv[1], "sql")==0) mode = sqlMode;
    if (strcasecmp(argv[1], "xml")==0) {
      mode = xmlMode;
      if (argc==2)                         // if xml, was a filename given?
        xmlout = stdout;                   // if not, output to stdout
      else
        if ( !(xmlout=fopen(argv[2],"a")) ) {
	  fprintf(stderr, "?WS: Cannot open %s for output in append mode\n", argv[2]);
	  exit(2);
	};
      if (xmlout!=stdout) fclose(xmlout);
    };
  if (strcasecmp(argv[1], "help")==0 || strcasecmp(argv[1], "?")==0 ) {
    printf("WeatherStation v%s: program to collect and record meteorological data\n", Version);
    printf("\tws <mode> where <mode> = prt || sql || xml\n");
    exit(EXIT_SUCCESS);
  };

  };
  return(mode);
};                                         // end setStoreMode

/* This procedure returns a single line of sample data from the probe each time it is called.
   It is called after a "sample" command has been sent to the probe by the invoking procedure.
   The "sample" command causes the probe to return data from its sensors with
   each line terminated by a '\n' character (not a NULL).
   A "sampling" may return 1 line (e.g., sql mode) or many lines (e.g., xml mode)
   This routine is called repeatedly and returns a value of "true" along with
   a line of data in the line buffer, "lbuf", until the receive buffer has been emptied
   and there is no more data in the serial-port pipeline from the probe.  When that
   happens, the procedure returns a value of "false" the next time it is called, and the
   invoking procedure knows that all the data from its last "sample" command has been processed.
*/
boolean getDataLine(unsigned char lbuf[]) {
#define  maxWait  10                           // if no response from Probe in 10 sec, quit
  static unsigned char rbuf[4096];             // receive buffer
  static int rbufSize=0, rbufPtr=0;            // chars left in rbuf
  int i,  lbufPtr=0;
  boolean gotLine = false;                   // start loop with no line from probe
  int timerCount = 0;

  lbuf[0] = 0;
  if ( rbufSize == 0 )                       // no data in buffer so go get some
    while ( ( (rbufSize = RS232_PollComport(cport_nr, rbuf, 4095) )<=0 ) 
            && (timerCount < maxWait) 
	    && keepReading )  {
      sleep(1);
      timerCount++;
    };
  if(rbufSize > 0) {
    rbuf[rbufSize] = 0;                        // put a "null" at the end
    //    printf("rbuf = %s\n", &rbuf[rbufPtr]);
    lbufPtr=0; 
    do {
      lbuf[lbufPtr++] = rbuf[rbufPtr++];
    } while ( (rbufPtr<rbufSize) && (lbuf[lbufPtr-1] != '\n') );
    lbuf[lbufPtr] = 0;                        // null-terminate the line
    if ( rbufPtr == rbufSize ) rbufPtr = rbufSize = 0;    // if we're at end of buffer, say it's empty
    gotLine = true;
  };                                          // end of loop to copy receive-->line
  return(gotLine);
};                                          // end getDataLine()
