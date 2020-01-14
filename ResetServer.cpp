#include <Arduino.h>
#include <IPAddress.h>
#include <SPI.h>
#include <Ethernet2.h> 
#include <EthernetServer.h>
#include <EthernetClient.h>
#include <Reset.h>

#include "ResetServer.h"
#include "lcd.h"
#include "EthernetConfig.h"

//------- params -------

int port = 8080;
const char* pathStr = "password";
const char* resetStr = "reset";

//------- EthernetReset -------

EthernetServer ethernetServer(port);
EthernetClient ethernetClient;

void stdResponse(char *msg) {
  ethernetClient.println("HTTP/1.1 200 OK");
  ethernetClient.println("Content-Type: text/html");
  ethernetClient.println("Connnection: close");
  ethernetClient.println();
  ethernetClient.println("<!DOCTYPE HTML>");
  ethernetClient.println("<html>");
  ethernetClient.println(msg);
  ethernetClient.println("</html>");
}

void notFoundResponse(char *msg) {
  ethernetClient.println("HTTP/1.1 404 OK");
  ethernetClient.println("Content-Type: text/html");
  ethernetClient.println("Connnection: close");
  ethernetClient.println();
  ethernetClient.println("<!DOCTYPE HTML>");
  ethernetClient.println("<html>");
  ethernetClient.println(msg);
  ethernetClient.println("</html>");
}

void reset() {
  delay(10UL);
  ethernetClient.stop();
  immediateReset();
}

void resetServerSetup() {
  if (!ethernetPresent) return;
  ethernetServer.begin();
  lcdLog.print(localIp);
  lcdLog.print(":");
  lcdLog.print(port);
  lcdLog.println();
}

#define HTTP_REQ_PREFIX_LENGTH     5   // "GET /"
#define HTTP_SLASH_LENGTH          1   // "/"
#define HTTP_REQ_POSTFIX_LENGTH    9   // " HTTP/1.1"
#define MAX_COMMAND_LENGTH        10

#define REQUEST_BUFFER_EXTRA_SIZE (HTTP_REQ_PREFIX_LENGTH + HTTP_SLASH_LENGTH + MAX_COMMAND_LENGTH + HTTP_REQ_POSTFIX_LENGTH)

void resetServerCheck() {
  if (!ethernetPresent) return;
  char http_req[strlen(pathStr) + REQUEST_BUFFER_EXTRA_SIZE];
  ethernetClient = ethernetServer.available();
  if (!ethernetClient) return;
  while (ethernetClient.connected()) {
    if (!ethernetClient.available()) continue;
    char c;
    char *url = http_req;
    while ((c = ethernetClient.read()) != '\n')
       if (url < (http_req + strlen(pathStr) + REQUEST_BUFFER_EXTRA_SIZE))
          *(url++) = c;
    url = http_req + HTTP_REQ_PREFIX_LENGTH;
    ethernetClient.flush();
    if (!strncmp(url, pathStr, strlen(pathStr))) {
       url += (strlen(pathStr) + HTTP_SLASH_LENGTH);
       if (!strncmp(url, resetStr, strlen(resetStr))) {
          stdResponse("Rebooting...");
          reset();
       } else
          stdResponse("Wrong command");
    } else
       notFoundResponse("Unknown path");
    break;
  }
  delay(10UL);
  ethernetClient.stop();
}
