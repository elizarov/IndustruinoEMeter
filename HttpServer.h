#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <Arduino.h>
#include <Ethernet2.h>
#include <EthernetClient.h>

extern int httpPort;
extern EthernetClient httpConn;

void httpServerSetup();
void httpServerRoute(char *req, void (*func)());
void httpServerCheck();
void httpConnDone();

#endif
