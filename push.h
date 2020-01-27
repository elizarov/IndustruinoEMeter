#ifndef PUSH_H_
#define PUSH_H_

#include <Arduino.h>
#include <Timeout.h>
#include <FixNum.h>

#include "msgbuf.h"

struct PushItem {
  PushItem* next;
  const char* tag;
  int32_t val;    // value
  prec_t prec;    // precision
  byte updated;
  byte sending;
};

class PushDest {
protected:
  byte _mask;
  const char* _host;
  int _port;
  const char* _url;
  const char* _auth;
  const char* _method;

  Timeout _period;
  Timeout _timeout;

  bool _next;
  bool _sending;

  bool sendPacket(byte size);
  void parseChar(char ch);
  bool readResponse();

  virtual void doneSend(bool success);
  virtual void printExtraUrlParams() {}
  virtual void printExtraHeaders() {}
  virtual void parseResponseHeaders(char ch) {}
  virtual void parseResponseBody(char ch) {}
public:
  PushDest(byte mask, char* host, int port, char* url, char* auth);
  void check();
};

const long MAX_COOKIE_LEN = 20;

class PushMsgDest : PushDest {
protected:
  long _indexIn;
  msg_index_t _indexOut;
  bool _newSession;
  char _cookie[MAX_COOKIE_LEN + 1];
  byte _parseCookieState; // Parse Set-Cookie response header
  byte _parseBodyState; // Parse response messages from body
  bool _wait;
  virtual void doneSend(bool success);
  virtual void printExtraUrlParams();
  virtual void printExtraHeaders();
  virtual void parseResponseHeaders(char ch);
  virtual void parseResponseBody(char ch);
public:
  PushMsgDest(byte mask, char* host, int port, char* url, char* auth);
  void check();
};

// declared in push_config.cpp
extern PushDest haworks_data;
extern PushMsgDest haworks_message;

PushItem* pushTag(const char* tag);
void push(PushItem* item, int32_t val, prec_t prec);
void checkPush();

template<typename T, prec_t prec> void push(PushItem* item, FixNum<T, prec> val) {
    push(item, val.mantissa(), prec);
}

#endif
