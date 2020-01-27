#ifndef MSGBUF_H_
#define MSGBUF_H_

#include <Arduino.h>

const int MSGBUF_SIZE = 300;
const int MAX_MESSAGE_SIZE = 200;
const int MESSAGE_TAG_SIZE = 5;

const uint8_t MESSAGE_TIME_LEN = 10;
const uint8_t MESSAGE_INDEX_LEN = 2;

typedef int8_t msg_index_t;
const msg_index_t MAX_MESSAGE_INDEX = 100;

class MsgBufClass {
private:
  byte _buf[MSGBUF_SIZE];
  int _head;
  int _tail;
  int _size;
  int _cur;
  int _cur_size;
  msg_index_t _index;

  void ensureFreeSpace();
  void putInternal(char c);
  char getInternal();
public:
  void putChar(char c);
  void undoMessage();
  void saveMessage();
  bool available();
  int encodeMessages(char *s, int len, msg_index_t &index);
  void removeMessages(msg_index_t index);
};

extern MsgBufClass MsgBuf;

#endif
