#ifndef MERCURY_H_
#define MERCURY_H_

#include <Arduino.h>
#include <FixNum.h>

const int8_t TARIFFS = 2;

extern fixnum32_1 volts[4];
extern fixnum32_1 amps[4];
extern fixnum32_1 watts[4];
extern fixnum32_1 hertz;
extern fixnum32_0 totalEnergy[TARIFFS + 1];
extern fixnum32_0 curDayEnergy[TARIFFS + 1];
extern fixnum32_0 prevDayEnergy[TARIFFS + 1];
extern int8_t validValues;

const int8_t EXPECTED_VALUES = 15 + 3 * TARIFFS;

void setupMercury();
bool checkMercury();

#endif
