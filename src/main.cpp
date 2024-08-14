#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <pt.h>
#include <secrets.h>

static struct pt ptHandleIncomingMessage, ptInitWlan, ptPingPong;

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
bool showBatteryWarning = false;
// Task sleep times
const static unsigned int handleMessageWait = 4000;
const static unsigned int initWlanWait = 1000;

void handleRoot()
{
  String html = "<html>"
                "<head>"
                "<title>Robot Car Guard</title>"
                "<script>"
                "function refreshImage() {"
                "  var img = document.getElementById('cameraImage');"
                "  img.src = '/camera?' + new Date().getTime();"
                "}"
                "setInterval(refreshImage, 1000);" // Refresh the image every second
                "</script>"
                "</head>"
                "<body>"
                "<h1>Robot Car Guard</h1>"
                "<p>Current Mode: " +
                mode + "</p>"
                       "<p>"
                       "<button onclick=\"location.href='/mode/toy'\">Toy/Map Mode</button>"
                       "<button onclick=\"location.href='/mode/guard'\">Guard Mode</button>"
                       "</p>"
                       "<p>Motor Battery Voltage: " +
                String(motorBatteryVoltage) + " V</p>"
                                              "<p>Compute Battery Voltage: " +
                String(computeBatteryVoltage) + " V</p>"
                                                "<p><img id='cameraImage' src='/camera' width='320' height='240'></p>";

  if (showBatteryWarning)
  {
    html += "<p style='color:red'>Battery Voltage Low!</p>";
  }

  html += "</body>"
          "</html>";

  httpServer.send(200, "text/html", html);
}

void receivePicture()
{
  if (Serial.available())
  {
    String imageData = Serial.readStringUntil('\n');
    httpServer.send(200, "text/html", "<html><body><img src='data:image/jpeg;base64," + imageData + "'></body></html>"); // Send image data to client
  }
  else
  {
    httpServer.send(200, "text/html", "<html><body><p>Image data not available</p></body></html>");
  }
}

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

PT_THREAD(handleIncomingMessage(struct pt *pt))
{
  unsigned long startTime = millis(); // Initialize startTime
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
        // Do silly Arduino doesn't know weve already spoken of this...
        Serial.println("pong");
      }
      else
      {
        Serial.println("msg: Unknown command: " + message);
      }
    }
    PT_WAIT_UNTIL(pt, millis() - startTime >= handleMessageWait);
    // PT_YIELD_UNTIL(pt, millis() - startTime >= handleMessageWait);
    startTime = millis();
  }
  PT_END(pt);
}

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
  // Redirect back to the root page
  httpServer.sendHeader("Location", "/");
  httpServer.send(303);
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
      // Serial.print(".");
      //  PT_YIELD_UNTIL(pt, millis() - startTime >= 1000);
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

PT_THREAD(pingPong(struct pt *pt))
{
  PT_BEGIN(pt);
  unsigned int startTime = millis(); // Initialize startTime once at the beginning of the loop

  while (1)
  {
    if (Serial.available())
    {
      // Serial.println("serial available");
      String message = Serial.readStringUntil('\n');
      message.trim();
      if (message == "ping")
      {
        Serial.println("pong");
        Serial.println("pong");
        delay(2000);
        Serial.println("pong"); // Respond to ping with pong
        serialConnected = true;
        PT_EXIT(pt); // Exit the protothread
      }
    }
    startTime = millis();
    // PT_WAIT_UNTIL(pt, millis() - startTime >= 2500); // Wait 2500ms before looping again
    PT_YIELD_UNTIL(pt, millis() - startTime >= 1000);
    startTime = millis();
  }

  PT_END(pt);
}

void setup()
{
  Serial.begin(9600);
  delay(1000);
  PT_INIT(&ptPingPong);
  PT_INIT(&ptInitWlan);
  PT_INIT(&ptHandleIncomingMessage);
}

void loop()
{
  if (!serialConnected)
  {
    PT_SCHEDULE(pingPong(&ptPingPong));
  }
  else
  {
    if (!wlanConnected)
    {
      PT_SCHEDULE(initWlan(&ptInitWlan));
    }
    else
    {
      // Handle incoming HTTP requests
      httpServer.handleClient();
      // Handle incoming messages from Arduino
      PT_SCHEDULE(handleIncomingMessage(&ptHandleIncomingMessage));
      Serial.println("READY");
    }
  }
}
