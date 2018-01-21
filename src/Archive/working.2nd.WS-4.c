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

Written by HDTodd, Bozeman Montana & Williston Vermont, August, 2015.
Revised January, 2016, to use sqlite3 database as alternative to MySQL

USB serial connection modeled after file: demo_rx.c using the 
  rs232.c and .h files that were written by Teunis van Beelen and 
  available at http://www.teuniz.net/RS-232/ and 
  licensed under GPL version 2.

Link/load with the file rs232.c, included with the package and
  automatically linked if the Makefile is used.

**************************************************/
#ifndef USE_SQLITE3
  #ifndef USE_MYSQL
    #define USE_SQLITE3
  #endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "rs232.h"

#ifdef USE_SQLITE3
  #include <sqlite3.h>
  #ifndef DBName 
    #define DBName "/home/hdtodd/WeatherData.db"
  #endif
#endif
#ifdef USE_MYSQL
  #include <my_global.h>
  #include <my_sys.h>
  #include <mysql.h>
#endif

typedef enum  {false=0, true=~0} boolean;
typedef enum {prtMode=0, csvMode, xmlMode} storeModes;
storeModes storeMode=prtMode;             // by default, just print out

#ifdef USE_MYSQL
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
#endif
#ifdef USE_SQLITE3
  sqlite3 *db;                            // database handle 
  char *zErrMsg = 0,			  // returned error code
       *sql, 				  // sql command string
       outstr[200];                       // space for datetime string
  int  rc;				  // result code from sqlite3 function calls

  static int callback(void *NotUsed, int argc, char **argv, 
	char **azColName); 		  // not used at present but ref'd by sqlite3 call
#endif
  
#define SAMPLE_PERIOD 298               // 5 min between samples with 2 sec for sampling

boolean keepReading=true;

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
  char sqlString[300];

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

#ifdef USE_SQLITE3
  rc = sqlite3_open(DBName, &db);
  if ( rc ) {
    fprintf(stderr, "Can't open or create database %s\n%s\n", DBName, sqlite3_errmsg(db));
    exit(0);
  } else {
    fprintf(stdout, "Opened database %s;\t", DBName);
  };

  // If the table doesn't exist, create it
  strcpy(sqlString, "CREATE TABLE if not exists ProbeData ");
  strcat(sqlString, "(date_time TEXT PRIMARY KEY, mpl_press INT, mpl_temp REAL, dht22_temp REAL, dht22_rh INT);");
  rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
  if ( rc != SQLITE_OK ) {
    fprintf(stderr, "\n?WeatherStation: Can't open or create database table 'ProbeData'\n");
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    exit(0);
  } else {
    fprintf(stdout, "Table 'ProbeData' opened or created successfully\n");
  };
  sqlite3_close(db); 
#endif

  if(RS232_OpenComport(cport_nr, bdrate, mode)) {
    fprintf(stderr, "\n?WeatherStation: Cannot open comport\n");
      return(1);
  }
  sleep(2);                                    // let startup settle down

  /* At this point, we have a serial port open, but we don't know what's at
     the other end, and we don't know if we're using the same serial settings,
     and we don't have the communication buffers are sync'd.  So we'll clear our
     buffer, send a "who are you" query, and confirm that we're talking to 
     each other correctly and in sync.
  */

  // Confirm that we have WeatherProbe ("wp") software running on the Arduino
  boolean haveWP = false;
  int count=0;
  while (! haveWP ) {
    if (! keepReading) return(1);              // if ^C given at kbd, quit
    RS232_PollComport(cport_nr, rbuf, 4095);   // clear buffer to start
    RS232_SendBuf(cport_nr, "WhoRU\n", 6);     // ask who's there
    count++;
    sleep(1);
    n = RS232_PollComport(cport_nr, rbuf, 4095);// get response
    if (n>0) {
      rbuf[n]=0;                             // make a NULL-terminated string
      printf("%WS: WP response to WhoRU = %s\n", rbuf);
      if ( (tolower(rbuf[0])=='w') && (tolower(rbuf[1])=='p') ) {
          printf("%WS: confirmed that WP is connected, running %s\n", rbuf);
          haveWP = true;
	  }
        else {
	  printf("%WS: WP invalid response after %d tries\n", count);
	};
    }
    else {
      printf("%WS: empty buffer after %d sec\n", count-1);
    }; 
  };

  /* Finally, get down to work.  Tell the probe how we want to see the data */
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
  sleep(2);

  while(keepReading) {                         // exit if ^C received, or other trigger in future
    boolean gotLine;
    gotLine = false;                           // start loop with no line from probe

    RS232_SendBuf(cport_nr, "sample\n", 7);    // tell the probe to take a sample
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
	    fprintf(stderr, "%WS: Line formatted incorrectly:\n\t%s", lbuf);
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
#ifdef USE_MYSQL
	    conn = mysql_init (NULL);           // initialize connection handler
	    if (conn == NULL) {
	      fprintf (stderr, "?WS: mysql_init() failed (probably out of memory)\n");
	      exit (EXIT_FAILURE);
	    };
	    if (mysql_real_connect (conn,       // connect to server
                          opt_host_name, opt_user_name, opt_password,
			  opt_db_name, opt_port_num, opt_socket_name, 
			  opt_flags) == NULL) {
	      fprintf (stderr, "?WS: mysql_real_connect() failed\n");
	      mysql_close (conn);
	      exit (EXIT_FAILURE);
	    };

	    strcpy(sqlString, "INSERT INTO ProbeData VALUES");
	    strcat(sqlString, lbuf);
	    if (mysql_query (conn, sqlString) != 0) {   // add the row
	      fprintf(stderr, "?MySQL INSERT statement failed\n");
	      fprintf(stderr, "\t%s\n", mysql_error(conn) );
	    };

	    mysql_close (conn);                  // disconnect from server
#endif
#ifdef USE_SQLITE3
	    /* Open database */
	    rc = sqlite3_open(DBName, &db);
	    if ( rc ) {
	      fprintf(stderr, "?WS: Can't open database file '%s'\n%s\n", 
		      DBName, sqlite3_errmsg(db));
	      exit(EXIT_FAILURE);
	    };

	    /* Create and execute the INSERT with these data values as parameters*/
	    strcpy(sqlString,"INSERT INTO ProbeData (date_time, mpl_press, mpl_temp, dht22_temp, dht22_rh) VALUES");
	    strcat(sqlString, lbuf);
	    /*    fprintf(stderr, sqlString); */
	    rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
	    if ( rc != SQLITE_OK ) {
	      fprintf(stderr, "?WS: SQL error during row insert: %s\n", zErrMsg);
	      fprintf(stderr, "?WS:\tCan't write to database file %s\n", DBName);
	      sqlite3_free(zErrMsg);
	      exit(EXIT_FAILURE);
	    };
	    sqlite3_close(db) ;                  // Done with the DB for now so close it
#endif
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

  
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
  int i;
  for (i=0; i<argc; i++) {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}
