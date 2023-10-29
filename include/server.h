#ifndef SERVER_H_
#define SERVER_H_

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include "esp_wpa2.h"
#include <WiFiManager.h>

// Create AsyncWebServer object on port 80
extern WebServer server;
extern WebSocketsServer ws;

extern volatile bool web_setup;
extern bool is_websocket_connected;

bool wifi_conn_status();
String get_mac_address();
void start_web_services();
void start_ota();

#endif /* SERVER_H_ */
