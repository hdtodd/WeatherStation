/* Procedure to confirm that we have WeatherProbe ("wp") software running 
   on the Arduino.  Return "true" when successful, "false" if no port.
   Loops with error message if we can't sync.

   A correctly-functioning probe will respond with "WP v<version #>"
   when it receives a "WhoRU" query.

   Procedure uses "keepReading" external boolean that is set "false"
   if user types CNTL-C to abort program.  That triggers exit if typed.

   Written by HDTodd, 2016-08, as part of the WeatherStation system.
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "rs232.h"
#include "WS.h"

boolean connectToWP(struct commPort *Uno) {
  extern boolean keepReading;
  boolean haveWP = false;
  int n=0, count=0;

  // If we can't open the port at all, return with error
  if ( RS232_OpenComport(Uno->portNum, Uno->baudRate, Uno->commMode) ) return(false);
  sleep(2);                             // let startup settle down

  /* At this point, we have a serial port open, but we don't know what's at
     the other end, and we don't know if we're using the same serial settings,
     and we don't have the communication buffers sync'd.  So we'll clear our
     buffer, send a "who are you" query, and confirm that we're talking to 
     each other correctly and in sync.
  */

  RS232_PollComport(Uno->portNum, Uno->rBuf, rBufSize-1);   // clear buffer to start
  while (! haveWP ) {
    if (! keepReading) exit(0);              // if ^C given at kbd, quit
    RS232_SendBuf(Uno->portNum, "WhoRU\n", 6);     // ask who's there
    count++;
    sleep(1);
    n = RS232_PollComport(Uno->portNum, Uno->rBuf, rBufSize-1);// get response
    if (n>0) {                               // we're looking for "wp"
      Uno->rBuf[n]=0;                        // make a NULL-terminated string
      if ( (tolower(Uno->rBuf[0])=='w') && (tolower(Uno->rBuf[1])=='p') )
          haveWP = true;
        else 
	  fprintf(stderr,"[%WS] WP invalid response after %d tries\n", count);
    }
    else {
      fprintf(stderr,"[%WS] empty buffer after %d sec\n", count-1);
    }; 
  };
  return(haveWP);
};  // End boolean connectToWP(void)
