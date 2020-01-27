#ifndef ETHERNET_CONFIG_H_
#define ETHERNET_CONFIG_H_

#include <IPAddress.h>

extern IPAddress localIp;
extern void ethernetSetup();
extern bool ethernetPresent;

#endif
