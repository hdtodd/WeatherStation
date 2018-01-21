/**************************************************
Weather_Station
Raspberry Pi-based program for weather station management.

This management program:
  o  connects to an Arduino programmed to be a "weather probe"
     to collect atmospheric data
  o  periodicly instructs that weather probe to sample its data
     sensors and report back its observations
  o  records the reported data, with date_time stamp, into either
     a MySQL database or an XML data file, or simply report the
     data as a formatted printout.

Written by HDTodd, Bozeman, Montana, August, 2015.

USB serial connection modeled after file: demo_rx.c using the 
  rs232.c and .h files that were written by Teunis van Beelen and 
  available at http://www.teuniz.net/RS-232/ and 
  licensed under GPL version 2.

Link/load with the file rs232.c, included with the package and
  automatically linked if the Makefile is used.

**************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "rs232.h"

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>

typedef enum  {false=0, true=~0} boolean;
typedef enum {prtMode=0, csvMode, xmlMode} storeModes;
storeModes storeMode=prtMode;           // are we storing as CSV or XML?

static char *opt_host_name = NULL;      // server host (default=localhost)
static char *opt_user_name = "root";    // username (default=login name)
static char *opt_password = "ducks1";   // password (default=none)
static unsigned int opt_port_num = 0;   // port number (use built-in value)
static char *opt_socket_name = NULL;    // socket name (use built-in value)
static char *opt_db_name = "weather";   // database name (default=none)
static unsigned int opt_flags = 0;      // connection flags (none)
static MYSQL *conn;                     // pointer to connection handler
MYSQL_RES *res;
MYSQL_ROW row;

#define SAMPLE_PERIOD 298               // 5 min between samples

boolean keepReading=true;
                                        //  less 2 sec sample delay
void intHandler(int sigType) {
  keepReading = false;
}

int main(int argc, char *argv[]) 
{
  char *ch;
  FILE *xmlout;
  int i, n, lnext=0, bufcnt=0,
    cport_nr=24,                        // /dev/ttyS0 (COM1 on windows)
                                        // 24 = /dev/ttyACM0 on RaspPi
			                // try 25 = /dev/ttyACM1 if that doesn't work
    bdrate=9600;                      // 9600 baud works; try 115200
  unsigned char rbuf[4096], lbuf[4096]; // receive buffer and line buffer
  unsigned char obuf[256];              // output buffer
  char mode[]={'8','N','1',0};
  char sqlString[200];

  struct sigaction act;                 // catch CNTL-C to stop program & terminate XML file
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);

  if (argc>1) {                        // what mode are we running in?  prt, csv, or xml?
    for (ch = argv[1]; *ch!=0; ch++) *ch = tolower(*ch);
    if (strcmp(argv[1], "csv")==0) storeMode = csvMode; // unless explicit, assume prt
    if (strcmp(argv[1], "xml")==0) {
      storeMode = xmlMode;
      if (argc==2)                     // if xml, was a filename given?
        xmlout = stdout;               // if not, output to stdout
      else
        if ( !(xmlout=fopen(argv[2],"a")) ) {
	  fprintf(stderr, "?Cannot open %s for output in append mode\n", argv[2]);
	  exit(2);
	};
      if (xmlout!=stdout) fclose(xmlout);
    };
  };

  if(RS232_OpenComport(cport_nr, bdrate, mode)) {
      printf("?Cannot open comport\n");
      return(1);
  }

  sleep(1);                                    // let setup settle down
  RS232_PollComport(cport_nr, rbuf, 4095);     // clear buffer to start
  switch (storeMode) {                         // tell probe which print mode we want
  case prtMode:
    RS232_SendBuf(cport_nr, "printout\n", 9);  // just a straight printout
    break;
  case xmlMode:
    RS232_SendBuf(cport_nr, "xmlstart\n", 9);  // tell it xml & get header back
    break;
  case csvMode:
    RS232_SendBuf(cport_nr, "csv\n", 4);       // we want results as csv
    break;
  };

  while(keepReading) {                         // may be some condition in future
    boolean gotLine;                           //   that triggers loop termination
    gotLine = false;

    RS232_SendBuf(cport_nr, "sample\n", 7);    // start by sampling
    sleep(2);                                  // wait a bit
    n = RS232_PollComport(cport_nr, rbuf, 4095);
    if(n > 0) {
      bufcnt++;
      rbuf[n] = 0;                               // put a "null" at the end
      for (i = 0; i<n ; i++) {                   // copy receive buffer to line buffer
	lbuf[lnext++] = rbuf[i]; 
	if (rbuf[i] == '\n') {                   // did we just finish a line?
	  lbuf[lnext] = 0;                       //   yep, terminate it & process
          if ( (storeMode==csvMode && (lbuf[0]!='(' || lbuf[1]!='\''))  // csv lines start ('
               || (storeMode==xmlMode && lbuf[0]!=('<')) )  {          // xml lines start <
	    fprintf(stderr, "Line formatted incorrectly:\n\t%s", lbuf);
	    break;
	  };
	  gotLine = true;
	  switch (storeMode) {
	  case prtMode:
	    printf("%s", lbuf);                 // for printout mode, just print the line
	    break;
	  case xmlMode:                         // if xml mode, just copy the line to the file
	    if (xmlout!=stdout) xmlout = fopen(argv[2], "a");
	    fprintf(xmlout,"%s", lbuf);
	    if (xmlout!=stdout) fclose(xmlout);
	    break;
	  case csvMode:                         // if csv mode, add row to the table
	    conn = mysql_init (NULL);           // initialize connection handler
	    if (conn == NULL) {
	      fprintf (stderr, "?mysql_init() failed (probably out of memory)\n");
	      exit (1);
	    };
	    if (mysql_real_connect (conn,       // connect to server
                          opt_host_name, opt_user_name, opt_password,
			  opt_db_name, opt_port_num, opt_socket_name, 
			  opt_flags) == NULL) {
	      fprintf (stderr, "?mysql_real_connect() failed\n");
	      mysql_close (conn);
	      exit (1);
	    };

	    strcpy(sqlString, "INSERT INTO ProbeData VALUES");
	    strcat(sqlString, lbuf);
	    if (mysql_query (conn, sqlString) != 0) {   // add the row
	      fprintf(stderr, "?MySQL INSERT statement failed\n");
	      fprintf(stderr, "\t%s\n", mysql_error(conn) );
	    };

	    mysql_close (conn);                  // disconnect from server
	    break;                               // end csv processing
	  };                                     // close switch for processing
	  
	  lnext=0;                               // end processing this line
	  lbuf[0] = 0;                           //   by releasing line buffer
	};                                       // end of line processing
      };                                         // end of loop to copy receive-->line
    };                                           // end of processing of receive buffer
    if (gotLine) sleep(SAMPLE_PERIOD);           // sleep for specified period before sampling again 
  };                                             // if there's ever a time when we don't
                                                 //   keepReading, we'll exit here
  if (storeMode==xmlMode) {                      // and if we're doing xml mode, end the document
    if (xmlout!=stdout) xmlout = fopen(argv[2], "a");  
    fprintf(xmlout,"</samples>\n");
    if (xmlout!=stdout) fclose(xmlout);
  };
  return(0);
}

  
