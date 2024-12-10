#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Wi-Fi configuration
const char* ssid = "Thanh Ngoc 5G";   // Replace with your Wi-Fi SSID
const char* password = "11111111";    // Replace with your Wi-Fi password

// Raspberry Pi server address
const char* serverName = "http://192.168.100.208:8080/data"; // Replace with your Raspberry Pi IP address

// DHT11 Sensor configuration
#define DHTPIN 4         // GPIO pin for DHT11 sensor
#define DHTTYPE DHT11    // Define sensor type as DHT11
DHT dht(DHTPIN, DHTTYPE);

// MQ135 Sensor configuration (Air Quality)
#define MQ135PIN 34      // GPIO pin for MQ135 sensor

// Water Level Sensor configuration
#define WATER_SENSOR_PIN 33 // GPIO pin for water level sensor

// Web server instance on port 80
WebServer server(80);

// GPIO Pin for control (e.g., LED or relay)
#define CONTROL_PIN 22

// Global variable to track control state and time of last HTTP command
bool controlState = false;  // Initially, set the state to OFF
unsigned long lastControlTime = 0;  // Last time the control was updated (in ms)
const unsigned long CONTROL_TIMEOUT = 5000;  // Timeout for 5 seconds (5 * 1000 ms)

// Function declarations
void connectWiFi();
void handleRoot();
void handleControl();
void sendDataToServer(float temperature, float humidity, int airQuality, int waterLevel);
void handleWaterLevel(int waterLevel);
void checkAutoTurnOff();

void setup() {
  Serial.begin(115200);

  // Initialize DHT11 sensor
  dht.begin();

  // Connect to Wi-Fi
  connectWiFi();

  // Initialize GPIO pin for control
  pinMode(CONTROL_PIN, OUTPUT);
  digitalWrite(CONTROL_PIN, LOW); // Initially set the pin LOW

  // Configure HTTP server routes
  server.on("/", HTTP_GET, handleRoot);       // Route for root URL ("/")
  server.on("/control", HTTP_GET, handleControl);  // Route for control URL

  // Start the HTTP server
  server.begin();
  Serial.println("HTTP Server started");
}

void loop() {
  server.handleClient(); // Handle incoming HTTP requests

  // Check if we need to automatically turn off GPIO D22 after 5 seconds of inactivity
  checkAutoTurnOff();

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Send HTTP POST request to server
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    // Read data from DHT11 sensor
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Read value from MQ135 (Air Quality)
    int airQuality = analogRead(MQ135PIN); // Raw value from the sensor

    // Read water level sensor state (But won't be used to control GPIO anymore)
    int waterLevel = analogRead(WATER_SENSOR_PIN); // Get analog value of the water level sensor

    // Check if the values are valid
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Handle water level and control GPIO D22 accordingly
    handleWaterLevel(waterLevel);

    // Send data to the server (temperature, humidity, air quality, water level)
    sendDataToServer(temperature, humidity, airQuality, waterLevel);
  } else {
    Serial.println("WiFi Disconnected");
  }

  delay(1000); // Send data every 1 second
}

// Handle the root route ("/")
void handleRoot() {
  String message = "ESP32 is running!";
  server.send(200, "text/plain", message); // Send response back to the client
}

// Handle the /control route to control GPIO pin
void handleControl() {
  String state = server.arg("state");  // Get the state parameter from the URL

  if (state == "on") {
    if (!controlState) { // Only turn on if it was previously off
      controlState = true;  // Set controlState to ON
      digitalWrite(CONTROL_PIN, HIGH);  // Set GPIO pin HIGH (e.g., turn on LED or relay)
      lastControlTime = millis(); // Reset the timeout timer
      server.send(200, "text/plain", "GPIO22 is ON");
    } else {
      server.send(200, "text/plain", "GPIO22 is already ON");
    }
  } else if (state == "off") {
    if (controlState) {  // Only turn off if it was previously on
      controlState = false;   // Set controlState to OFF
      digitalWrite(CONTROL_PIN, LOW);   // Set GPIO pin LOW (e.g., turn off LED or relay)
      lastControlTime = millis(); // Reset the timeout timer
      server.send(200, "text/plain", "GPIO22 is OFF");
    } else {
      server.send(200, "text/plain", "GPIO22 is already OFF");
    }
  } else {
    server.send(400, "text/plain", "Invalid state parameter. Use 'on' or 'off'.");
  }
}

// Send data to server (via HTTP POST)
void sendDataToServer(float temperature, float humidity, int airQuality, int waterLevel) {
  HTTPClient http;
  DynamicJsonDocument doc(1024);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["airQuality"] = airQuality;
  doc["waterLevel"] = waterLevel;

  String jsonData;
  serializeJson(doc, jsonData);

  // Send data as JSON
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonData);

  // Check the response code
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    Serial.println("Server Response: " + response);
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }

  http.end(); // End the HTTP request
}

// Connect to Wi-Fi
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.println("IP Address: " + WiFi.localIP().toString());
}

// Handle water level and control GPIO D22 accordingly
void handleWaterLevel(int waterLevel) {
  if (controlState) {
    // If controlState is ON, keep GPIO D22 ON regardless of water level
    digitalWrite(CONTROL_PIN, HIGH);
  } else {
    // If controlState is OFF, check water level
    if (waterLevel > 1000) {
      // If water level is high, turn on GPIO D22
      digitalWrite(CONTROL_PIN, HIGH);
      Serial.println("Water level is high! Turning ON GPIO D22.");
    } else {
      // Otherwise, turn off GPIO D22
      digitalWrite(CONTROL_PIN, LOW);
      Serial.println("Water level is low. Turning OFF GPIO D22.");
    }
  }
}

// Check if GPIO D22 should be automatically turned off after 5 seconds of no "off" signal from web
void checkAutoTurnOff() {
  // If 5 seconds have passed since the last "off" command, and GPIO is ON, turn it off automatically
  if (controlState == true && (millis() - lastControlTime) > CONTROL_TIMEOUT) {
    controlState = false; // Set controlState to OFF
    digitalWrite(CONTROL_PIN, LOW); // Turn off GPIO D22
    Serial.println("No 'off' signal for 5 seconds. Automatically turning off GPIO D22.");
  }
}
