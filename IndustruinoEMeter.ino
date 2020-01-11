#include <Indio.h>
#include <Wire.h>
#include <UC1701.h>
#include <FixNum.h>
#include <Timeout.h>

#include "crc.h"

//------- ALL TIME DEFS ------

const unsigned int STATUS_BLINK_INTERVAL = 1000; // 1 sec

const unsigned long RS485_TIMEOUT = 200;
const unsigned long RS485_DELAY = 10;
const unsigned long UPDATE_PERIOD = 500;

const unsigned long OPEN_CHANNEL_PERIOD = 10000L; // 10 sec

//------- HARDWARE ------

static UC1701 lcd;

#define rs485 Serial

const int BL_PIN = 26; // PWM output pin that the LED backlight is attached to
const int BL_VALUE = 255; // LCD backlight intesity

const int RS485_RTS_PIN = 9; // tx enable pin
const long RS485_BAUD = 9600;

//------- STATE ------

fixnum32_1 volts[4];
fixnum32_1 amps[4]; 
fixnum32_1 watts[4];
fixnum32_1 hertz;
fixnum32_0 curDayEnergy;
fixnum32_0 prevDayEnergy;
int8_t validValues;

const int8_t EXPECTED_VALUES = 13;

Timeout openChannelTimeout(0);
Timeout updateTimeout(0);
Timeout statusTimeout(0);
bool statusBlink;

//------- LCD ------

void updateLCDSummary() {
  //              01234567890123456789
  char buf[21] = "[?]  ??.?Hz   ?????W";
  int8_t missingValues = EXPECTED_VALUES - validValues;
  char status;
  if (missingValues == 0)
    status = '*';
  else if (missingValues < 0)
    status = '+';
  else if (missingValues <= 9)
    status = '0' + missingValues;  
  else if (validValues > 0)
    status = '#';
  else  
    status = '!';
  if (statusTimeout.check()) {
    statusTimeout.reset(STATUS_BLINK_INTERVAL);
    statusBlink = !statusBlink;
    if (statusBlink) status = ' ';
  }
  buf[1] = status;    
  hertz.format(buf + 5, 4, FMT_RIGHT | 1);
  watts[0].format(buf + 14, 5, FMT_RIGHT | 0);
  lcd.setCursor(0, 0);
  lcd.print(buf);
}

inline void updateLCDPhase(uint8_t i) {
  //              01234567890123456789
  char buf[21] = "I: ???V ??.?A ?????W";
  buf[0] = '0' + i;
  volts[i].format(buf + 3, 3, FMT_RIGHT | 0);
  amps[i].format(buf + 8, 4, FMT_RIGHT | 1);
  watts[i].format(buf + 14, 5, FMT_RIGHT | 0);
  lcd.setCursor(0, i);
  lcd.print(buf);
}

inline void updateLCD() {
  updateLCDSummary();
  for (uint8_t i = 1; i <= 3; i++)
    updateLCDPhase(i);
}

//------- RS485 -------

uint8_t sendReceiveRaw(uint8_t* req, uint8_t req_size, uint8_t* resp, uint8_t resp_size) {
  computeCRC(req, req_size - 2);
  // drain read buffer before making request
  while (rs485.available())
    rs485.read();
  // write request  
  digitalWrite(RS485_RTS_PIN, 1); // write 
  delay(RS485_DELAY);
  rs485.write(req, req_size);
  // read response
  digitalWrite(RS485_RTS_PIN, 0); // read
  Timeout timeout(RS485_TIMEOUT);
  uint8_t n = 0;
  while (!timeout.check() && n < resp_size) {
    if (rs485.available())
      resp[n++] = rs485.read();
  }
  return n;
}

bool sendReceive(uint8_t* req, uint8_t req_size, uint8_t* resp, uint8_t resp_size) { 
  uint8_t n = sendReceiveRaw(req, req_size, resp, resp_size);
  if (n < resp_size) 
    return false;
  uint8_t c0 = resp[resp_size - 2];  
  uint8_t c1 = resp[resp_size - 1];
  computeCRC(resp, resp_size - 2);
  return c0 == resp[resp_size - 2] && c1 == resp[resp_size - 1];  
}

bool openChannel() {
  const uint8_t req_size = 11;
  uint8_t req[req_size];
  req[0] = 0x00;
  req[1] = 0x01; // open channel
  req[2] = 0x01; // first level
  for (uint8_t i = 3; i < 9; i++)
    req[i] = 0x01; // password  
  const uint8_t resp_size = 4;
  uint8_t resp[resp_size];   
  if (!sendReceive(req, req_size, resp, resp_size))
    return false;
  return resp[1] == 0x00; // Ok
}

bool checkChannel() {
  if (openChannelTimeout.check()) {
      if (!openChannel())
        return false;
      openChannelTimeout.reset(OPEN_CHANNEL_PERIOD);
  }
  return true;
}

const int32_t INVALID_VALUE = 0x7fffffffL;

int32_t readValue(uint8_t code) {
  if (!checkChannel())
    return INVALID_VALUE;
  const uint8_t req_size = 6;
  uint8_t req[req_size];
  req[0] = 0x00;
  req[1] = 0x08; // read
  req[2] = 0x11; // extra params
  req[3] = code;
  const uint8_t resp_size = 6;
  uint8_t resp[resp_size];
  if (!sendReceive(req, req_size, resp, resp_size)) 
    return INVALID_VALUE;
  return ((uint32_t)(resp[1] & (uint8_t)0x3f) << 16) |
    ((uint32_t)resp[2]) | ((uint32_t)resp[3] << 8);  
}

// num
// x00 -- from reset
// x10 -- for current year
// x20 -- for previous year
// x3x -- for month x
// x40 -- for current day
// x50 -- for prev day
int32_t readEnergy(uint8_t num) {
  if (!checkChannel())
    return INVALID_VALUE;
  const uint8_t req_size = 6;
  uint8_t req[req_size];
  req[0] = 0x00;
  req[1] = 0x05; // read summaries
  req[2] = num;  // what
  req[3] = 0x00; /// all tariffs
  const uint8_t resp_size = 19;
  uint8_t resp[resp_size];
  if (!sendReceive(req, req_size, resp, resp_size)) 
    return INVALID_VALUE;
  return (uint32_t)resp[3] |
    ((uint32_t)resp[4] << 8) | 
    ((uint32_t)resp[1] << 16) |
    ((uint32_t)resp[2] << 24);  
}

template<prec_t prec,prec_t prec2> uint8_t updateValue(FixNum<int32_t,prec>& value, FixNum<int32_t,prec2> x) {
  if (!x.valid()) 
    return 0;
  value = x;
  return 1;
}

void updateValues() {
  int8_t cnt = 0;
  for (uint8_t i = 1; i <= 3; i++)
    cnt += updateValue(volts[i], fixnum32_2(readValue(0x10 + i)));
  for (uint8_t i = 1; i <= 3; i++)
    cnt += updateValue(amps[i], fixnum32_3(readValue(0x20 + i)));
  for (uint8_t i = 0; i <= 3; i++)
    cnt += updateValue(watts[i], fixnum32_2(readValue(0x00 + i)));
  cnt += updateValue(hertz, fixnum32_2(readValue(0x40)));  
  cnt += updateValue(curDayEnergy, fixnum32_0(readEnergy(0x40)));
  cnt += updateValue(prevDayEnergy, fixnum32_0(readEnergy(0x50)));
  validValues = cnt;
}

void updateState() {
  if (updateTimeout.check()) {
    updateTimeout.reset(UPDATE_PERIOD);
    updateValues();
    updateLCD();
  }
}

//------- SETUP & MAIN -------

void setup() {
  // RS485 setup
  rs485.begin(RS485_BAUD);
  pinMode(RS485_RTS_PIN, OUTPUT);
  // LCD setup
  pinMode(BL_PIN, OUTPUT); //set backlight pin to output
  analogWrite(BL_PIN, BL_VALUE); //convert backlight intesity from a value of 0-5 to a value of 0-255 for PWM.
  lcd.begin();  
  lcd.clear();  
  lcd.setCursor(0, 0);  
  lcd.print("{Industruino EMeter}");
  delay(250);
}

void loop() {
  updateState();
}
