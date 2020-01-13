#ifndef LCD_H_
#define LCD_H_

#include <Arduino.h>
#include <Print.h>
#include <UC1701.h>

extern UC1701 lcd;

void lcdSetup();

class LcdLogPrint : public Print {
public:
  virtual size_t write(uint8_t b);
  void printlnAt(int row, char* msg, bool log);
};

extern LcdLogPrint lcdLog;

#endif
