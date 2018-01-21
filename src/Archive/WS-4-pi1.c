/*********************************************************************
Weather_Station v4.0
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

V4.1
Revised Feb-Aug 2016 to modularize code 

V 3.0
Revised January, 2016, to use sqlite3 database as alternative to MySQL
 
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

<<<<<<< HEAD
*********************************************************************/

#ifndef USE_SQLITE3
  #ifndef USE_MYSQL
    #define USE_SQLITE3            // Unless otherwise specified, use sqlite3 db
  #endif
#endif
=======
**************************************************/
#define Version "4.1"
#define SAMPLE_PERIOD 288             // 5 min between samples with 12 sec for processing
>>>>>>> d28665b455755d72a968a5cb0c858d8e1d8ebda8

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "rs232.h"
<<<<<<< HEAD
#include "WeatherStation.h"

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
     NULL, NULL};
=======
#include "ws.h"

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
>>>>>>> d28665b455755d72a968a5cb0c858d8e1d8ebda8

int main(int argc, char *argv[]) 
{
  char *ch;
<<<<<<< HEAD
  FILE *xmlout;
  int i, n, lnext=0, bufcnt=0;
  struct commPort Uno = {          // port 24 = /dev/ttyACM0 on RaspPi
    24, 9600, "8N1" };             // try 25 = /dev/ttyACM1 if that doesn't work
  unsigned char lBuf[lBufSize];    // line buffer
  char sqlString[300];
  struct sigaction act;            // catch CNTL-C to terminate XML file cleanly

/* Start of main code 
 *------------------------------------------------------------------------------
*/
=======
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
>>>>>>> d28665b455755d72a968a5cb0c858d8e1d8ebda8

  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);

<<<<<<< HEAD
/* Determine report-out mode and verify access to database/recording files */
  processCommandLine(argc, argv, &storeMode, &xmlToFile);

  startUnoComm(storeMode, &Uno);

  while(keepReading) {

    RS232_SendBuf(Uno.portNum, "sample\n", 7);   // trigger a sampling
    sleep(2);                      // wait a bit

    if ( gotLine(&Uno, lBuf) ) {
      switch (storeMode) {
	case prtMode:
	  printf("%s", lBuf);      // for printout mode, just print the line
	  break;
	case xmlMode:              // if xml mode, just copy the line to the file
	  xmlout = !xmlToFile ? stdout : fopen(argv[2], "a");
	  fprintf(xmlout,"%s", lBuf);
	  if ( xmlToFile ) fclose(xmlout);
	  break;
	case csvMode:              // if csv mode, add row to the table
	  appendDbRecord(lBuf);
	  break;
      };                           // close switch for processing
      sleep(SAMPLE_PERIOD);        // sleep for specified period before sampling again 
    }; //end gotLine processing
  }; // end while (keepReading) -- only exit here if ^C caught by intHandler

// If we exit that loop because of ^C and we're in XML mode, finish the document
  if (storeMode==xmlMode) {
    xmlout = !xmlToFile ? stdout : fopen(argv[2], "a");  
    fprintf(xmlout,"</samples>\n");
    if (xmlToFile) fclose(xmlout);
  };
  return(0);
};  // end main


/* Start of sqlite3 callback function
 *------------------------------------------------------------------------------
*/
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
  int i;
  for (i=0; i<argc; i++) {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}


/* Start of processCommandLine()
 *------------------------------------------------------------------------------
 *  processCommandLine processes any parameters entered on the command line;
 *  If a reporting mode is requested, that mode is returned in "storeMode".
 *  Default to prtMode if nothing is specified.
 *  If csvMode is requested and we're using sqlite3, we check to see if 
 *    we can write data to the file and make sure the table is defined
 *    in that file.
 *  If xmlMode is requested *and* an output file name is provided,
 *    make sure we can append to that file.
 *
 *  Returns 
 *        storeMode:   The format in which samples are reported out
 *        xmlToFile    If storeMode==xmlMode and an output filename was specified,
 *                        xmlToFile is set to "true" else "false"   
 */
void processCommandLine(int argc, char *argv[], 
                        storeModes *storeMode, boolean *xmlToFile) {
  char sqlString[300];
  char *ch;
  FILE *xmlout;

#ifdef USE_SQLITE3
  sqlite3 *db;
  char *zErrMsg = 0,		   // returned error code
       *sql; 			   // sql command string
  int  rc;			   // result code from sqlite3 function calls
  struct fieldDesc *field;
#endif

  if (argc>1) {                    // -- but did user ask for  prt, csv, or xml?
    for (ch = argv[1]; *ch!=0; ch++) *ch = tolower(*ch);
    *storeMode = prtMode;          // default to printout mode for reporting
    if (strcmp(argv[1], "csv")==0) *storeMode = csvMode;
    if (strcmp(argv[1], "xml")==0) *storeMode = xmlMode;
    if ( *storeMode==xmlMode ) {
      if (argc==2)                 // if xml, was a filename given?
        *xmlToFile = false;        // if not, output to stdout
      else {
        *xmlToFile = true;
	// now make sure we can write to that file
        if ( !(xmlout=fopen(argv[2],"a")) ) {
	  fprintf(stderr, "?Weatherstation: Cannot open %s for XML output in append mode\n", argv[2]);
=======
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
>>>>>>> d28665b455755d72a968a5cb0c858d8e1d8ebda8
	  exit(2);
	};
	fclose(xmlout);
      }; //end else xmlToFile==true
    }; //end if (xml)
  };  //end if (argc>1)

#ifdef USE_SQLITE3
  if ( *storeMode == csvMode ) {
    rc = sqlite3_open(DBName, &db); // make sure we can open the file specified      
    if ( rc ) {
      fprintf(stderr, "?Weatherstation: Can't open or create sqlite3 database %s\n%s\n", 
                      DBName, sqlite3_errmsg(db));
      exit(1);
    } else {
//      fprintf(stdout, "%Weatherstation: Opened database %s;\t", DBName);
    };

    /* If the table doesn't exist, create it from the list of field names & attributes */
    strcpy(sqlString,"CREATE TABLE if not exists ProbeData (");
    for (field=fieldList, 
	   strcat(sqlString, field->fieldName), 
	   strcat(sqlString, " "),
	   strcat(sqlString, field++->fieldAttributes);
	 field->fieldName; 
	 field++) {
      strcat(sqlString, ", "); 
      strcat(sqlString, field->fieldName );
      strcat(sqlString, " "),
	strcat(sqlString, field->fieldAttributes );
    };
<<<<<<< HEAD
    strcat(sqlString, ");");
//    fprintf(stderr, "\t%s\n", sqlString);

    rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
    if ( rc != SQLITE_OK ) {
      fprintf(stderr, "\n?WeatherStation: Can't open or create sqlite3 database table 'ProbeData'\n");
      fprintf(stderr, "\tSQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      exit(0);
    } else {
//      fprintf(stdout, "Table 'ProbeData' opened or created successfully\n");
    };
    sqlite3_close(db); 
=======
  if (strcasecmp(argv[1], "help")==0 || strcasecmp(argv[1], "?")==0 ) {
    printf("WeatherStation v%s: program to collect and record meteorological data\n", Version);
    printf("\tws <mode> where <mode> = prt || sql || xml\n");
    exit(EXIT_SUCCESS);
>>>>>>> d28665b455755d72a968a5cb0c858d8e1d8ebda8
  };
#endif  // USE_SQLITE3
}; // end processCommandLine


/* Start of appendDbRecord()
 *------------------------------------------------------------------------------
 * Appends the data receive from the Uno to a sqlite3 or MySQL database
*/
void appendDbRecord(char *lBuf) {

#ifdef USE_MYSQL
  static char *opt_host_name = myHost;    // server host (default=localhost)
  static char *opt_user_name = myUsrName; // username
  static char *opt_password = myPwd;      // password
  static unsigned int opt_port_num = 0;   // port number (use built-in value)
  static char *opt_socket_name = NULL;    // socket name (use built-in value)
  static char *opt_db_name = myDB;        // database name
  static unsigned int opt_flags = 0;      // connection flags (none)
  static MYSQL *conn;                     // pointer to connection handler
  MYSQL_RES *res;
  MYSQL_ROW row;

  conn = mysql_init (NULL);        // initialize connection handler
  if (conn == NULL) {
    fprintf (stderr, "?mysql_init() failed (probably out of memory)\n");
    exit (1);
  };
  if (mysql_real_connect (conn,    // connect to server
                          opt_host_name, opt_user_name, opt_password,
			  opt_db_name, opt_port_num, opt_socket_name, 
			  opt_flags) == NULL) {
    fprintf (stderr, "?mysql_real_connect() failed\n");
    mysql_close (conn);
     exit (1);
    };

  strcpy(sqlString, "INSERT INTO ProbeData VALUES");
  strcat(sqlString, lBuf);
  if (mysql_query (conn, sqlString) != 0) {   // add the row
    fprintf(stderr, "?MySQL INSERT statement failed\n");
    fprintf(stderr, "\t%s\n", mysql_error(conn) );
  };

  mysql_close (conn);              // disconnect from server
#endif

<<<<<<< HEAD
#ifdef USE_SQLITE3
  sqlite3 *db;                     // database handle 
  char *zErrMsg = 0,		   // returned error code
       *sql; 			   // sql command string
  char sqlString[300];
  int  rc;			   // result code from sqlite3 function calls
  struct fieldDesc *field;

  /* Open database */
  rc = sqlite3_open(DBName, &db);
  if ( rc ) {
    fprintf(stderr, "?WeatherStation: Can't open database file '%s'\n%s\n", 
                    DBName, sqlite3_errmsg(db));
    exit(0);
  };

  /* Create and execute the INSERT with these data values as parameters*/
  strcpy(sqlString,"INSERT INTO ProbeData (");
  for (field=fieldList, strcat(sqlString, field++->fieldName); field->fieldName; field++) {
    strcat(sqlString, ", "); 
    strcat(sqlString, field->fieldName );
  };
  strcat(sqlString, ") VALUES");
  strcat(sqlString, lBuf);
fprintf(stderr, sqlString);
  rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
  if ( rc != SQLITE_OK ) {
    fprintf(stderr, "?WeatherStation: SQL error during row insert: %s\n", zErrMsg);
    fprintf(stderr, "\tCan't write to database file %s\n", DBName);
    sqlite3_free(zErrMsg);
    exit(EXIT_FAILURE);
  };
  sqlite3_close(db) ;              // Done with the DB for now so close it
#endif
};  // end appendDbRecord()


/* Start of startUnoComm()
 *------------------------------------------------------------------------------
 * This procedure starts the communication with the Arduino Uno
 * over the USB serial port.
*/
void startUnoComm(storeModes storeMode, struct commPort *Uno) {

  if ( RS232_OpenComport(Uno->portNum, Uno->baudRate, Uno->commMode) ) {
      printf("?WeatherStation: Cannot open comport to Arduino\n");
      exit(3);
  };
  sleep(2);                        // let setup settle down
  RS232_PollComport(Uno->portNum, Uno->rBuf, rBufSize); // clear buffer to start
  switch (storeMode) {             // tell probe which print mode we want
  case prtMode:
    RS232_SendBuf(Uno->portNum, "printout\n", 9);  // just a straight printout
    break;
  case xmlMode:
    RS232_SendBuf(Uno->portNum, "xmlstart\n", 9);  // tell it xml & get header back
    break;
  case csvMode:
    RS232_SendBuf(Uno->portNum, "csv\n", 4);       // we want results as csv
    break;
  };
  sleep(2);
}; // end startUnoComm()


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

/* Boneyard
   residual error handling ... do we need it?
          if ( (storeMode==csvMode && (lBuf[0]!='(' || lBuf[1]!='\''))  // csv lines start ('
               || (storeMode==xmlMode && lBuf[0]!=('<')) )  {          // xml lines start <
	    fprintf(stderr, "Line formatted incorrectly:\n\t%s", lBuf);

	    lnext=0;                               // end processing this line
	    lBuf[0] = 0;                           //   by releasing line buffer


       outstr[200];                // space for datetime string

*/
=======
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
>>>>>>> d28665b455755d72a968a5cb0c858d8e1d8ebda8
