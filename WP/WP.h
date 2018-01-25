// Definitions used by the Arduino Uno Weather_Probe code, WS.ino
//
#define Vers "WP5.1 DB3.0"    // <Code-version> <Database-version>
                              // DB version may be used to create
                              // sqlite3 DB CREATE/INSERT strings,
                              // so be sure to update its version
                              // number if you change the DB
                              // structure below

typedef enum rptMode {none=0, report, csv, xml} rptModes;
typedef enum cmdTypes {vers=0, csample, creport, ccsv, cxmlstart, 
		       cxmlstop, cwhoru, chelp, csettime, crestart,  noCmd} cmds;
static const String cmdNames[] = {"version","sample", "report", "csv", 
				  "xmlstart", "xmlstop", "whoru", "help", "settime",
				  "restart", "unrecognized"};

// Chronodot RTC
                              // VCC to Uno 5v, GND to Uno GND
                              // SCL to Uno pin 13
                              // SDA to Uno pin 11

// TFT pin definitions and parameters for Sainsmart 1.8" TFT SPI on the Uno
                              // VCC to Uno 5v, GND to Uno GND
                              // SCL to Uno pin 13
                              // SDA to Uno pin 11
#define cs  10                // CS to pin 10
#define dc   9                // RS/DC to pin 9
#define rst  8                // Reset to pin 8
#define UPDATE_DELAY 60000    // update TFT every 60 sec if no Pi command comes in
#define TFT_BLACK 0,0,0
#define TFT_WHITE 255,255,255
typedef enum tftTYPE { tftMISSING=0, tftST7735R, tftILI9163C } tftTYPE; // see Prentice
tftTYPE  findTFT(void);

// DS18 pin definitions and parameters
                              // We use powered rather than parasitic mode
                              // VCC to Uno 5v, GND to Uno GND
                              // Connect the DS18 data wire to Uno pin 5
                              //  with 4K7 Ohm pullup to VCC 5V
#define dsResetTime 250       // delay time req'd after search reset in msec
#define DSMAX 4               // max number of devices we're prepared to handle
#define oneWirePin 5          // We'll use Uno pin 5 for OneWire connections to DS18B20

// DHT22 pin definitions and parameters
                              // VCC to Uno 5v, GND to Uno GND
#define DHTPIN 2              // Data pin to Uno pin 2
                              // connect a 4K7-10K pull-up resistor between VCC 5v and data pin
#define DHTTYPE DHT22         // the model of our sensor -- could be DHT11 or DHT21

// MPL3115A2 pin definitions and parameters
                              // VCC to Uno 5v, GND to Uno GND
                              // SCL to Uno SCL
                              // SDA to Uno SDA
uint8_t sampleRate=S_128;     // Sample 128 times for each MPL3115A2 reading
#define MY_ALTITUDE 40        // Set this to your actual GPS-verified altitude in meters
                              // if you want altitude readings to be corrected
                              // Bozeman, MT = 1520, Williston VT = 40
#define FT_PER_METER 3.28084  // conversion

void     (* restartFunc) (void) = 0;  // declare restart function at address 0
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
  float tempf;
};

struct dhtReadings {
  float tempf;
  float rh;
};

struct cdReadings {
  char  dt[20];
  float tempf;
};

struct dsReadings {
  char label[DSMAX][3];
  float tempf[DSMAX];
};

struct recordValues {
  struct cdReadings  cd;
  struct mplReadings mpl;
  struct dhtReadings dht;
  struct dsReadings ds18;
    };
