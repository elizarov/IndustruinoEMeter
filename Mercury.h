#ifndef MERCURY_H_
#define MERCURY_H_

#include <Arduino.h>
#include <FixNum.h>

extern fixnum32_1 volts[4];
extern fixnum32_1 amps[4];
extern fixnum32_1 watts[4];
extern fixnum32_1 hertz;
extern fixnum32_0 curDayEnergy;
extern fixnum32_0 prevDayEnergy;
extern int8_t validValues;

const int8_t EXPECTED_VALUES = 13;

void setupMercury();
bool checkMercury();

#endif
