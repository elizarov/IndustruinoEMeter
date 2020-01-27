#include <Arduino.h>
#include <FixNum.h>

#include "msgbuf.h"

MsgBufClass MsgBuf;

void MsgBufClass::ensureFreeSpace() {
  while (MSGBUF_SIZE - _size < MAX_MESSAGE_SIZE + MESSAGE_TAG_SIZE)
    removeMessages(MAX_MESSAGE_INDEX);
}

void MsgBufClass::putInternal(char c) {
  _buf[_cur++] = c;
  if (_cur == MSGBUF_SIZE) _cur = 0;
  _cur_size++;
}

char MsgBufClass::getInternal() {
  char ch = _buf[_head++];
  if (_head == MSGBUF_SIZE) _head = 0;
  _size--;
  return ch;
}

void MsgBufClass::putChar(char c) {
  if (c == 0)
    return;
  if (_cur_size == 0)
    ensureFreeSpace();
  if (_cur_size >= MAX_MESSAGE_SIZE)
    return;
  putInternal(c);
}

void MsgBufClass::undoMessage() {
  _cur = _tail;
  _cur_size = 0;
}

void MsgBufClass::saveMessage() {
  if (_cur_size >= MAX_MESSAGE_SIZE) {
    undoMessage();
    return;
  }
  putInternal(0);
  long time = millis();
  putInternal(time >> 24);
  putInternal(time >> 16);
  putInternal(time >> 8);
  putInternal(time);
  _index++;
  if (_index >= MAX_MESSAGE_INDEX) _index = 1;
  putInternal(_index);
  _tail = _cur;
  _size += _cur_size;
  _cur_size = 0;
}

bool MsgBufClass::available() {
  return _size != 0;
}

int MsgBufClass::encodeMessages(char *s, int len, msg_index_t &index) {
  int head0 = _head;
  int size0 = _size;
  int i = 0;
  while (_size != 0 && i + 2 <= len) {
    int head1 = _head;
    int size1 = _size;
    int i1 = i;
    s[i++] = '1';
    s[i++] = ',';
    char ch;
    while (i < len && (ch = getInternal()) != 0)
      s[i++] = ch;
    if (i + MESSAGE_TIME_LEN + MESSAGE_INDEX_LEN + 3 >= len) {
      _head = head1;
      _size = size1;
      i = i1;
      break;
    }
    // format time
    long time = (getInternal() & 0xffL) << 24;
    time |= (getInternal() & 0xffL) << 16;
    time |= (getInternal() & 0xffL) << 8;
    time |= (getInternal() & 0xffL);
    s[i++] = ',';
    i += formatDecimal((int32_t)(time - millis()), s + i, MESSAGE_TIME_LEN, FMT_SIGN);
    // format index
    index = getInternal();
    s[i++] = ',';
    i += formatDecimal(index, s + i, MESSAGE_INDEX_LEN, 0);
    s[i++] = '\n';
    if (index == MAX_MESSAGE_INDEX - 1)
      break; // last message in this session
  }
  s[i] = 0;
  _head = head0;
  _size = size0;
  return i;
}

// when index == MAX_MESSAGE_INDEX removes any one message from head
void MsgBufClass::removeMessages(msg_index_t index) {
  if (_size == 0)
    return;
  int head0 = _head;
  int size0 = _size;
  while (_size != 0) {
    while (getInternal() != 0); // skip message
    for (byte i = 0; i < 4; i++)
      getInternal(); // skip time
    if (getInternal() == index)
      return; // removed
    if (index == MAX_MESSAGE_INDEX)
      return; // removed anyway
  }
  // not found
  _head = head0;
  _size = size0;
}
