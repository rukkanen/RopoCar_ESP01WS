#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <pt.h>
#include "secrets.h"
#include "YA_OV7670_camera.h"

// Web server on port 80
ESP8266WebServer httpServer(80);

// State variables
String mode = "guard";        // Current mode of the robot car
bool wlanConnected = false;   // WiFi connection status
bool readyNotified = false;   // Whether ESP is ready
bool serialConnected = false; // Serial communication status

// Battery voltage placeholders
float motorBatteryVoltage = 0.0;
float computeBatteryVoltage = 0.0;
bool showBatteryWarning = false;

// Task sleep times (in milliseconds)
const static unsigned int handleMessageWait = 4000;
const static unsigned int initWlanWait = 1000;

// Protothreads for multitasking
static struct pt ptHandleIncomingMessage, ptInitWlan, ptPingPong, ptCameraCapture;

// OV7670 Camera configuration (defined in YA_OV7670_camera.h)
// YA_OV7670_camera<OV7670> camera;

void handleRoot()
{
  String html = "<html>"
                "<head><title>Robot Car Guard</title>"
                "<script>function refreshImage() {"
                "var img = document.getElementById('cameraImage');"
                "img.src = '/camera?' + new Date().getTime();"
                "}setInterval(refreshImage, 1000);</script></head>"
                "<body><h1>Robot Car Guard</h1>"
                "<p>Current Mode: " +
                mode + "</p>"
                       "<p><button onclick=\"location.href='/mode/toy'\">Toy/Map Mode</button>"
                       "<button onclick=\"location.href='/mode/guard'\">Guard Mode</button></p>"
                       "<p>Motor Battery Voltage: " +
                String(motorBatteryVoltage) + " V</p>"
                                              "<p>Compute Battery Voltage: " +
                String(computeBatteryVoltage) + " V</p>"
                                                "<p><img id='cameraImage' src='/camera' width='320' height='240'></p>";

  if (showBatteryWarning)
  {
    html += "<p style='color:red'>Battery Voltage Low!</p>";
  }
  html += "</body></html>";
  httpServer.send(200, "text/html", html);
}

// Mode switching based on HTTP request
void handleModeChange()
{
  String newMode;
  if (httpServer.uri() == "/mode/toy")
  {
    newMode = "toy";
  }
  else if (httpServer.uri() == "/mode/guard")
  {
    newMode = "guard";
  }

  if (newMode != mode)
  {
    mode = newMode;
    Serial.println("mode_change:" + mode);
  }
  httpServer.sendHeader("Location", "/");
  httpServer.send(303);
}

// Receive and display a picture from the camera
void receivePicture()
{
  if (Serial.available())
  {
    String imageData = Serial.readStringUntil('\n');
    httpServer.send(200, "text/html", "<html><body><img src='data:image/jpeg;base64," + imageData + "'></body></html>");
  }
  else
  {
    httpServer.send(200, "text/html", "<html><body><p>Image data not available</p></body></html>");
  }
}

// Handle incoming battery voltage data
void receiveBatteryVoltages(String message)
{
  int commaIndex = message.indexOf(',');
  motorBatteryVoltage = message.substring(0, commaIndex).toFloat();
  computeBatteryVoltage = message.substring(commaIndex + 1).toFloat();
  if (motorBatteryVoltage < 6.0 || computeBatteryVoltage < 6.0)
  {
    showBatteryWarning = true;
  }
}

// Handle incoming messages from Arduino
PT_THREAD(handleIncomingMessage(struct pt *pt))
{
  unsigned long startTime = millis();
  PT_BEGIN(pt);

  while (1)
  {
    if (Serial.available())
    {
      String message = Serial.readStringUntil('\n');
      message.trim();
      if (message.startsWith("battery_level="))
      {
        receiveBatteryVoltages(message);
      }
      else if (message == "picture_start")
      {
        Serial.println("Got command: picture_start");
        receivePicture();
      }
      else if (message == "ping")
      {
        Serial.println("pong");
      }
      else
      {
        Serial.println("msg: Unknown command: " + message);
      }
    }
    PT_YIELD_UNTIL(pt, millis() - startTime >= handleMessageWait);
    startTime = millis();
  }
  PT_END(pt);
}

// setup the httpServer routes
void initHttpServer()
{
  Serial.println("initHttp:start");
  httpServer.on("/", handleRoot);
  httpServer.on("/mode/toy", handleModeChange);
  httpServer.on("/mode/guard", handleModeChange);
  httpServer.begin();
  Serial.println("initHttp:ok");
}

// Handle WiFi initialization
PT_THREAD(initWlan(struct pt *pt))
{
  PT_BEGIN(pt);
  unsigned int startTime = millis();

  WiFi.begin(ssid, password);
  Serial.println("initWlan:start");
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      PT_WAIT_UNTIL(pt, millis() - startTime >= initWlanWait);
      startTime = millis();
    }
    else
    {
      wlanConnected = true;
      initHttpServer();
      Serial.println("initWlan:ok");
      Serial.println("msg: WLAN connected: IP address: " + WiFi.localIP().toString());
      PT_EXIT(pt);
    }
  }
  PT_END(pt);
}

// Ping-Pong for serial communication
PT_THREAD(pingPong(struct pt *pt))
{
  PT_BEGIN(pt);
  unsigned int startTime = millis();

  while (1)
  {
    if (Serial.available())
    {
      String message = Serial.readStringUntil('\n');
      message.trim();
      if (message == "ping")
      {
        Serial.println("pong");
        serialConnected = true;
        PT_EXIT(pt);
      }
    }
    PT_WAIT_UNTIL(pt, millis() - startTime >= 1000);
    startTime = millis();
  }
  PT_END(pt);
}

// Handle camera capture and transmission
PT_THREAD(cameraCapture(struct pt *pt))
{
  PT_BEGIN(pt);
  // Camera capture logic here (use YA_OV7670_camera library)
  PT_END(pt);
}

// Setup function to initialize components
void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("Booting up...");

  PT_INIT(&ptPingPong);
  PT_INIT(&ptInitWlan);
  PT_INIT(&ptHandleIncomingMessage);
  PT_INIT(&ptCameraCapture);
}

// Main loop handling tasks and scheduling protothreads
void loop()
{
  if (!serialConnected)
  {
    PT_SCHEDULE(pingPong(&ptPingPong));
  }
  else
  {
    Serial.println("PingPOng done");
    if (!wlanConnected)
    {
      PT_SCHEDULE(initWlan(&ptInitWlan));
    }
    else
    {
      httpServer.handleClient();
      PT_SCHEDULE(handleIncomingMessage(&ptHandleIncomingMessage));
      PT_SCHEDULE(cameraCapture(&ptCameraCapture));
      if (!readyNotified)
      {
        Serial.println("READY");
        readyNotified = true;
      }
    }
  }
}
