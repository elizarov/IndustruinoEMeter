#include <Arduino.h>
#include <IPAddress.h>
#include <SPI.h>
#include <Ethernet2.h>
#include <EthernetServer.h>
#include <EthernetClient.h>
#include <Timeout.h>

#include "HttpServer.h"

const long HTTP_TIMEOUT = 1000; // 1 sec
int httpPort = 80;

EthernetServer ethernetServer(httpPort);
EthernetClient httpConn;

void httpServerSetup() {
  ethernetServer.begin();
}

void httpResponse(int code, char* msg) {
  httpConn.print("HTTP/1.1 ");
  httpConn.print(code);
  httpConn.print(" ");
  httpConn.println(msg);
  httpConn.println("Content-Type: text/html");
  httpConn.println("Connnection: close");
  httpConn.println();
}

void httpConnDone() {
  httpConn.flush();
  httpConn.stop();
}

void badRequest(char* msg) {
  httpResponse(400, "Bad request");
  httpConn.println(msg);
  httpConnDone();
}

struct Route {
  Route* next;
  char* req;
  void (*func)();
  bool handle(char* path);
};

bool Route::handle(char* path) {
  if (strcmp(req, path) == 0) {
    httpResponse(200, "OK");
    (*func)();
    httpConnDone();
    return true;
  }
  return false;
}

Route* last_route = nullptr;

void httpServerRoute(char *req, void (*func)()) {
  Route* route = new Route{last_route, req, func};
  last_route = route;
}

const int MAX_REQ = 256;
char http_req[MAX_REQ + 1];
int http_req_n;
Timeout httpTimeout;

void httpParse() {
  if (strncmp(http_req, "GET ", 4) != 0) {
    badRequest("Unsupported method");
    return;
  }
  char* path = http_req + 4;
  char* sp = strchr(path, ' ');
  if (sp == nullptr) {
    badRequest("Invalid request");
    return;
  }
  *sp = 0;
  Route* route = last_route;
  while (route != nullptr) {
    if (route->handle(path)) return;
    route = route->next;
  }
  httpResponse(404, "Not found");
  httpConn.print("Requested path not found: ");
  httpConn.println(path);
  httpConnDone();
}

void httpServerCheck() {
  if (!httpConn) {
    httpConn = ethernetServer.available();
    if (!httpConn) return;
    httpTimeout.reset(HTTP_TIMEOUT);
    http_req_n = 0;
  }
  // Connnection
  while (httpConn.available()) {
    char c = httpConn.read();
    if (c == 0x0D) {
      http_req[http_req_n] = 0;
      httpParse();
      return;
    }
    if (http_req_n >= MAX_REQ) {
      badRequest("Request is too big");
      return;
    }
    http_req[http_req_n++] = c;
  }
  if (httpTimeout.check()) {
    badRequest("Request timeout");
  }
}
