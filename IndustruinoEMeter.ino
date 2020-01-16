#include <Indio.h>
#include <Wire.h>
#include <FixNum.h>
#include <Timeout.h>
#include <HardwareSerial.h>

#include "lcd.h"
#include "EthernetConfig.h"
#include "ResetServer.h"
#include "Mercury.h"

//------- ALL TIME DEFS ------

const unsigned int STATUS_BLINK_INTERVAL = 1000; // 1 sec

Timeout statusTimeout(0);
bool statusBlink;

//------- LCD ------

void updateLCDSummary(bool logStatus) {
  //              01234567890123456789
  char buf[21] = "[?]  ??.?Hz   ?????W";
  int8_t missingValues = EXPECTED_VALUES - validValues;
  char status;
  if (statusBlink)
    status = ' ';
  else if (missingValues == 0)
    status = '*';
  else if (missingValues < 0)
    status = '+';
  else if (missingValues <= 9)
    status = '0' + missingValues;
  else if (validValues > 0)
    status = '#';
  else
    status = '!';
  buf[1] = status;
  hertz.format(buf + 5, 4, FMT_RIGHT | 1);
  watts[0].format(buf + 14, 5, FMT_RIGHT | 0);
  lcdLog.printlnAt(0, buf, logStatus);
}

inline void updateLCDPhase(uint8_t i, bool logStatus) {
  //              01234567890123456789
  char buf[21] = "I: ???V ??.?A ?????W";
  buf[0] = '0' + i;
  volts[i].format(buf + 3, 3, FMT_RIGHT | 0);
  amps[i].format(buf + 8, 4, FMT_RIGHT | 1);
  watts[i].format(buf + 14, 5, FMT_RIGHT | 0);
  lcdLog.printlnAt(i, buf, logStatus);
}

void updateLCD(bool logStatus) {
  updateLCDSummary(logStatus);
  for (uint8_t i = 1; i <= 3; i++)
    updateLCDPhase(i, logStatus);
}

bool checkStatusBlink() {
  if (!statusTimeout.check()) return false;
  statusTimeout.reset(STATUS_BLINK_INTERVAL);
  statusBlink = !statusBlink;
  return true;
}

//------- SETUP & MAIN -------

void setup() {
  // LCD setup
  lcdSetup();
  lcdLog.println("{Industruino EMeter}");
  // Ethernet setup
  ethernetSetup();
  resetServerSetup();
  // RS485 setup
  setupMercury();
}

void loop() {
  resetServerCheck();
  bool blink = checkStatusBlink();
  bool mercury = checkMercury();
  if (blink || mercury) updateLCD(blink && validValues > 0);
}
