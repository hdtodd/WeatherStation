/*
 *  Includes and definitions used in the WeatherStation.c ("WS-n") modules
 */

// defs used in SQL db processing
#define myHost     NULL
#define myUsrName  "pi"
#define myPwd      "BetterNotBeRaspberry"
#define myDB       "weather"
#define sqlite3DB "~/WeatherData.db"

#ifdef USE_SQLITE3
  #include <sqlite3.h>
#endif // end USE_SQLITE3


#ifdef USE_MYSQL
  #include <my_global.h>
  #include <my_sys.h>
  #include <mysql.h>
#endif // end USE_MYSQL

#define SAMPLE_PERIOD 288             // 5 min between samples less 12 sec for processing
#define rBufSize 4096
#define lBufSize 4096
#define oBufSize  256
typedef enum  {false=0, true=~0} boolean;
typedef enum {noMode=0, rptMode, sqlMode, xmlMode} storeModes;
struct commPort {
  int portNum;                        // port number
  int baudRate;                       // baud rate
  char commMode[4];                   // communication protocol modes
  unsigned char rBuf[rBufSize];       // receive buffer
  int rBufLen;};                      // length of current buffer contents

void intHandler(int sigType);
void appendToDB(unsigned char lBuf[]);
boolean getDataLine(struct commPort *Uno, unsigned char lBuf[]);
storeModes setStoreMode(int argc, char *argv[]);
void initDBMgr();
boolean connectToWP(struct commPort *Uno);


