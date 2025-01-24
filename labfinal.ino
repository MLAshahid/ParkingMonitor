/*
  Smart Parking Space Monitor with V-One Integration
*/
// Include V-One MQTT client
#include "VOneMqttClient.h"

// Undefine the macros from vonesetting.h
//#undef WIFI_SSID
//#undef WIFI_PASSWORD

// Define Wi-Fi credentials
//const char* WIFI_SSID = "40EdenHouse";    // Replace with your Wi-Fi SSID
//const char* WIFI_PASSWORD = "11801180";  // Replace with your Wi-Fi password

// Device IDs for V-One integration
#define DEVICEID_IR1 "77786695-f61c-45da-ad6d-511b9f3f929c"
#define DEVICEID_IR2 "419d7a55-36a2-4f5f-a208-510c8d7ea9af"
#define DEVICEID_SMOKE "f2fd095b-1515-48da-9aeb-5d23e435e128"
#define DEVICEID_LED1 "eb02be3c-6ab6-4663-a9a8-81e3cd4b18ed"
#define DEVICEID_LED2 "8c7a6f5f-bd48-4805-8e14-8e7553f2f7d1"
#define DEVICEID_LED3 "7e86e3e8-2cbb-42a4-abd8-37c9f42775e0"

// Pin configuration
const int irPin1 = 4;    // 1st IR sensor for Entry
const int irPin2 = 42;   // 2nd IR sensor for Exit
const int ledObstacle = 5; // Yellow LED for Obstacle
const int ledExit = 6;    // Red LED for Car Leaving
const int ledEntry = 9;   // Green LED for Car Entering
const int smokePin = 38;  // MQ2 smoke detector

const int maxParkingSpaces = 10; // Maximum parking spaces
int carCount = 0;               // Counter for cars in the parking lot

unsigned long debounceDelay = 600;
unsigned long obstacleClearDelay = 2000;
unsigned long lastObstacleClearTime = 0;

bool obstacleDetected = false;
bool ir1LastState = false;
bool ir2LastState = false;

// Variable for smoke detection
int lastSmokeLevel = 0; // Store the last smoke level
const int smokeThreshold = 5000; // Threshold for high smoke level

// V-One MQTT client
VOneMqttClient voneClient;

// Functions for LED indications
void indicateCarEntering() {
    digitalWrite(ledEntry, HIGH);
    delay(1000);
    digitalWrite(ledEntry, LOW);
    Serial.println("Car Entering: Green LED Blinked");
    voneClient.publishTelemetryData(DEVICEID_LED1, "Car Entering", 1);
}

void indicateCarLeaving() {
    digitalWrite(ledExit, HIGH);
    delay(1000);
    digitalWrite(ledExit, LOW);
    Serial.println("Car Leaving: Red LED Blinked");
    voneClient.publishTelemetryData(DEVICEID_LED2, "Car Leaving", 1);
}

void indicateObstacle() {
    if (!obstacleDetected) {
        digitalWrite(ledObstacle, HIGH);
        Serial.println("Obstacle Detected: Yellow LED ON");
        obstacleDetected = true;
        voneClient.publishTelemetryData(DEVICEID_LED3, "Obstacle", 1);
    }
}

void clearObstacleIndicator() {
    if (obstacleDetected) {
        unsigned long currentTime = millis();
        if (currentTime - lastObstacleClearTime >= obstacleClearDelay) {
            digitalWrite(ledObstacle, LOW);
            Serial.println("Obstacle Cleared: Yellow LED OFF");
            obstacleDetected = false;
            voneClient.publishTelemetryData(DEVICEID_LED3, "Obstacle", 0);
        }
    }
}

void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(9600);
    setup_wifi();
    voneClient.setup();

    pinMode(irPin1, INPUT_PULLUP);
    pinMode(irPin2, INPUT_PULLUP);
    pinMode(ledObstacle, OUTPUT);
    pinMode(ledExit, OUTPUT);
    pinMode(ledEntry, OUTPUT);
    pinMode(smokePin, INPUT);

    Serial.println("System Initialized");
    clearObstacleIndicator();
}

void loop() {
    if (!voneClient.connected()) {
        voneClient.reconnect();
    }
    voneClient.loop();

    unsigned long currentTime = millis();
    bool irVal1 = !digitalRead(irPin1); // Entry sensor
    bool irVal2 = !digitalRead(irPin2); // Exit sensor

    // Debouncing for IR sensors
    if (irVal1 != ir1LastState || irVal2 != ir2LastState) {
        delay(debounceDelay);
        irVal1 = !digitalRead(irPin1);
        irVal2 = !digitalRead(irPin2);
    }

    // Obstacle detection
    if (irVal1 && irVal2) {
        indicateObstacle();
    } else {
        clearObstacleIndicator();
    }

    // Handle entry and exit logic
    if (!obstacleDetected) {
        if (irVal1 && !ir1LastState) {
            if (carCount < maxParkingSpaces) {
                carCount++;
                Serial.print("Car Entered. Current Count: ");
                Serial.println(carCount);
                voneClient.publishTelemetryData(DEVICEID_IR1, "Car Count", carCount);
                indicateCarEntering();
            } else {
                Serial.println("Parking Full: Entry Restricted");
            }
        }

        if (irVal2 && !ir2LastState) {
            if (carCount > 0) {
                carCount--;
                Serial.print("Car Left. Current Count: ");
                Serial.println(carCount);
                voneClient.publishTelemetryData(DEVICEID_IR2, "Car Count", carCount);
                indicateCarLeaving();
            } else {
                Serial.println("Error: No cars to leave");
            }
        }
    }

    // Update last states
    ir1LastState = irVal1;
    ir2LastState = irVal2;

    // Smoke Detector Logic
    int smokeLevel = analogRead(smokePin);
    if (abs(smokeLevel - lastSmokeLevel) > 100) { // Significant change in smoke level
        Serial.print("Smoke Level: ");
        Serial.println(smokeLevel);

        if (smokeLevel > smokeThreshold) {
            Serial.println("Alert: High smoke level detected!");
            voneClient.publishTelemetryData(DEVICEID_SMOKE, "Smoke Alert", 1);
        } else {
            voneClient.publishTelemetryData(DEVICEID_SMOKE, "Smoke Alert", 0);
        }
    }

    lastSmokeLevel = smokeLevel;
    delay(1000);
}
