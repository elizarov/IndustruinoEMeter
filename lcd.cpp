#include <Timeout.h>
#include "lcd.h"

const int BL_PIN = 26; // LED backlight

UC1701 lcd;
LcdLogPrint lcdLog;
int logLine = 0;
Timeout serialTimeout;

void lcdSetup() {
  pinMode(BL_PIN, OUTPUT); //set backlight pin to output
  digitalWrite(BL_PIN, 1);
  lcd.begin();
  lcd.clear();
  lcd.setCursor(0, logLine);
  SerialUSB.begin(57600);
  serialTimeout.reset(3000); // 3 sec max wait
  while (!SerialUSB && !serialTimeout.check()) {} // wait for serial monitor connection

}

size_t LcdLogPrint::write(uint8_t b) {
  if (b == '\r') {
    lcd.setCursor(0, logLine);
  } else if (b == '\n') {
    lcd.setCursor(0, ++logLine);
  } else
    lcd.write(b);
  return SerialUSB.write(b);
}

void LcdLogPrint::printlnAt(int row, char* msg, bool log) {
  lcd.setCursor(0, row);
  lcd.print(msg);  
  logLine = row + 1;
  lcd.setCursor(0, logLine);
  if (log) SerialUSB.println(msg);
}
