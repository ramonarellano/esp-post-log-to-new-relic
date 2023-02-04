/***************************************************
Example code for posting log entries to your free New Relic account.
The code posts JSON entries to a REST API on an HTTPS / TLS connection.
For information on the API and getting a free API-Key see:
https://docs.newrelic.com/docs/apis/rest-api-v2/get-started/introduction-new-relic-rest-api-v2/

You will need to change the following in this code:
1. Your wifi SSID and password
2. The API-KEY for New Relic in the url variable
Ramón Arellano 2023
****************************************************/

// Dependencies
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// Casa Solar Monitoring Configuration
const long timeInterval = 5000;  // Milliseconds between logging to New Relic

// WIFI ID and password
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";

// New Relic server information
const char* host = "log-api.eu.newrelic.com";
const int httpsPort = 443;
String url = "/log/v1?Api-Key=your_new_relic_api_key";

// SHA1 finger print of New Relic site certificate copied from web browser
const char fingerprint[] PROGMEM =
    "87 53 A0 9C 01 7C DC EC 65 88 4A 66 E9 3F 4F 75 A6 E0 87 5B";

ADC_MODE(ADC_VCC);      // Reading ESP operating voltage through analog to digital converter
#define analogPin A0    // ESP8266 Analog Pin ADC0 = A0

// Global variables
unsigned long lastTime = 0;
bool movementSinceLast = false;

// Setup serial connection
void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.println(
      "------------------------------------------------------------------");
  Serial.println("Starting up");
}

////////////////////////////////////////////////////////////////////
// Connect to WIFI
////////////////////////////////////////////////////////////////////
void connectToWifi() {
  WiFi.mode(
      WIFI_OFF);  // Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA);  // Only Station No AP, This line hides the viewing of
                        // ESP as wifi hotspot

  WiFi.begin(ssid, password);  // Connect to WiFi router
  Serial.println("");
  String logString = "Connecting to Wifi: ";
  Serial.println(logString + ssid + " with password: " + password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  // If connection successful show IP address in serial monitor
  Serial.print("Connected to Wifi: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  // IP address assigned to your ESP
  Serial.println(
      "------------------------------------------------------------------");
}

////////////////////////////////////////////////////////////////////
// Connect to New Relic Log API URL
////////////////////////////////////////////////////////////////////
WiFiClientSecure connectToNewRelic() {
  WiFiClientSecure client;  // Declare object of class WiFiClient

  // Serial.printf("Using fingerprint: '%s'\n", fingerprint);
  client.setFingerprint(fingerprint);
  client.setTimeout(15000);  // 15 Seconds
  delay(1000);

  // If connection to New Relic fails after retries stop trying, in case WIFI
  // has been disconnected
  int retries = 0;
  while ((!client.connect(host, httpsPort)) && (retries < 10)) {
    delay(100);
    Serial.print(".");
    retries++;
  }

  if (retries == 10) {
    Serial.println("Connection to New Relic failed");
  } else {
    Serial.printf("Connected to New Relic host: '%s'\n", host);
  }

  return client;
}

////////////////////////////////////////////////////////////////////
// Create the JSON data package to send to New Relic
////////////////////////////////////////////////////////////////////
String buildJsonData() {
  // Get average voltage from ADC of 100 samplings with 10 ms interval
  long voltage = 0;
  for (int i = 0; i < 100; i++) {
    voltage += analogRead(analogPin);
    delay(10);
  }
  voltage = voltage / 100;

  // Create JSON
  const int capacity = JSON_OBJECT_SIZE(3);
  StaticJsonDocument<capacity> doc;
  JsonObject message = doc.createNestedObject("message");
  message["application"] = "myApplication";
  message["voltageLevel"] = voltage;
  String jsonString;
  serializeJson(doc, jsonString);

  return jsonString;
}

////////////////////////////////////////////////////////////////////
// Post JSON data to New Relic Log API
////////////////////////////////////////////////////////////////////
void postJSONtoNewRelic(WiFiClientSecure client, String jsonData) {
  Serial.print("Requesting URL: ");
  Serial.println(host + url);
  Serial.print("Posting JSON to New Relic: ");
  Serial.println(jsonData);
  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + (String)host);
  client.println("User-Agent: ESP8266/1.0");
  client.println("Connection: close");
  client.println("Content-Type: application/json;");
  client.print("Content-Length: ");
  client.println(jsonData.length());
  client.println();
  client.println(jsonData);

  Serial.print("Request sent - ");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }

  Serial.print("Reply from New Relic: ");
  String line;
  while (client.available()) {
    line = client.readStringUntil('\n');  // Read Line by Line
    Serial.println(line);                 // Print response
  }
  Serial.println("Closing connection");
  Serial.println(
      "------------------------------------------------------------------");
}

////////////////////////////////////////////////////////////////////
// Main program loop
////////////////////////////////////////////////////////////////////
void loop() {
  // Wait between each loop
  delay(1000);

  // Debug code showing sensor values
  while (false) {
    Serial.print("Analogpin: ");
    Serial.print(analogRead(analogPin));
    Serial.print(", ");
    Serial.println(millis());
    delay(100);
  }


  // Send an HTTP POST request every 10 minutes
  if ((millis() - lastTime) > timeInterval) {
    // Reset timer
    lastTime = millis();

    // Connect to wifi if not connected
    while (WiFi.status() != WL_CONNECTED) {
      connectToWifi();
    }

    // Connect to New Relic site
    WiFiClientSecure client = connectToNewRelic();

    // Create JSON with sensor values
    String jsonData = buildJsonData();

    // Post JSON and disconnect
    postJSONtoNewRelic(client, jsonData);
  }
}
