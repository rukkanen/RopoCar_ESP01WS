#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <pt.h>
#include <secrets.h>
#include <SerialRedirect.h>

static struct pt ptHandleIncomingMessage, ptInitWlan, ptMakeSerialConnection; 

// Web server on port 80
ESP8266WebServer httpServer(80);

// Current mode of the robot car
String mode = "guard";
// Wlan and serial connection status
static bool wlanConnected = false;
static bool serialConnected = false;

// Placeholder battery voltage variables
float motorBatteryVoltage = 0.0;
float computeBatteryVoltage = 0.0;
// Task sleep times
const static unsigned int handleMessageWAIT = 4000;
const static unsigned int initWlanWAIT = 1000;

// Function declarations
void handleRoot();
void handleModeChange();
void handleCamera();
void updateBatteryVoltages();
void handleIncomingMessage(String message);
void sendCommandToArduino(String command);
/**
 * Utility functions
 */
void printToSerial(String message) {
    printf("%s\n", message.c_str());
}

void printToSerial(String id, String message) {
    printf("%s: %s\n", id.c_str(), message.c_str());
}

/**
 * Business logic functions
 */
void handleRoot() {
    String html = "<html>\
                    <head>\
                      <title>Robot Car Guard</title>\
                      <meta http-equiv=\"refresh\" content=\"1\">\
                    </head>\
                    <body>\
                      <h1>Robot Car Guard</h1>\
                      <p>Current Mode: " + mode + "</p>\
                      <p>\
                        <button onclick=\"location.href='/mode/toy'\">Toy/Map Mode</button>\
                        <button onclick=\"location.href='/mode/guard'\">Guard Mode</button>\
                      </p>\
                      <p>Motor Battery Voltage: " + String(motorBatteryVoltage) + " V</p>\
                      <p>Compute Battery Voltage: " + String(computeBatteryVoltage) + " V</p>\
                      <p><img src='/camera' width='320' height='240'></p>\
                    </body>\
                  </html>";
    httpServer.send(200, "text/html", html);
}

void handleModeChange() {
    if (httpServer.uri() == "/mode/toy") {
        mode = "toy";
        printToSerial("1:mode_toy");
    } else if (httpServer.uri() == "/mode/guard") {
        mode = "guard";
        printToSerial("1:mode_guard");
    }
    handleRoot();  // Refresh the webpage
}

void handleCamera() {
    // the arduino will offer ~1/sec a new picture

    if (Serial.available()) {
        String imageData = Serial.readStringUntil('\n');
        httpServer.send(200, "image/jpeg", imageData);  // Send image data to client
    } else {
        httpServer.send(200, "image/jpeg", "<image data not available>");
    }
}

void updateBatteryVoltages() {
    if (Serial.available()) {
        String data = Serial.readStringUntil('\n');
        int commaIndex = data.indexOf(',');
        motorBatteryVoltage = data.substring(0, commaIndex).toFloat();
        computeBatteryVoltage = data.substring(commaIndex + 1).toFloat();
    }
}

PT_THREAD(handleIncomingMessage(struct pt *pt)) {
  PT_BEGIN(pt);
  while (Serial.available()) {
      String message = Serial.readStringUntil('\n');
      if (message == "ping") {
          printToSerial("ESP: pong");  // Respond to ping with pong
      } else if (message.startsWith("battery_level=")) {
          // Parse and update battery levels from the incoming message
          int commaIndex = message.indexOf(',');
          motorBatteryVoltage = message.substring(0, commaIndex).toFloat();
          computeBatteryVoltage = message.substring(commaIndex + 1).toFloat();
      } else if (message == "picture_start") {
          // Send the picture to the client
          handleCamera();
      } else {
          printToSerial("ESP: Unknown command: " + message);
      }
      // Wait for 4 seconds (4000 milliseconds) before continuing// Start the timer
      unsigned int startTime = millis();
      PT_WAIT_UNTIL(pt, millis() - startTime >= handleMessageWAIT);
  }
  PT_END(pt);
}

PT_THREAD(initWlan(struct pt *pt)) {
  PT_BEGIN(pt);
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {      
      Serial.print(".");
      unsigned int startTime = millis();
      PT_WAIT_UNTIL(pt, millis() - startTime >= initWlanWAIT);
  }
  wlanConnected = true;
  printToSerial("\nWiFi connected");
  printToSerial("IP address: " + WiFi.localIP().toString());
  PT_END(pt);
}

PT_THREAD(makeSerialConnection(struct pt *pt)) {
  PT_BEGIN(pt);
  Serial.begin(9600);
  while (!Serial) {
      PT_WAIT_UNTIL(pt, Serial);
  }
  PT_END(pt);
}

// setup the httpServer routes
void initHttpServer() {
  httpServer.on("/", handleRoot);
  httpServer.on("/mode/toy", handleModeChange);
  httpServer.on("/mode/guard", handleModeChange);
  httpServer.on("/camera", handleCamera);
  httpServer.begin();
  Serial.println("SHTTP server started!");
}

void setup() {
  PT_INIT(&ptInitWlan);
  PT_INIT(&ptHandleIncomingMessage);
  // Set up the serial connection
  PT_INIT(&ptMakeSerialConnection);

  // Set up the web server routes
  httpServer.on("/", handleRoot);
  httpServer.on("/mode/toy", handleModeChange);
  httpServer.on("/mode/guard", handleModeChange);
  httpServer.on("/camera", handleCamera);

  // Start the web server
  httpServer.begin();
  Serial.println("HTTP server started!");
}


void loop() {
  if (!serialConnected) {
    PT_SCHEDULE(makeSerialConnection(&ptMakeSerialConnection));
  }
  if (!wlanConnected) {
    PT_SCHEDULE(initWlan(&ptInitWlan));
  } else {
    // Handle incoming HTTP requests
    httpServer.handleClient();
    // updateBatteryVoltages();

    // Handle incoming messages from Arduino
    PT_SCHEDULE(handleIncomingMessage(&ptHandleIncomingMessage));
  }
}
