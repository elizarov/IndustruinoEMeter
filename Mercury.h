#ifndef MERCURY_H_
#define MERCURY_H_

#include <Arduino.h>
#include <FixNum.h>

struct MercuryTime {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t date;
  uint8_t month;
  uint8_t year;
};

extern MercuryTime mercuryTime;

enum EnergyType { E_TOTAL, E_CUR_DAY, E_PREV_DAY, E_CUR_MONTH, E_PREV_MONTH, E_PR_2_MONTH, E_CUR_YEAR, E_PREV_YEAR };

extern EnergyType displayEnergyType;

const int8_t TARIFFS = 2;

extern fixnum32_1 volts[4];
extern fixnum32_1 amps[4];
extern fixnum32_1 watts[4];
extern fixnum32_1 hertz;
extern fixnum32_3 displayEnergy[TARIFFS + 1];
extern fixnum32_3 curDayEnergy[TARIFFS + 1];
extern fixnum32_3 prevDayEnergy[TARIFFS + 1];
extern int8_t validValues;
extern int8_t expectedValues;

void setupMercury();
bool checkMercury();

#endif
