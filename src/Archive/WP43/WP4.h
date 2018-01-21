// Definitions used by the Arduino Uno Weather_Probe code, WS.ino
//
#define Vers "WP4.3 DB2.0"         // <Code-version> <Database-version>
                                   // DB version may be used to create
                                   // sqlite3 DB CREATE/INSERT strings,
                                   // so be sure to update its version
                                   // number if you change the DB
                                   // structure below

uint8_t sampleRate=S_128;          // Sample 128 times for each MPL3115A2 reading
static  uint8_t cs  = 10;        // define pins used to connect the TFT
static  uint8_t dc  =  9;
static  uint8_t rst =  8;
//#define cs  10                     // TFT pin definitions for SPI on the Uno
//#define dc   9
//#define rst  8

#define TFT_BLACK 0,0,0
#define TFT_WHITE 255,255,255
#define DHTPIN 2                   // connect a 4.7K - 10K resistor between VCC and data pin
#define DHTTYPE DHT22              // the model of our sensor -- could be DHT11 or DHT21
#define UPDATE_FREQ 60000          // update TFT every 60 sec if no Pi command comes in
#define MY_ALTITUDE 1520           // Set this to your actual GPS-verified altitude in meters
                                   // if you want altitude readings to be corrected
                                   // Bozeman, MT = 1520, Williston VT = 40
#define FT_PER_METER 3.28084       // conversion

typedef enum tftTYPE { tftMISSING=0, tftST7735R, tftILI9163C } tftTYPE; // see Prentice
typedef enum rptMode {none=0, report, csv, xml} rptModes;
typedef enum cmdTypes {vers=0, csample, creport, ccsv, cxmlstart, 
		       cxmlstop, cwhoru, chelp, crestart,  noCmd} cmds;
static const String cmdNames[] = {"version","sample", "report", "csv", 
				  "xmlstart", "xmlstop", "whoru", "help", "restart", 
				  "unrecognized"};

void     (* restartFunc) (void) = 0;  // declare restart function at address 0
tftTYPE  findTFT(void);
uint32_t readwrite8(uint8_t cmd, uint8_t bits, uint8_t dummy);

struct fieldDesc {
  const char *fieldName; const char *fieldAttributes; };
struct fieldDesc fieldList[] = {
  {"date_time", "TEXT PRIMARY KEY"},
  {"mpl_press", "INT"},
  {"mpl_temp",  "REAL"},
  {"dht22_temp","REAL"},
  {"dht22_rh",  "INT"},
  NULL,        NULL };

struct mplReadings {
  float alt;
  float press;
  float temp;
};
struct dhtReadings {
  float tempf;
  float tempc;
  float rh;
};

struct cdReadings {
  char  dt[30];
  float temp;
};

struct recordValues {
  struct cdReadings  cd;
  struct mplReadings mpl;
  struct dhtReadings dht;
    };
