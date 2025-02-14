/*********************************************************************
Weather_Station v5.0
Raspberry Pi-based program for weather station management.

This management program:
  o  connects to an Arduino programmed to be a "weather probe"
     to collect atmospheric data
  o  periodicly instructs that weather probe to sample its data
     sensors and report back its observations
  o  records the reported data, with date_time stamp, into either
     a sqlite3 or MySQL database or into an XML data file, or simply 
     report the data as a formatted printout.

Written by HDTodd, Bozeman Montana & Williston Vermont, August, 2015.
Revised January, 2016, to use sqlite3 database as alternative to MySQL

  v5.0  Incorporate DS18 support

  v4.2  Finish modularizing code, prepare for DS18 incorporation

  v4.1  Revised Feb-Aug 2016 to modularize code
 
  v4.0  Clean up code: reduce duplication, use procedures to 
        make code flow more transparent, introduce structured
        approach to collecting data from multiple devices that
        might or might not actually be present, make database
        storage more flexible for future modification.

  v3.0  Add sqlite3 database storage as alternative to MySQL.

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

*********************************************************************/
#define Version "5.0"
#ifndef USE_SQLITE3
  #ifndef USE_MYSQL
    #define USE_SQLITE3            // Unless otherwise specified, use sqlite3 db
  #endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "rs232.h"
#include "WS.h"

/* Global variables, used by ancillary procedures */
FILE *xmlout;                      // used by setStoreMode()
storeModes storeMode;
boolean keepReading=true;          // set "false" in intHandler by ^c
boolean xmlToFile;
struct fieldDesc {
    char *fieldName; 
    char *fieldAttributes; }; 
struct fieldDesc fieldList[] = {
    {"date_time", "TEXT PRIMARY KEY"},
    {"mpl_press", "INT"},
    {"mpl_temp",  "REAL"},
    {"dht22_temp","REAL"},
    {"dht22_rh",  "INT"},
    {"ds18_1_lbl", "TEXT"},
    {"ds18_1_temp", "REAL"},
    {"ds18_2_lbl", "TEXT"},
    {"ds18_2_temp", "REAL"},
    {"ds18_3_lbl", "TEXT"},
    {"ds18_3_temp", "REAL"},
    {"ds18_4_lbl", "TEXT"},
    {"ds18_4_temp", "REAL"},
    NULL, NULL};

int main(int argc, char *argv[]) 
{
  char *ch;
  int i, n;
  unsigned char rBuf[rBufSize];    // receive buffer from Uno
  unsigned char lBuf[lBufSize];    // line buffer
  struct commPort Uno = {          // port 24 = /dev/ttyACM0 on RaspPi
    24, 9600, "8N1", *rBuf, 0 };  // try 25 = /dev/ttyACM1 if that doesn't work
  struct sigaction act;            // catch CNTL-C to terminate XML file cleanly
  //  boolean gotLine;

struct WPCmds {
  char *cmdString; 
  int cmdLen; 
  } startWPCmds[] = {        // storeModes order 
    {"\n",1},
    {"report\n",7}, 
    {"csv\n",4},
    {"xmlstart\n",9}
    };

/* Start of main code 
 *------------------------------------------------------------------------------
*/

/* Catch ^C's to exit cleanly */
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);

/* Validate arguments or provide help.
   Determine report-out mode and verify access to database/recording files 
*/
  storeMode = setStoreMode(argc, argv);     // set the storage mode for sampled data
  if (storeMode == sqlMode) initDBMgr();    // test database connection if necessary

  if (! connectToWP(&Uno) ) {               // verify connection to Weather Probe
        fprintf(stderr, "[?WS] WeatherStation cannot open comport to Arduino\n");
	exit(EXIT_FAILURE);
  };

  /* Finally, get down to work.  Tell the probe how we want to see the data   */
  RS232_SendBuf(Uno.portNum, startWPCmds[storeMode].cmdString, startWPCmds[storeMode].cmdLen);
  sleep(1);                                 // give the probe time to set up

  /* This loops "forever" -- or until a ^C is typed */
  while (keepReading) {                     // exit if ^C received, or other trigger in future
    RS232_SendBuf(Uno.portNum, "sample\n", 7);    // tell the probe to take a sample
    sleep(2);                                  // wait a bit for the probe to do its work
                                               // and then start reading lines from the serial port
    while ( getDataLine(&Uno,lBuf) ) {
      if ( (storeMode==sqlMode && (lBuf[0]!='(' || lBuf[1]!='\''))  // csv lines start ('
            || (storeMode==xmlMode && lBuf[0]!=('<')) )  {          // xml lines start <
	fprintf(stderr, "[%WS] Data line formatted incorrectly:\n\t%s", lBuf);
      };
      switch (storeMode) {
        case rptMode:
	  printf("%s", lBuf);                 // for report mode, just print the line
	  break;
        case xmlMode:                         // if xml mode, just copy the line to the file
	  xmlout = !xmlToFile ? stdout : fopen(argv[2], "a");
	  fprintf(xmlout,"%s", lBuf);
	  if (xmlToFile) fclose(xmlout);
	  break;
        case sqlMode:                         // if sql mode, add row to the table
	  appendToDB(lBuf);
	  break;                             // end sql processing
        default:
	  break;
      };                                     // close switch for processing
    };                                       // end getDataLine -- process all lines for this sample
    if (keepReading) sleep(SAMPLE_PERIOD);   // sleep for specified period before sampling again
  };                                         // end while keepReading -- terminate recording
                                             // if there's ever a time when we don't
                                             // keepReading, we'll exit here to terminate cleanly
  if (storeMode==xmlMode) {                  // if we're doing xml mode, end the document
    xmlout = !xmlToFile ? stdout : fopen(argv[2], "a");  
    fprintf(xmlout,"</samples>\n");
    if (xmlToFile) fclose(xmlout);
  };
  exit(EXIT_SUCCESS);
};  // end main()


/* Start of intHandler() 
 *------------------------------------------------------------------------------
 * Triggers end or main loop if a ^C is received on controlling terminal
*/
void intHandler(int sigType) {            // catch ^C's
  keepReading = false;
}; // end intHandler()


/* Start of gotLine()
 *------------------------------------------------------------------------------
 * gotLine() assembles a complete line, ended by '\n', from the Uno serial port
 * and returns it in lBuf as a NULL-terminated string
*/
boolean gotLine(struct commPort *Uno, char *lBuf) {
  int n, i, lnext;
  boolean sawEOL;

  lnext = 0;                       // init output line pointer
  do {
    n = RS232_PollComport(Uno->portNum, Uno->rBuf, rBufSize-1);
    if(n > 0) {
      Uno->rBuf[n] = 0;            // put a NULL at the end
      sawEOL = false;              // look for EOL
      for (i = 0; i<n ; i++) {
        lBuf[lnext++] = Uno->rBuf[i]; // copy receive buffer to line buffer
        if (Uno->rBuf[i] == '\n') {   // did we just finish a line?
	   lBuf[lnext] = 0;        //   yep, terminate it & process
	   sawEOL = true;
	};
      };                           // end of loop to copy receive-->line
    };                             // end n>0
    if ( !sawEOL ) sleep(1);       // if no EOL, wait for more from Uno
  } while ( !sawEOL && keepReading ); // end do...while
  return( sawEOL ? true : false );
};  // end gotLine()

/* This procedure processes any command-line arguments to "ws" to determine
   the mode of reporting (rpt, sql, or xml) and if it's in xml mode, initiates
   the xml output data file if one is given
*/
storeModes setStoreMode(int argc, char *argv[]) {
  storeModes mode;
  char *ch;

  mode  = noMode;
  if (argc>1) {                             // what mode was requested?  rpt, sql, or xml?
    if (strcasecmp(argv[1], "rpt")==0) mode = rptMode;
    if (strcasecmp(argv[1], "sql")==0) mode = sqlMode;
    if (strcasecmp(argv[1], "xml")==0) {
      mode = xmlMode;
      if (argc>=2)                         // if xml, was a filename given?
        xmlout = stdout;                   // if not, output to stdout
      else
        if ( !(xmlout=fopen(argv[2],"a")) ) {
	  fprintf(stderr, "[?WS] Cannot open %s for output in append mode\n", argv[2]);
	  exit(EXIT_FAILURE);
	};
      if (xmlout!=stdout) fclose(xmlout);
    };
  };
  if (mode==noMode) {
    printf("WeatherStation v%s: program to collect and record meteorological data\n", Version);
    printf("\tws <mode> where <mode> = rpt | sql | xml\n");
    printf("\tfor a report-style printout, SQL database recording, or XML data file recording\n");
    exit(EXIT_SUCCESS);
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
boolean getDataLine(struct commPort *Uno, unsigned char lbuf[]) {
#define  maxWait  10                           // if no response from Probe in 10 sec, quit
  //  static int rbufLen=0, rbufPtr=0;            // chars left in rbuf
  static int rBufPtr=0;
  int i,  lbufPtr=0;
  boolean gotLine = false;                   // start loop with no line from probe
  int timerCount = 0;

  lbuf[0] = 0;
  if ( Uno->rBufLen == 0 )                       // no data in buffer so go get some
    while ( ( (Uno->rBufLen = RS232_PollComport(Uno->portNum, Uno->rBuf, rBufSize-1) )<=0 ) 
            && (timerCount < maxWait) 
	    && keepReading )  {
      sleep(1);
      timerCount++;
    };
  if(Uno->rBufLen > 0) {
    Uno->rBuf[Uno->rBufLen] = 0;                        // put a "null" at the end
    lbufPtr=0; 
    do {
      lbuf[lbufPtr++] = Uno->rBuf[rBufPtr++];
    } while ( (rBufPtr<Uno->rBufLen) && (lbuf[lbufPtr-1] != '\n') );
    lbuf[lbufPtr] = 0;                        // null-terminate the line
    if ( rBufPtr == Uno->rBufLen ) rBufPtr = Uno->rBufLen = 0;    // if we're at end of buffer, say it's empty
    gotLine = true;
  };                                          // end of loop to copy receive-->line
  return(gotLine);
};                                          // end getDataLine()

