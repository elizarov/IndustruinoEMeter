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

void updateLCDSummary() {
  //              01234567890123456789
  char buf[21] = "[?]  ??.?Hz   ?????W";
  int8_t missingValues = expectedValues - validValues;
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
  lcdLog.println(buf);
}

void updateLCDPhase(uint8_t i) {
  //              01234567890123456789
  char buf[21] = "I: ???V ??.?A ?????W";
  buf[0] = '0' + i;
  volts[i].format(buf + 3, 3, FMT_RIGHT | 0);
  amps[i].format(buf + 8, 4, FMT_RIGHT | 1);
  watts[i].format(buf + 14, 5, FMT_RIGHT | 0);
  lcdLog.println(buf);
}

void updateLCDEneryHeader() {
  //              012345678901234567890
  char buf[22] = " -- TOTAL -- TODAY --";
  lcdLog.println(buf);  
}

void updateLCDEnergy(uint8_t i) {
  //              012345678901234567890
  char buf[22] = "T| ??????.? |??.??kWh";
  if (i > 0) buf[0] = '0' + i; 
  totalEnergy[i].format(buf + 3, 8, FMT_RIGHT | 1);
  curDayEnergy[i].format(buf + 13, 5, FMT_RIGHT | 2);
  lcdLog.println(buf);
}

void updateLCD(bool logStatus) {
  lcdLog.reset(logStatus);
  updateLCDSummary();
  for (uint8_t i = 1; i <= 3; i++)
    updateLCDPhase(i);
  updateLCDEneryHeader();
  for (uint8_t i = 0; i <= TARIFFS; i++)
    updateLCDEnergy(i);
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
