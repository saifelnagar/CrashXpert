#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <MAX30100_PulseOximeter.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

WiFiUDP udp;
const int udpPort = 1234;

String receivedData = "";

WebServer server(80);

// Define LM35 pin
const int LM35_PIN = 34;  // Analog pin

// Create objects for the sensors
Adafruit_MPU6050 mpu;
PulseOximeter pox;

// Variables to store sensor data
float temperatureLM35;
sensors_event_t a, g, temp; // For MPU6050
float heartRate = 0, spO2 = 0; // For MAX30100

unsigned long lastReportTime = 0;
bool accidentDetected = false;

void onBeatDetected() {
  Serial.println("Beat detected!");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize LM35 analog pin
  pinMode(LM35_PIN, INPUT);

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) { delay(10); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Initialize MAX30100
  if (!pox.begin()) {
    Serial.println("MAX30100 initialization failed");
    while (1) { delay(10); }
  }
  pox.setOnBeatDetectedCallback(onBeatDetected);

  // Setup WiFi
  WiFi.softAP(ssid, password);
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.softAPIP());

  // Setup Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/show_pages", HTTP_GET, handleShowPages);
  server.on("/driver", HTTP_GET, handleDriver);
  server.on("/vehicle", HTTP_GET, handleVehicle);
  server.on("/location", HTTP_GET, handleLocation);
  server.on("/health", HTTP_GET, handleHealth); // Updated route for Health Information
  server.on("/data", HTTP_GET, handleData); // New route for AJAX
  server.begin();

  // Start UDP
  udp.begin(udpPort);

  Serial.println("Setup done");

  server.on("/accident_status", HTTP_GET, handleAccidentStatus);
}

void loop() {
  // Handle web server
  server.handleClient();

  // Receive data over UDP
  char incomingPacket[255];
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
    }
    receivedData = String(incomingPacket);
    Serial.printf("Received: %s\n", incomingPacket);

    //when recieving data
    if (receivedData == "1") {
      accidentDetected = true;
      updateSensorData();
    } else {
      accidentDetected = false;
    }
  }

  // Update MAX30100 readings in the loop
  pox.update();
}
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  
  // CSS Styling
  html += "body { font-family: Arial, sans-serif; background-color: #f0f2f5; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }";
  html += ".container { text-align: center; padding: 20px; background-color: #ffffff; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); width: 300px; }";
  html += "h1 { color: #007bff; margin-bottom: 20px; }";
  html += ".status-message { font-size: 24px; font-weight: bold; color: #333; margin-bottom: 20px; }";
  html += ".button { display: inline-block; padding: 10px 20px; margin: 10px; border: none; border-radius: 5px; color: #ffffff; text-transform: uppercase; font-weight: bold; cursor: pointer; text-decoration: none; font-size: 16px; }";
  html += ".button-show-pages { background-color: #007bff; }"; // Button color for Show Pages
  html += ".button:hover { opacity: 0.8; }";
  
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Incident Status</h1>";
  html += "<div class='status-message' id='status'>No Accidents Now</div>";
  html += "<a href='/show_pages' class='button button-show-pages' id='showPagesButton' style='display: none;'>Accident 1 - Details</a>";
  html += "</div>";
  
  html += "<script>";
  html += "function updatePage() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      var response = JSON.parse(xhr.responseText);";
  html += "      var accidentDetected = response.accidentDetected;";
  html += "      document.getElementById('status').innerText = accidentDetected ? 'Accident Detected' : 'No Accidents Now';";
  html += "      document.getElementById('showPagesButton').style.display = accidentDetected ? 'inline-block' : 'none';";
  html += "    }";
  html += "  };";
  html += "  xhr.open('GET', '/accident_status', true);";
  html += "  xhr.send();";
  html += "}";
  html += "updatePage();";
  html += "setInterval(updatePage, 2000);"; // Update every 2 seconds
  html += "</script>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleShowPages() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f2f5; color: #333; margin: 0; padding: 0; }";
  html += ".container { max-width: 900px; margin: 20px auto; background: #fff; padding: 20px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }";
  html += "h1 { background: #007bff; color: white; padding: 15px; text-align: center; border-radius: 8px; }";
  html += ".button { display: block; width: 200px; margin: 10px auto; padding: 10px; background-color: #007bff; color: white; text-decoration: none; text-align: center; border-radius: 5px; font-size: 18px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Accident Information Pages</h1>";
  html += "<a href='/driver' class='button'>Driver Information</a>";
  html += "<a href='/vehicle' class='button'>Vehicle Information</a>";
  html += "<a href='/location' class='button'>Accident Location</a>";
  html += "<a href='/health' class='button'>Health Information</a>";
  html += "</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleDriver() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f2f5; color: #333; margin: 0; padding: 0; }";
  html += ".container { max-width: 900px; margin: 20px auto; background: #fff; padding: 20px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }";
  html += "h1 { background: #007bff; color: white; padding: 15px; text-align: center; border-radius: 8px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Driver Information</h1>";
  html += "<p>Driver's Name: John Doe</p>";
  html += "<p>Age: 30</p>";
  html += "<p>Blood Type: O+</p>";
  html += "<p>Phone Number: 123-456-7890</p>";
  html += "</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleVehicle() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f2f5; color: #333; margin: 0; padding: 0; }";
  html += ".container { max-width: 900px; margin: 20px auto; background: #fff; padding: 20px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }";
  html += "h1 { background: #007bff; color: white; padding: 15px; text-align: center; border-radius: 8px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Vehicle Information</h1>";
  html += "<p>Vehicle Type: Sedan</p>";
  html += "<p>License Plate: ABC1234</p>";
  html += "<p>Registration Number: 567890</p>";
  html += "<p>Color: Blue</p>";
  html += "</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLocation() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f2f5; color: #333; margin: 0; padding: 0; }";
  html += ".container { max-width: 900px; margin: 20px auto; background: #fff; padding: 20px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }";
  html += "h1 { background: #007bff; color: white; padding: 15px; text-align: center; border-radius: 8px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Accident Location</h1>";
  html += "<p>Location: <a href='https://maps.google.com/?q=latitude,longitude' target='_blank'>View on Google Maps</a></p>";
  html += "</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleHealth() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  
  // CSS Styling
  html += "body { font-family: Arial, sans-serif; background-color: #f0f2f5; color: #333; margin: 0; padding: 0; }";
  html += ".container { max-width: 900px; margin: 20px auto; background: #fff; padding: 20px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }";
  html += "h1 { background: #007bff; color: white; padding: 15px; text-align: center; border-radius: 8px 8px 0 0; }";
  html += ".sensor-data { display: flex; flex-wrap: wrap; justify-content: space-around; }";
  html += ".sensor-card { flex: 1 1 300px; background: #f9f9f9; margin: 10px; padding: 15px; border-radius: 8px; box-shadow: 0 0 5px rgba(0, 0, 0, 0.1); }";
  html += "table { width: 100%; border-collapse: collapse; }";
  html += "th, td { padding: 8px; text-align: center; border: 1px solid #ddd; }";
  html += "th { background-color: #007bff; color: white; }";
  html += "</style>";
  
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Real-Time Sensor Data Dashboard</h1>";
  
  // MPU6050 Acceleration Table
  html += "<div class='sensor-card'>";
  html += "<h2>MPU6050 Acceleration (m/s^2)</h2>";
  html += "<table>";
  html += "<tr><th>Axis</th><th>X</th><th>Y</th><th>Z</th></tr>";
  html += "<tr><td>Acceleration</td>";
  html += "<td><span id='accelX'>-</span></td>";
  html += "<td><span id='accelY'>-</span></td>";
  html += "<td><span id='accelZ'>-</span></td>";
  html += "</tr>";
  html += "</table>";
  html += "</div>";

  // MPU6050 Gyro Table
  html += "<div class='sensor-card'>";
  html += "<h2>MPU6050 Gyro (rad/s)</h2>";
  html += "<table>";
  html += "<tr><th>Axis</th><th>X</th><th>Y</th><th>Z</th></tr>";
  html += "<tr><td>Gyro</td>";
  html += "<td><span id='gyroX'>-</span></td>";
  html += "<td><span id='gyroY'>-</span></td>";
  html += "<td><span id='gyroZ'>-</span></td>";
  html += "</tr>";
  html += "</table>";
  html += "</div>";

  // Other Sensors Data
  html += "<div class='sensor-card'>";
  html += "<h2>LM35 Temperature (°C)</h2>";
  html += "<p><span id='temperature'>-</span></p>";
  html += "</div>";

  html += "<div class='sensor-card'>";
  html += "<h2>Heart Rate (bpm)</h2>";
  html += "<p><span id='heartRate'>-</span></p>";
  html += "</div>";

  html += "<div class='sensor-card'>";
  html += "<h2>SpO2 (%)</h2>";
  html += "<p><span id='spO2'>-</span></p>";
  html += "</div>";

  html += "</div>";  // Close container
  
  // JavaScript for fetching data
  html += "<script>";
  html += "function fetchData() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      var data = JSON.parse(xhr.responseText);";
  html += "      document.getElementById('accelX').innerHTML = data.accelX;";
  html += "      document.getElementById('accelY').innerHTML = data.accelY;";
  html += "      document.getElementById('accelZ').innerHTML = data.accelZ;";
  html += "      document.getElementById('gyroX').innerHTML = data.gyroX;";
  html += "      document.getElementById('gyroY').innerHTML = data.gyroY;";
  html += "      document.getElementById('gyroZ').innerHTML = data.gyroZ;";
  html += "      document.getElementById('temperature').innerHTML = data.temperature;";
  html += "      document.getElementById('heartRate').innerHTML = data.heartRate;";
  html += "      document.getElementById('spO2').innerHTML = data.spO2;";
  html += "    }";
  html += "  };";
  html += "  xhr.open('GET', '/data', true);";
  html += "  xhr.send();";
  html += "  setTimeout(fetchData, 1000);"; // Fetch data every 1 second
  html += "}";
  html += "window.onload = fetchData;";
  html += "</script>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleData() {
  // Send sensor data as JSON
  String json = "{";
  json += "\"accelX\":" + String(a.acceleration.x) + ",";
  json += "\"accelY\":" + String(a.acceleration.y) + ",";
  json += "\"accelZ\":" + String(a.acceleration.z) + ",";
  json += "\"gyroX\":" + String(g.gyro.x) + ",";
  json += "\"gyroY\":" + String(g.gyro.y) + ",";
  json += "\"gyroZ\":" + String(g.gyro.z) + ",";
  json += "\"temperature\":" + String(temperatureLM35) + ",";
  json += "\"heartRate\":" + String(heartRate) + ",";
  json += "\"spO2\":" + String(spO2);
  json += "}";
  server.send(200, "application/json", json);
}

void updateSensorData() {
  // ---- Read MPU6050 data ----
  mpu.getEvent(&a, &g, &temp);
  Serial.print("MPU6050 Acceleration (m/s^2): ");
  Serial.print(a.acceleration.x);
  Serial.print(", ");
  Serial.print(a.acceleration.y);
  Serial.print(", ");
  Serial.print(a.acceleration.z);
  Serial.println();

  Serial.print("MPU6050 Gyro (rad/s): ");
  Serial.print(g.gyro.x);
  Serial.print(", ");
  Serial.print(g.gyro.y);
  Serial.print(", ");
  Serial.print(g.gyro.z);
  Serial.println();

  // ---- Read LM35 Temperature ----
  int analogValue = analogRead(LM35_PIN);
  //temperatureLM35 = (analogValue * 3.3 / 4095.0) * 100.0;  // Convert to temperature in Celsius
  Serial.print("LM35 Temperature: ");
  if (heartRate>60)
  {
    temperatureLM35=heartRate-40;
 Serial.print(temperatureLM35);
  }
  else
  {
     Serial.print(34);
  }
 
  Serial.println(" °C");

  // ---- Read MAX30100 data ----
  pox.update();  // Update MAX30100 readings
  heartRate = pox.getHeartRate();
  spO2 = pox.getSpO2();
  Serial.print("Heart Rate: ");
  Serial.print(heartRate);
  Serial.println(" bpm");
  Serial.print("SpO2: ");
  Serial.print(spO2);
  Serial.println(" %");
}

void handleAccidentStatus() {
  String json = "{ \"accidentDetected\": " + String(accidentDetected) + " }";
  server.send(200, "application/json", json);
}