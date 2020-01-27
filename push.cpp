#include <Arduino.h>
#include <Ethernet2.h>
#include <FixNum.h>

#include "push.h"
#include "msgbuf.h"

#define log SerialUSB

const char LOCATION_PREFIX = 'H';

const char MESSAGE_OUT_ID = '3';
const char MESSAGE_IN_ID = '4';

/* Data intervals */
const long INITIAL_INTERVAL = 60000L; // 1min
const long RETRY_INTERVAL = 10000L;   // 10sec
const long NEXT_INTERVAL = 300000L;   // 5min

/* Message intervals */
const long INITIAL_MSG_WAIT = 3000L;   // 3sec
const long POLL_MSG_INTERVAL = 60000L; // 1min

/* Timeout for any HTTP interaction */
const long PUSH_TIMEOUT = 30000L; // 30sec

const int MAX_PACKET = 1000;
const int MAX_NUM_LEN = 10;

const byte MASK_ALL = 0xff;

PushItem* last_item = nullptr;

const char HTTP_RES[] = "HTTP/1.1";
const char HTTP_OK[] = "HTTP/1.1 200 OK";
const char PUT[] = "PUT";
const char POST[] = "POST";
const char COOKIE[] = "Cookie: ";
const char SET_COOKIE[] = "Set-Cookie: ";

const int RESPONSE_LINE1       = 0;  // 1st line of response
const int RESPONSE_HEADERS0    = 1;  // response headers start of line; '\n' was seen
const int RESPONSE_HEADERS1    = 2;  // response headers; '\n\r' was seen
const int RESPONSE_HEADERS_ANY = 3;  // response headers, mid of line
const int RESPONSE_BODY        = 4;  // response body; '\n[\r]\n' was seen

const int PCOOKIE_STATE_0          = 0;
const int PCOOKIE_STATE_SET_COOKIE = 12; // length of SET_COOKIE string
const int PCOOKIE_STATE_ERR        = PCOOKIE_STATE_SET_COOKIE + MAX_COOKIE_LEN;
const int PCOOKIE_STATE_DONE       = PCOOKIE_STATE_ERR + 1;

const int PBODY_STATE_0     = 0;  // expect MESSAGE_IN_ID
const int PBODY_STATE_1     = 1;  // expect ','
const int PBODY_STATE_MSG   = 2;  // reading message chars
const int PBODY_STATE_WIDX  = 3;  // wait for ',' to begin index
const int PBODY_STATE_IDX   = 4;  // parsing index
const int PBODY_STATE_DONE  = 5;  // successfully parsed
const int PBODY_STATE_ERR   = 6;  // error

const int MAX_RESPONSE = 300;

EthernetClient client;
bool clientBusy;

int responsePart;
int responseSize;
char response[MAX_RESPONSE + 1];

char packet[MAX_PACKET + 1];

byte composeDataPacket(byte mask, bool &next) {
  int size = 0;
  next = false;
  for (PushItem* cur = last_item; cur != nullptr; cur = cur->next) {
    if (cur->updated & mask == 0) continue;
    int tagLen = strlen(cur->tag);
    int reqLen = 1 + tagLen + 1 + MAX_NUM_LEN + 1;
    if (size + reqLen >= MAX_PACKET) {
      next = true;
      break;
    }
    cur->sending |= mask;
    packet[size++] = LOCATION_PREFIX;
    strncpy(packet + size, cur->tag, tagLen);
    size += tagLen;
    packet[size++] = ',';
    int numLen = formatDecimal(cur->val, packet + size, MAX_NUM_LEN, cur->prec);
    size += numLen;
    packet[size++] = '\n';
  }
  packet[size] = 0;
  return size;
}

void markSent(byte mask, bool success) {
  for (PushItem* cur = last_item; cur != nullptr; cur = cur->next) {
    if (cur->sending & mask == 0) continue;
    cur->sending &= ~mask;
    if (success) cur->updated &= ~mask;
  }
}

PushDest::PushDest(byte mask, char* host, int port, char* url, char* auth) :
  _period(INITIAL_INTERVAL),
  _timeout(PUSH_TIMEOUT)
{
  _mask = mask;
  _host = host;
  _port = port;
  _url = url;
  _auth = auth;
  _method = PUT;
}

bool PushDest::sendPacket(byte size) {
  log.print(_host);
  log.print(':');
  log.print(' ');
  log.print(_method);
  log.print(' ');
  log.print(size, DEC);
  log.println(" bytes");
  if (!client.connect(_host, _port)) {
    log.print(_host);
    log.println(": failed to connect");
    return false;
  }

  // PUT/POST <url> HTTP/1.1
  client.print(_method);
  client.print(' ');
  client.print( _url);
  printExtraUrlParams();
  client.println(" HTTP/1.1");

  // Host: <host>
  client.print("Host: ");
  client.println(_host);

  // <auth>
  client.println(_auth);

  // extra stuff
  printExtraHeaders();

  // Connection: close
  client.println("Connection: close");

  // Content-Length: <size>
  client.print("Content-Length: ");
  client.print(size, DEC);
  client.println();

  // empty line & packet itself
  client.println();
  client.print(packet);
  _timeout.reset(PUSH_TIMEOUT);
  _sending = true;
  responsePart = RESPONSE_LINE1;
  responseSize = 0;
  clientBusy = true;
  return true;
}

void PushDest::doneSend(bool success) {
  markSent(_mask, success);
  _period.reset(success ? NEXT_INTERVAL : RETRY_INTERVAL);
}

void PushDest::parseChar(char ch) {
  switch (responsePart) {
  case RESPONSE_LINE1:
    if (ch == '\n') {
      responsePart = RESPONSE_HEADERS0;
    } else {
      if (ch != '\r' && responseSize < MAX_RESPONSE)
        response[responseSize++] = ch;
    }
    break;
  case RESPONSE_HEADERS0:
    if (ch == '\r')
      responsePart = RESPONSE_HEADERS1;
    else if (ch == '\n')
      responsePart = RESPONSE_BODY;
    else
      responsePart = RESPONSE_HEADERS_ANY;
    parseResponseHeaders(ch);
    break;
  case RESPONSE_HEADERS1:
    if (ch == '\n')
      responsePart = RESPONSE_BODY;
    else
      responsePart = RESPONSE_HEADERS0;
    parseResponseHeaders(ch);
    break;
  case RESPONSE_HEADERS_ANY:
    if (ch == '\n')
      responsePart = RESPONSE_HEADERS0;
    parseResponseHeaders(ch);
    break;
  case RESPONSE_BODY:
    parseResponseBody(ch);
    break;
  }
}

bool PushDest::readResponse() {
  if (!_sending)
    return false;
  if (client.connected() && !_timeout.check()) {
    while (client.available())
      parseChar(client.read());
    if (client.connected())
      return true; // if still connected will read more
  }
  // not connected anymore or timeout
  client.stop();
  _sending = false;
  bool ok = false;
  if (responsePart != RESPONSE_LINE1 && strncmp(response, HTTP_RES, strlen(HTTP_RES)) == 0) {
    response[responseSize] = 0;
    ok = strcmp(response, HTTP_OK) == 0;
    log.print(_host);
    log.print(": ");
    log.println(response);
  } else {
    log.print(_host);
    log.println(": no response");
  }
  doneSend(ok);
  clientBusy = false;
  return false; // done with response
}

void PushDest::check() {
  if (readResponse())
    return; // reading response
  if (clientBusy)
    return; // client is busy serving some other destination
  if (_next == 0 && !_period.check())
    return;
  byte size = composeDataPacket(_mask, _next);
  if (size == 0)
    return;
  if (!sendPacket(size))
    doneSend(false);
}

PushMsgDest::PushMsgDest(byte mask, char* host, int port, char* url, char* auth) :
    PushDest(mask, host, port, url, auth)
{
  _newSession = true;
  _wait = true;
  _period.reset(INITIAL_MSG_WAIT); // need to wait for Ethernet to initialize
  _method = POST;
}

void PushMsgDest::doneSend(bool success) {
  if (success) {
    if (_newSession) {
      _newSession = false;
      _indexOut = 0;
    } else
      MsgBuf.removeMessages(_indexOut);
    if (_parseBodyState != PBODY_STATE_DONE)
      _indexIn = 0; // reset incoming index if message was not properly parsed
    _wait = false; // no forced wait
    _period.reset(POLL_MSG_INTERVAL);
  } else {
    _wait = true;
    _period.reset(RETRY_INTERVAL);
  }
}

void PushMsgDest::printExtraUrlParams() {
  client.print("?id=");
  client.print(MESSAGE_OUT_ID);
  client.print("&last=1");
  if (_indexIn > 0) {
    client.print("&index=");
    client.print(_indexIn, DEC);
  }
  if (_newSession)
    client.print("&newsession1");
  _parseCookieState = PCOOKIE_STATE_0;
  _parseBodyState = PBODY_STATE_0;
}

void PushMsgDest::printExtraHeaders() {
  if (_newSession || _cookie[0] == 0)
    return;
  client.print(COOKIE);
  client.print(_cookie);
  client.println();
}

void PushMsgDest::parseResponseHeaders(char ch) {
  bool eoln = ch == '\r' || ch == '\n';
  if (_parseCookieState < PCOOKIE_STATE_SET_COOKIE) {
    if (eoln)
      _parseCookieState = PCOOKIE_STATE_0;
    else if (ch == pgm_read_byte(SET_COOKIE + _parseCookieState))
      _parseCookieState++;
    else
      _parseCookieState = PCOOKIE_STATE_ERR;
  } else if (_parseCookieState < PCOOKIE_STATE_ERR) {
    if (eoln)
      _parseCookieState = PCOOKIE_STATE_DONE;
    else {
      _cookie[_parseCookieState - PCOOKIE_STATE_SET_COOKIE] = ch;
      _parseCookieState++;
      _cookie[_parseCookieState - PCOOKIE_STATE_SET_COOKIE] = 0;
    }
  } else if (_parseCookieState == PCOOKIE_STATE_ERR) {
    if (eoln)
      _parseCookieState = PCOOKIE_STATE_0;
  } // else PSTATE_DONE do nothing
}

void PushMsgDest::parseResponseBody(char ch) {
  switch (_parseBodyState) {
  case PBODY_STATE_0:
    _parseBodyState = (ch == MESSAGE_IN_ID) ? PBODY_STATE_1 : PBODY_STATE_ERR;
    break;
  case PBODY_STATE_1:
    _parseBodyState = (ch == ',') ? PBODY_STATE_MSG : PBODY_STATE_ERR;
    break;
  case PBODY_STATE_MSG:
    if (ch == ',') {
      Serial.println();
      _parseBodyState = PBODY_STATE_WIDX;
    } else
      Serial.print(ch);
    break;
  case PBODY_STATE_WIDX:
    if (ch == ',') {
      _parseBodyState = PBODY_STATE_IDX;
      _indexIn = 0;
    }
    break;
  case PBODY_STATE_IDX:
    if (ch == '\r' || ch == '\n') {
      _parseBodyState = PBODY_STATE_DONE;
    } else if (ch >= '0' && ch <= '9') {
      _indexIn *= 10;
      _indexIn += ch - '0';
    } else
      _parseBodyState = PBODY_STATE_ERR;
    break;
  }
}

void PushMsgDest::check() {
  if (readResponse())
    return;
  if (clientBusy)
    return;
  bool periodCheck = _period.check();
  if (_wait && !periodCheck)
    return; // we are in a 'forced wait' either on startup or after error
  msg_index_t index = 0;
  int size = MsgBuf.encodeMessages(packet, MAX_PACKET, index);
  // We return if we don't have outgoing message nor incoming messages to confirm nor periodic poll time
  if (size == 0 && _indexIn == 0 && !periodCheck)
    return;
  if (size > 0 && index < _indexOut)
    _newSession = true; // force new session for outgoing messages if index was reset
  if (_newSession) {
    // send empty message to create new session
    size = 0;
    packet[0] = 0;
  } else
    _indexOut = index;
  if (!sendPacket(size))
    doneSend(false);
}

void checkPush() {
  //haworks_data.check();
  //haworks_message.check(); // todo: upload messages, too
}

PushItem* pushTag(const char* tag) {
  PushItem* cur = last_item;
  while (cur != nullptr) {
    if (strcmp(cur->tag, tag) == 0) return cur;
  }
  cur = new PushItem{last_item, tag};
  last_item = cur;
  return cur;
}

void push(PushItem* item, int32_t val, byte prec) {
  item->val = val;
  item->prec = prec;
  item->updated = MASK_ALL;
}
