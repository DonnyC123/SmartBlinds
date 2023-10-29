#include "server.h"
#include "filesystem.h"

const char* ota_name = "esp32-webcam";
const char* ota_password = "esp32-webcam";

const char* mdns_name = "esp32-webcam";

// Create AsyncWebServer object on port 80
WebServer server(80);
WebSocketsServer ws(81);

// IPAddress local_IP(192, 168, 1, 184);
// IPAddress gateway(192, 168, 1, 1);
// IPAddress subnet(255, 255, 0, 0);

uint32_t websocket_conn_count = 0;
volatile bool web_setup = false;
TaskHandle_t wifi_reconnect_task_handle = NULL;
bool is_websocket_connected = false;

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t * payload, size_t len) {
  Serial.printf("payload: %s\r\n", payload);
}

void onEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t len) {
  IPAddress ip = ws.remoteIP(num);
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("WebSocket client #%u connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      websocket_conn_count = ws.connectedClients();
      Serial.printf("Total connected clients: %u\r\n", websocket_conn_count);
      is_websocket_connected = true;
      break;
    case WStype_DISCONNECTED:
      Serial.printf("WebSocket client #%u disconnected\r\n", num);
      websocket_conn_count = ws.connectedClients();
      Serial.printf("Total connected clients: %u\r\n", websocket_conn_count);
      if (websocket_conn_count == 0) {
        Serial.println("No connected clients");
        is_websocket_connected = false;
      }
      break;
    case WStype_TEXT:
      handleWebSocketMessage(num, type, payload, len);
      break;
    case WStype_PONG:
    case WStype_ERROR:
      break;
  }
}

void startWebSocket() { // Start a WebSocket server
  ws.begin();                          // start the websocket server
  ws.onEvent(onEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdns_name);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdns_name);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.html")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from FILESYSTEM
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
                                              // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

void start_ota() { // Start the OTA service
  ArduinoOTA.setHostname(ota_name);
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void wifi_reconnect_task(void * parameters) {
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
    }

    Serial.println("WiFi disconnected...reconnecting");
    // send connection data to gpio tasks

    // restart wifi
    uint8_t mac[6];
    WiFi.macAddress(mac);

    // Connect to Wi-Fi
    WiFiManager wm;
    char web_setup_ap_ssid[20];
    sprintf(web_setup_ap_ssid, "ESD1 %x%x", mac[4], mac[5]);
    Serial.printf("web setup AP: %s\r\n", web_setup_ap_ssid);
    bool res;
    res = wm.autoConnect(web_setup_ap_ssid);

    uint32_t start_attempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_attempt < 10000);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi reconnection failed!");
      vTaskDelay(20000 / portTICK_PERIOD_MS);
    }
    else {
      Serial.println("WiFi reconnected: " + WiFi.localIP());
    }
  }
}

void start_web_setup() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  // Connect to new Wi-Fi
  WiFiManager wm;
  wm.resetSettings();
  web_setup = false;
  ESP.restart();
}

void server_loop_task(void * params) {
  uint32_t counter = 0;
  //BlinkData_t blink_data;
  while (1) {
    ws.loop();
    server.handleClient();
    ArduinoOTA.handle();

    if (web_setup) {
      start_web_setup();
    }

    if (counter++ == 1000) { // checking too frequently crashes the server
      counter = 0;
    }

    vTaskDelay(2);
  }
}

bool wifi_conn_status() {
  return WiFi.status() == WL_CONNECTED;
}

String get_mac_address() {
  return WiFi.macAddress();
}

void start_web_services() {
  Serial.println("Heyyyy"); //use putty to see these
  Serial.println(WiFi.macAddress());
  uint8_t mac[6];
  WiFi.macAddress(mac);

  WiFiManager wm;
  char web_setup_ap_ssid[20];
  sprintf(web_setup_ap_ssid, "ESD1 %x%x", mac[4], mac[5]);
  Serial.printf("web setup AP: %s\r\n", web_setup_ap_ssid);
  bool res;
  res = wm.autoConnect(web_setup_ap_ssid);

  // if(!WiFi.config(local_IP, gateway, subnet)) {
  //   Serial.println("STA Failed to configure");
  // }

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start task to reconnect wifi on disconnect
  xTaskCreatePinnedToCore(
    wifi_reconnect_task,
    "wifi_reconnect",
    4096,
    NULL,
    1,
    &wifi_reconnect_task_handle,
    CONFIG_ARDUINO_RUNNING_CORE
  );

  start_ota();

  startWebSocket();

  startMDNS();

  startServer();

  // Start task to handle server loops
  xTaskCreatePinnedToCore(
    server_loop_task,
    "server_loops",
    8192,
    NULL,
    2,
    NULL,
    CONFIG_ARDUINO_RUNNING_CORE
  );
}