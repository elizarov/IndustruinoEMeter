#include <Timeout.h>

#include "Mercury.h"
#include "crc.h"

//------- HARDWARE ------

#define rs485 Serial

const int RS485_RTS_PIN = 9; // tx enable pin
const long RS485_BAUD = 9600;
const uint8_t RS485_ADDR = 0; // any device

//------- TIMING ------

const unsigned long RS485_TIMEOUT = 200;
const unsigned long RS485_DELAY = 3;

//------- PUBLIC STATE ------

MercuryTime mercuryTime;
EnergyType displayEnergyType;

fixnum32_1 volts[4];
fixnum32_1 amps[4];
fixnum32_1 watts[4];
fixnum32_1 hertz;
fixnum32_3 displayEnergy[TARIFFS + 1];
fixnum32_3 curDayEnergy[TARIFFS + 1];
fixnum32_3 prevDayEnergy[TARIFFS + 1];
int8_t validValues = 0;
int8_t expectedValues = 1;

//------- REQUESTS ------

struct Req {
  Req* next = nullptr;
  virtual uint8_t req_size() = 0;
  virtual uint8_t res_size() = 0;
  virtual void request() = 0; // puts request in buf
  virtual bool response() = 0; // parses response from buf
  virtual void error(char* m) = 0; // called on error
  int check(int state);  
};

struct OpenChannelReq : public Req {
  virtual uint8_t req_size();
  virtual uint8_t res_size();
  virtual void request();
  virtual bool response();
  virtual void error(char* m);   
};

struct ReadTimeReq : public Req {
  virtual uint8_t req_size();
  virtual uint8_t res_size();
  virtual void request();
  virtual bool response();
  virtual void error(char* m);   
};

template<prec_t prec> struct ReadValueReq : public Req {
  fixnum32_1& value;
  uint8_t code;
  ReadValueReq(fixnum32_1& _value, uint8_t _code) : value(_value), code(_code) {}
  virtual uint8_t req_size();
  virtual uint8_t res_size();
  virtual void request();
  virtual bool response();
  virtual void error(char* m);   
};

// num
// x00 -- from reset
// x10 -- for current year
// x20 -- for previous year
// x3x -- for month x
// x40 -- for current day
// x50 -- for prev day
struct ReadEnergyReq : public Req {
  fixnum32_3& value;
  EnergyType type;
  uint8_t tariff;
  ReadEnergyReq(fixnum32_3& _value, EnergyType _type, uint8_t _tariff) : value(_value), type(_type), tariff(_tariff) {}  
  virtual uint8_t req_size();
  virtual uint8_t res_size();
  virtual void request();
  virtual bool response();
  virtual void error(char* m);   
};

//------- REQUEST/RESPONSE STATE ------

const uint8_t BUF_SIZE = 20;
uint8_t buf[BUF_SIZE];
uint8_t n_read;
Timeout timeout;

const int S_ERROR = -1;
const int S_SUCCESS = -2;

//------- Req ------

int Req::check(int state) {
  switch(state) {
  case 0:
    request();
    computeCRC(buf, req_size() - 2);
    // drain read buffer before making request
    while (rs485.available())
      rs485.read();
    // prepare to write request
    digitalWrite(RS485_RTS_PIN, 1); // write
    timeout.reset(RS485_DELAY); // wait before actually writing
    return 1; 
  case 1:
    if (!timeout.check()) return 1; // wait more
    rs485.write(buf, req_size());
    rs485.flush();
    digitalWrite(RS485_RTS_PIN, 0); // read
    n_read = 0;
    timeout.reset(RS485_TIMEOUT);
    return 2;
  case 2:
    uint8_t res_size = this->res_size();
    while (n_read < res_size && rs485.available()) {
      buf[n_read++] = rs485.read();
    }
    if (n_read < res_size) {
      if (!timeout.check()) return 2; // wait more
      error("timeout"); // timed out
      return S_ERROR;
    }
    uint8_t c0 = buf[res_size - 2];
    uint8_t c1 = buf[res_size - 1];
    computeCRC(buf, res_size - 2);
    bool ok = c0 == buf[res_size - 2] && c1 == buf[res_size - 1];
    if (!ok) {
       error("CRC"); // CRC error
       return S_ERROR;
    }
    // parse response
    if (!response()) {
      error("bad response");
      return S_ERROR; 
    }
    return S_SUCCESS; // done
  } 
}

//------- OpenChannelReq ------

uint8_t OpenChannelReq::req_size() { return 11; }

void OpenChannelReq::request() {
  buf[0] = RS485_ADDR;
  buf[1] = 0x01; // open channel
  buf[2] = 0x01; // first level
  for (uint8_t i = 3; i < 9; i++)
    buf[i] = 0x01; // password  
}

uint8_t OpenChannelReq::res_size() { return 4; }

bool OpenChannelReq::response() {
  return buf[1] == 0x00; // Ok
}

void OpenChannelReq::error(char* m) {
  SerialUSB.print("Channel err: ");
  SerialUSB.println(m);
}

//------- ReadTimeReq ------

uint8_t ReadTimeReq::req_size() { return 5; }

void ReadTimeReq::request() {
  buf[0] = RS485_ADDR;
  buf[1] = 0x04; // read time
  buf[2] = 0x00; // param
}

uint8_t ReadTimeReq::res_size() { return 11; }

uint8_t bcd(uint8_t x) {
  return (x >> 4) * 10 + (x & 0x0f);
}

bool ReadTimeReq::response() {
  mercuryTime.second = bcd(buf[1]);
  mercuryTime.minute = bcd(buf[2]);
  mercuryTime.hour = bcd(buf[3]);
  mercuryTime.date = bcd(buf[5]);
  mercuryTime.month = bcd(buf[6]);
  mercuryTime.year = bcd(buf[7]);
  return true;
}

void ReadTimeReq::error(char* m) {}

//------- ReadValueReq ------

const int32_t INVALID_VALUE = 0x7fffffffL;

template<prec_t prec> uint8_t ReadValueReq<prec>::req_size() { return 6; }

template<prec_t prec> void ReadValueReq<prec>::request() {
  buf[0] = RS485_ADDR;
  buf[1] = 0x08; // read
  buf[2] = 0x11; // extra params
  buf[3] = code;
}

template<prec_t prec> uint8_t ReadValueReq<prec>::res_size() { return 6; }

template<prec_t prec> bool ReadValueReq<prec>::response() {
  int32_t v = ((uint32_t)(buf[1] & (uint8_t)0x3f) << 16) |
    ((uint32_t)buf[2]) | ((uint32_t)buf[3] << 8);
  value = FixNum<int32_t, prec>(v); // interpret as specified precision & convert to target value 
  return true;
}

template<prec_t prec> void ReadValueReq<prec>::error(char* m) {
  value = fixnum32_1(INVALID_VALUE);
}

//------- ReadEnergyReq ------

uint8_t ReadEnergyReq::req_size() { return 6; }

void ReadEnergyReq::request() {
  uint8_t num = 0;
  switch(type) {
    case E_TOTAL: num = 0x00; break;
    case E_CUR_DAY: num = 0x40; break;
    case E_PREV_DAY: num = 0x50; break;
    case E_CUR_MONTH: num = 0x30 + mercuryTime.month; break;
    case E_PREV_MONTH: num = 0x30 + (mercuryTime.month + 10) % 12 + 1; break;
    case E_PR_2_MONTH: num = 0x30 + (mercuryTime.month + 9) % 12 + 1; break;
    case E_CUR_YEAR: num = 0x10; break;
    case E_PREV_YEAR: num = 0x20; break;
  }
  buf[0] = RS485_ADDR;
  buf[1] = 0x05; // read summaries
  buf[2] = num;  // what kind of energy
  buf[3] = tariff; // tariff
}

uint8_t ReadEnergyReq::res_size() { return 19; }

bool ReadEnergyReq::response() {
  int32_t v = (uint32_t)buf[3] |
    ((uint32_t)buf[4] << 8) |
    ((uint32_t)buf[1] << 16) |
    ((uint32_t)buf[2] << 24);;
  value = fixnum32_3(v);  
  return true;
}

void ReadEnergyReq::error(char* m) {
  value = fixnum32_3(INVALID_VALUE);
}

//------- TOP-LEVEL STATE ------

OpenChannelReq openChannel;
Req* last_req = &openChannel;
Req* cur_req = nullptr;
int cur_state;
int ok_values;
ReadEnergyReq* displayEnergyReq[TARIFFS+1];
EnergyType lastDisplayEnergyType = E_TOTAL;
bool refreshDisplayEnergy;

//------- TOP-LEVEL SETUP/CHECK ------

void add(Req* req) {
  last_req->next = req;
  last_req = req;
  expectedValues++;
}

void reinitLoop() {
  cur_req = &openChannel;
  cur_state = 0;
  ok_values = 0;
  for (uint8_t i = 0; i <= TARIFFS; i++)
    displayEnergyReq[i]->type = displayEnergyType;
}

void setupMercury() {
  // init hardware
  rs485.begin(RS485_BAUD);
  pinMode(RS485_RTS_PIN, OUTPUT);
  // allocate requests
  add(new ReadTimeReq());
  // display energy does first after time
  for (uint8_t i = 0; i <= TARIFFS; i++)
    add(displayEnergyReq[i] = new ReadEnergyReq(displayEnergy[i], E_TOTAL, i));
  // the rest of it  
  for (uint8_t i = 1; i <= 3; i++)
    add(new ReadValueReq<2>(volts[i], 0x10 + i));
  for (uint8_t i = 1; i <= 3; i++)
    add(new ReadValueReq<3>(amps[i], 0x20 + i));
  for (uint8_t i = 0; i <= 3; i++)
    add(new ReadValueReq<2>(watts[i], 0x00 + i));
  add(new ReadValueReq<2>(hertz, 0x40));  
  for (uint8_t i = 0; i <= TARIFFS; i++)
    add(new ReadEnergyReq(curDayEnergy[i], E_CUR_DAY, i));
  for (uint8_t i = 0; i <= TARIFFS; i++)
    add(new ReadEnergyReq(prevDayEnergy[i], E_PREV_DAY, i));  
  reinitLoop();  
}

void resetAllValues() {
  Req* req = openChannel.next;
  while(req != nullptr) {
    req->error("no channel");  
    req = req->next;
  }
}

bool checkNext() {
  if (lastDisplayEnergyType != displayEnergyType) {
    // abort & restart on change of displayEnergyType
    reinitLoop();
    lastDisplayEnergyType = displayEnergyType;
    refreshDisplayEnergy = true;
    return false;
  }
  if (refreshDisplayEnergy && cur_req == displayEnergyReq[TARIFFS]) {
    reinitLoop();
    refreshDisplayEnergy = false;  
    return true; // done refreshing
  }
  // regular -- work till the end
  cur_state = 0;
  cur_req = cur_req->next;
  if (cur_req != nullptr) return false; // not done yet
  validValues = ok_values;
  reinitLoop();  
  return true; // done
}

bool checkMercury() {
  cur_state = cur_req->check(cur_state);
  switch(cur_state) {
    case S_ERROR:
      if (cur_req == &openChannel) { 
        // open channel error - retry from scratch
        resetAllValues();
        reinitLoop();
        bool wasOk = validValues > 0;
        validValues = 0;
        return wasOk;
      } else {
        // just a value error - skip it
        return checkNext();
      }
    case S_SUCCESS:
      ok_values++;
      return checkNext();
  }
  return false;
}
