#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

WiFiUDP udp;
const char* udpAddress = "192.168.4.1";
const int udpPort = 1234;

const int button1Pin = 5;
const int button2Pin = 4;
const int button3Pin = 14;
const int button4Pin = 12;

// Variable to keep track of whether a button has been pressed
bool buttonPressed = false;
int currentNumber = 0;
unsigned long lastSendTime = 0; // To control the send interval
const unsigned long sendInterval = 100; // Interval for continuous sending (in milliseconds)

void setup() {
  Serial.begin(115200);
  pinMode(button1Pin, INPUT_PULLUP); // Use INPUT_PULLUP for button press detection
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);
  pinMode(button4Pin, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to ESP32 AP...");
  }
  Serial.println("Connected to ESP32 AP");
  udp.begin(udpPort);
}

void loop() {
  // Check if any button is pressed
  if (!buttonPressed) { // Only check if the flag hasn't been set yet
    if (digitalRead(button1Pin) == LOW || 
        digitalRead(button2Pin) == LOW || 
        digitalRead(button3Pin) == LOW || 
        digitalRead(button4Pin) == LOW) {
      buttonPressed = true; // Set the flag
      currentNumber = 1; // Set the number to 1 to start sending
      lastSendTime = 0; // Reset lastSendTime to send the number immediately
    }
  }

  // If the button was pressed at least once, continuously send '1'
  if (buttonPressed) {
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= sendInterval) {
      sendNumber(currentNumber);
      lastSendTime = currentTime;
    }
  }

  delay(10); // Reduce delay for faster response
}

void sendNumber(int number) {
  udp.beginPacket(udpAddress, udpPort);
  udp.print(number);
  udp.endPacket();
  Serial.print("Number sent to ESP32: ");
  Serial.println(number);
}