#ifndef MERCURY_H_
#define MERCURY_H_

#include <Arduino.h>
#include <FixNum.h>

const int8_t TARIFFS = 2;

extern fixnum32_1 volts[4];
extern fixnum32_1 amps[4];
extern fixnum32_1 watts[4];
extern fixnum32_1 hertz;
extern fixnum32_3 totalEnergy[TARIFFS + 1];
extern fixnum32_3 curDayEnergy[TARIFFS + 1];
extern fixnum32_3 prevDayEnergy[TARIFFS + 1];
extern int8_t validValues;
extern int8_t expectedValues;

void setupMercury();
bool checkMercury();

#endif
