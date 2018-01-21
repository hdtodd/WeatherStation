/*  WS-DBMgr.c
    Procedures to append  meterological data to MySQL or sqlite3 database.

    Written by HDTodd, hdtodd@gmail.com, 2016, for use with WeatherStation.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "WS.h"
char sqlString[300];
static int callback(void *NotUsed, int argc, char **argv, char **azColName);

#ifndef USE_SQLITE3
  #ifndef USE_MYSQL
    #define USE_SQLITE3
  #endif
#endif

#ifdef USE_SQLITE3
  #include <sqlite3.h>
  #ifndef DBName 
    #define DBName sqlite3DB
  #endif
#endif
#ifdef USE_MYSQL
  #include <my_global.h>
  #include <my_sys.h>
  #include <mysql.h>
#endif

#ifdef USE_MYSQL
  static char *opt_host_name = myHost;    // server host (default=localhost)
  static char *opt_user_name = myUsrName; // username (default=login name)
  static char *opt_password =  myPwd;     // password (default=none)
  static unsigned int opt_port_num = 0;   // port number (use built-in value)
  static char *opt_socket_name = NULL;    // socket name (use built-in value)
static char *opt_db_name =     myDB;      // database name (default=none)
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

  void initDBMgr(void) {
#ifdef USE_SQLITE3
  rc = sqlite3_open(DBName, &db);
  if ( rc ) {
    fprintf(stderr, "[?WS] Can't open or create database %s\n%s\n", DBName, sqlite3_errmsg(db));
    exit(0);
  } else {
    fprintf(stdout, "[%WS] Opened database %s\n", DBName);
  };

  // If the table doesn't exist, create it
  strcpy(sqlString, "CREATE TABLE if not exists ProbeData ");
  strcat(sqlString, "(date_time TEXT PRIMARY KEY, mpl_press INT, mpl_temp REAL, dht22_temp REAL, dht22_rh INT,");
  strcat(sqlString, "ds18_1_lbl TEXT, ds18_1_temp REAL,");
  strcat(sqlString, "ds18_2_lbl TEXT, ds18_2_temp REAL,");
  strcat(sqlString, "ds18_3_lbl TEXT, ds18_3_temp REAL,");
  strcat(sqlString, "ds18_4_lbl TEXT, ds18_4_temp REAL)");
  rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
  if ( rc != SQLITE_OK ) {
    fprintf(stderr, "[?WS] Can't open or create database table 'ProbeData'\n");
    fprintf(stderr, "\tSQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    exit(0);
  } else {
    fprintf(stdout, "[%WS] Table 'ProbeData' opened or created successfully\n");
  };
  sqlite3_close(db); 
#endif
  };

void appendToDB(unsigned char lbuf[]) {	    

#ifdef USE_MYSQL
	    conn = mysql_init (NULL);           // initialize connection handler
	    if (conn == NULL) {
	      fprintf (stderr, "[?WS] mysql_init() failed (probably out of memory)\n");
	      exit (EXIT_FAILURE);
	    };
	    if (mysql_real_connect (conn,       // connect to server
                          opt_host_name, opt_user_name, opt_password,
			  opt_db_name, opt_port_num, opt_socket_name, 
			  opt_flags) == NULL) {
	      fprintf (stderr, "[?WS] mysql_real_connect() failed\n");
	      mysql_close (conn);
	      exit (EXIT_FAILURE);
	    };

	    strcpy(sqlString, "INSERT INTO ProbeData VALUES");
	    strcat(sqlString, lbuf);
	    if (mysql_query (conn, sqlString) != 0) {   // add the row
	      fprintf(stderr, "[?WS] MySQL INSERT statement failed\n");
	      fprintf(stderr, "\t%s\n", mysql_error(conn) );
	      mysql_close(conn);
	      exit (EXIT_FAILURE);
	    };

	    mysql_close (conn);                  // disconnect from server
#endif
#ifdef USE_SQLITE3
	    /* Open database */
	    rc = sqlite3_open(DBName, &db);
	    if ( rc ) {
	      fprintf(stderr, "[?WS] Can't open database file '%s'\n%s\n", 
		      DBName, sqlite3_errmsg(db));
	      exit(EXIT_FAILURE);
	    };

	    /* Create and execute the INSERT with these data values as parameters*/
	    strcpy(sqlString,"INSERT INTO ProbeData (date_time, mpl_press, mpl_temp, dht22_temp, dht22_rh,");
	    strcat(sqlString, "ds18_1_lbl, ds18_1_temp,ds18_2_lbl, ds18_2_temp,ds18_3_lbl, ds18_3_temp, ds18_4_lbl, ds18_4_temp) VALUES ");
	    strcat(sqlString, lbuf);
  	    rc = sqlite3_exec(db, sqlString, callback, 0, &zErrMsg);
	    if ( rc != SQLITE_OK ) {
	      fprintf(stderr, "[?WS] SQL error during row insert: %s\n", zErrMsg);
	      fprintf(stderr, "\tCan't write to database file %s: check permissions\n", DBName);
	      sqlite3_free(zErrMsg);
	      exit(EXIT_FAILURE);
	    };
	    sqlite3_close(db) ;                  // Done with the DB for now so close it
#endif
}; // end appendToDB

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
  int i;
  for (i=0; i<argc; i++) {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return(0);
};                                     // end int callback()
