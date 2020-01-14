#include <Arduino.h>
#include <SPI.h>                                // for Ethernet
#include <Ethernet2.h>
#include <utility/w5500.h>
#include <Wire.h>                               // for RTC EEPROM MAC
#include "EthernetConfig.h"
#include "lcd.h"

//  ----------------- Networking parameters  -----------------

byte mac[6] = { 0x50, 0x34, 0xd5, 0x35, 0x3c, 0x02 }; // radnomly generated
IPAddress localIp(192, 168, 3, 6);
IPAddress dnsIp(192, 168, 3, 1);
IPAddress gwIp(192, 168, 3, 1);
IPAddress netMask(255, 255, 255, 0);
bool ethernetPresent;

// ----------------- code  -----------------

/////////////////////////////////////////////////////////////////////////
// the RTC has a MAC address stored in EEPROM
uint8_t readByte(uint8_t i2cAddr, uint8_t dataAddr) {
  Wire.beginTransmission(i2cAddr);
  Wire.write(dataAddr);
  Wire.endTransmission(false); // don't send stop
  Wire.requestFrom(i2cAddr, 1);
  return Wire.read();
}

void readMACfromRTC() {
  lcdLog.print("Reading MAC: ");
  int mac_index = 0;
  for (int i = 0; i < 8; i++) {   // read 8 bytes of 64-bit MAC address, 3 bytes valid OUI, 5 bytes unique EI
    byte m = readByte(0x57, 0xf0 + i);
    if (i != 3 && i != 4) {       // for 6-bytes MAC, skip first 2 bytes of EI
      mac[mac_index++] = m;
    }
  }
  for (int u = 0; u < 6; u++) {
    lcdLog.print(mac[u], HEX);
    if (u < 5) lcdLog.print(":");
  }
  lcdLog.println();
}

void ethernetSetup() {
  // Reading MAC does not work... (hangs)
  //readMACfromRTC();
  lcdLog.print("Ethernet: ");
  Ethernet.begin(mac, localIp, dnsIp, gwIp, netMask);
  ethernetPresent = w5500.readVersion() == 4;
  lcdLog.println(ethernetPresent ? "OK" : "NOT FOUND");
}
