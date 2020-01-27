#include <Indio.h>
#include <Wire.h>
#include <FixNum.h>
#include <Timeout.h>
#include <Button.h>
#include <HardwareSerial.h>
#include <Reset.h>

#include "lcd.h"
#include "EthernetConfig.h"
#include "HttpServer.h"
#include "Mercury.h"
#include "push.h"

//------- Button ------

Button upButton(25);
Button enterButton(24);
Button downButton(23);

//------- STATE ------

const unsigned long STATUS_BLINK_INTERVAL = 1000; // 1 sec
const unsigned long DISPLAY_RESET_TIMEOUT = 30000; // 30 sec

Timeout statusTimeout(0);
bool statusBlink;
Timeout displayResetTimeout;

enum { D_TOTAL, D_CUR_DAY, D_PREV_DAY, D_CUR_MONTH, D_PREV_MONTH, D_PR_2_MONTH, D_CUR_YEAR, D_PREV_YEAR, D_TIME, D_MAX };

char* displayNames[] = {
  "TOTAL",
  "CUR DAY",
  "PREV DAY",
  "CUR MONTH",
  "PREV MONTH",
  "PR-2 MONTH",
  "CUR YEAR",
  "PREV YEAR",
  "DATE/TIME"
};

EnergyType displayTypes[] = { E_TOTAL, E_CUR_DAY, E_PREV_DAY, E_CUR_MONTH, E_PREV_MONTH, E_PR_2_MONTH, E_CUR_YEAR, E_PREV_YEAR, E_TOTAL };

int displayMode = D_TOTAL;

//------- LCD ------

void printSummary(Print& out) {
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
  out.println(buf);
}

void printPhase(Print& out, uint8_t i) {
  //              01234567890123456789
  char buf[21] = "I: ???V ??.?A ?????W";
  buf[0] = '0' + i;
  volts[i].format(buf + 3, 3, FMT_RIGHT | 0);
  amps[i].format(buf + 8, 4, FMT_RIGHT | 1);
  watts[i].format(buf + 14, 5, FMT_RIGHT | 0);
  out.println(buf);
}

void printHeader(Print& out) {
  //              01234567890123456789
  char buf[21] = " --               --";
  char* name = displayNames[displayMode];
  strncpy(&buf[4], name, strlen(name));
  out.println(buf);
}

void printEnergy(Print& out, uint8_t i) {
  //              012345678901234567890
  char buf[22] = "T ????????.??kWh     ";
  if (i > 0) buf[0] = '0' + i;
  displayEnergy[i].format(buf + 2, 11, FMT_RIGHT | 2);
  out.println(buf);
}

int32_t daystart = millis();
int16_t updays = 0;

void printTime(Print& out) {
  //              01234567890123456789
  char buf[21] = "  ??-??-?? ??:??:?? ";
  char upd[21] = " rescan: ???ms / ?? ";
  char upt[21] = " up: ????d ??:??:?? ";
  formatDecimal(mercuryTime.year, buf + 2, 2, FMT_ZERO);
  formatDecimal(mercuryTime.month, buf + 5, 2, FMT_ZERO);
  formatDecimal(mercuryTime.date, buf + 8, 2, FMT_ZERO);
  formatDecimal(mercuryTime.hour, buf + 11, 2, FMT_ZERO);
  formatDecimal(mercuryTime.minute, buf + 14, 2, FMT_ZERO);
  formatDecimal(mercuryTime.second, buf + 17, 2, FMT_ZERO);
  formatDecimal(mercuryUpdateTime, upd + 9, 3, FMT_RIGHT);
  formatDecimal(validValues, upd + 17, 2, FMT_RIGHT);
  // prepare uptime
  int32_t time = millis();
  while (time - daystart > Timeout::DAY) {
    daystart += Timeout::DAY;
    updays++;
  }
  formatDecimal(updays, upt + 5, 4, FMT_RIGHT);
  time -= daystart;
  time /= 1000; // convert seconds
  formatDecimal(time % 60, upt + 17, 2, FMT_ZERO);
  time /= 60; // minutes
  formatDecimal(time % 60, upt + 14, 2, FMT_ZERO);
  time /= 60; // hours
  formatDecimal(time, upt + 11, 2, FMT_ZERO);
  // output
  out.println(buf);
  out.println(upd);
  out.println(upt);
}

void printStatus(Print& out) {
  printSummary(out);
  for (uint8_t i = 1; i <= 3; i++)
    printPhase(out, i);
  printHeader(out);
  switch(displayMode) {
    case D_TIME:
      printTime(out);
      break;
    default:
      for (uint8_t i = 0; i <= TARIFFS; i++)
        printEnergy(out, i);
  }
}

void updateLCD(bool logStatus) {
  lcdLog.reset(logStatus);
  printStatus(lcdLog);
}

bool checkStatusBlink() {
  if (!statusTimeout.check()) return false;
  statusTimeout.reset(STATUS_BLINK_INTERVAL);
  statusBlink = !statusBlink;
  return true;
}

bool checkButtons() {
  bool upd = false;
  if (upButton.check() && upButton.pressed()) {
    if (displayMode == 0) displayMode = D_MAX;
    displayMode--;
    upd = true;
  }
  if (downButton.check() && downButton.pressed()) {
    displayMode++;
    if (displayMode == D_MAX) displayMode = 0;
    upd = true;
  }
  if (upd) displayResetTimeout.reset(DISPLAY_RESET_TIMEOUT);
  if (displayResetTimeout.check()) {
    if (displayMode != 0) upd = true;
    displayMode = 0;
  }
  if (upd) {
    displayEnergyType = displayTypes[displayMode];
  }
  return upd;
}

//------- HTTP ROUTES -------

void httpRoot() {
  httpConn.print("<pre>");
  printStatus(httpConn);
  printTime(httpConn);
  httpConn.println("</pre>");
}

void httpReset() {
  httpConn.println("Rebooting");
  httpConnDone();
  delay(10);
  immediateReset();
}

//------- PUSH DATA -------

PushItem* hertzTag;
PushItem* wattsTag[4];
PushItem* voltsTag[4];
PushItem* ampsTag[4];
PushItem* curDayEnergyTag[TARIFFS + 1];
PushItem* prevDayEnergyTag[TARIFFS + 1];

PushItem* setupTag(char* prefix, int i, char* suffix) {
  auto pl = strlen(prefix);
  auto sl = strlen(suffix);
  int il = i > 0 ? 1 : 0;
  char* c = new char[pl + il + sl + 1];
  strncpy(c, prefix, pl);
  if (i > 0) c[pl] = '0' + i;
  strncpy(c + pl + il, suffix, sl);
  c[pl + il + sl] = 0;
  return pushTag(c);
}

void setupPushTags() {
  hertzTag = pushTag("Ef");
  for (int i = 0; i <= 3; i++) {
    wattsTag[i] = setupTag("E", i, "");
    if (i > 0) {
      voltsTag[i] = setupTag("E", i, "v");
      ampsTag[i] = setupTag("E", i, "a");
    }
  }
  for (int i = 0; i <= TARIFFS; i++) {
    curDayEnergyTag[i] = setupTag("E", i, "c");
    prevDayEnergyTag[i] = setupTag("E", i, "p");
  }
}

void pushData() {
  push(hertzTag, hertz);
  for (int i = 0; i <= 3; i++) {
    push(wattsTag[i], watts[i]);
    if (i > 0) {
      push(voltsTag[i], volts[i]);
      push(ampsTag[i], amps[i]);
    }
  }
  for (int i = 0; i <= TARIFFS; i++) {
    push(curDayEnergyTag[i], curDayEnergy[i]);
    push(prevDayEnergyTag[i], prevDayEnergy[i]);
  }
}

//------- SETUP & MAIN -------

void setup() {
  // LCD setup
  lcdSetup();
  lcdLog.println("{Industruino EMeter}");
  // Ethernet setup
  ethernetSetup();
  // Http setup
  if (ethernetPresent) {
    httpServerRoute("/", &httpRoot);
    httpServerRoute("/reset", &httpReset);
    httpServerSetup();
    // print http addr
    lcdLog.print(localIp);
    lcdLog.print(":");
    lcdLog.print(httpPort);
    lcdLog.println();
  }
  // RS485 setup
  setupMercury();
  setupPushTags();
}

void loop() {
  if (ethernetPresent) {
    httpServerCheck();
    checkPush();
  }
  bool blink = checkStatusBlink();
  bool mercury = checkMercury();
  bool button = checkButtons();
  if (blink || mercury || button) {
    updateLCD(blink && validValues > 0);
  }
  if (mercury) {
    pushData();
  }
}
