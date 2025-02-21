#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Pin definitions for Nano ESP32
#define SS_PIN   D5   // GPIO5
#define RST_PIN  D4   // GPIO4
#define LED_PIN  D2   // Onboard LED (Optional)

// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Debug flag
#define DEBUG true

// Global variables for session data
String currentUserId = "";
String currentSessionId = "";
String sensorColor = "";
String sensorStyle = "";

// Timer variables
unsigned long lastRequestTime = 0;
const long requestInterval = 2000; // 2 seconds

void displayData(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);    // Initialize serial communications
    while (!Serial && millis() < 3000) {
        ; // Wait for serial port to connect, timeout after 3 seconds
    }

    if (DEBUG) Serial.println(F("Initializing system..."));

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Initialize SPI bus with custom pin mappings
    SPI.begin(D8, D9, D10, D5);  // SCK, MISO, MOSI, SS (match wiring)

    mfrc522.PCD_Init();  // Init MFRC522

    // Connect to WiFi
    Serial.println("Connecting to WiFi...");
    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");

    // Read and print RFID module software version
    byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    if (DEBUG) {
        Serial.print(F("Software Version: 0x"));
        Serial.print(v, HEX);
        Serial.println();
    }

    if (DEBUG) Serial.println(F("System ready - Waiting for cards..."));
}

void loop() {
    static unsigned long lastDebugTime = 0;
    const unsigned long DEBUG_INTERVAL = 5000; // Debug message every 5 seconds
    
    // Periodic debug message
    if (DEBUG && (millis() - lastDebugTime > DEBUG_INTERVAL)) {
        Serial.println(F("Scanning for cards..."));
        lastDebugTime = millis();
    }

    // Look for new cards
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
        if (DEBUG) Serial.println(F("Error: Failed to read card serial"));
        return;
    }

    // Visual feedback
    digitalWrite(LED_PIN, HIGH);

    // Convert UID to string
    String rfidTag = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) rfidTag += "0";
        rfidTag += String(mfrc522.uid.uidByte[i], HEX);
    }
    rfidTag.toUpperCase();

    if (DEBUG) {
        Serial.println(F("\n=== Card Detected! ==="));
        Serial.print(F("Card UID: "));
        Serial.println(rfidTag);
    }

    // Process the RFID card
    processRFIDCard(rfidTag);

    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    
    digitalWrite(LED_PIN, LOW);
    
    if (DEBUG) Serial.println(F("=== End of Card Reading ===\n"));
}

String getUserIdFromRFID(String rfidTag) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.reconnect();
        return "";
    }

    HTTPClient http;
    String requestURL = String(SUPABASE_BASE_URL) + "/bracelets?rfid=eq." + rfidTag + "&select=user_id";
    if (DEBUG) Serial.println("\n[INFO] Querying bracelets table: " + requestURL);

    http.begin(requestURL);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String response = http.getString();
        if (DEBUG) Serial.println("[INFO] Bracelets Response: " + response);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (!error && doc.size() > 0) {
            String userId = doc[0]["user_id"];
            if (DEBUG) Serial.println("[SUCCESS] Found user_id: " + userId);
            http.end();
            return userId;
        } else {
            Serial.println("[ERROR] No bracelet found with RFID: " + rfidTag);
        }
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
    return "";
}

bool getSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.reconnect();
        return false;
    }

    HTTPClient http;
    String requestURL = String(SUPABASE_BASE_URL) + "/sensors?id=eq.1&select=color,style"; // id=eq.1 variable that has to be set for each sensor.
    if (DEBUG) Serial.println("\n[INFO] Querying sensors table: " + requestURL);

    http.begin(requestURL);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String response = http.getString();
        if (DEBUG) Serial.println("[INFO] Sensors Response: " + response);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (!error && doc.size() > 0) {
            sensorColor = doc[0]["color"].as<String>();
            sensorStyle = doc[0]["style"].as<String>();
            if (DEBUG) Serial.println("[SUCCESS] Found sensor data - Color: " + sensorColor + ", Style: " + sensorStyle);
            http.end();
            return true;
        } else {
            Serial.println("[ERROR] No sensor data found with id=1");
        }
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
    return false;
}

String getLatestSessionId(String userId) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.reconnect();
        return "";
    }

    HTTPClient http;
    String requestURL = String(SUPABASE_BASE_URL) + "/climbing_sessions?user_id=eq." + userId + "&order=created_at.desc&limit=1";
    if (DEBUG) Serial.println("\n[INFO] Querying climbing_sessions table: " + requestURL);

    http.begin(requestURL);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String response = http.getString();
        if (DEBUG) Serial.println("[INFO] Climbing Sessions Response: " + response);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (!error && doc.size() > 0) {
            String sessionId = doc[0]["id"];
            if (DEBUG) Serial.println("[SUCCESS] Found session_id: " + sessionId);
            http.end();
            return sessionId;
        } else {
            Serial.println("[ERROR] No active session found for user_id: " + userId);
        }
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
    return "";
}

bool createClimbingRoute() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.reconnect();
        return false;
    }

    HTTPClient http;
    String requestURL = String(SUPABASE_BASE_URL) + "/climbing_routes";
    if (DEBUG) Serial.println("\n[INFO] Creating climbing route entry: " + requestURL);

    http.begin(requestURL);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=minimal"); // Don't return the created record

    // Create JSON payload
    DynamicJsonDocument doc(1024);
    doc["color"] = sensorColor;
    doc["style"] = sensorStyle;
    doc["user_id"] = currentUserId;
    doc["session_id"] = currentSessionId;

    String jsonString;
    serializeJson(doc, jsonString);

    if (DEBUG) Serial.println("[INFO] Request payload: " + jsonString);

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode >= 200 && httpResponseCode < 300) {
        if (DEBUG) Serial.println("[SUCCESS] Created climbing route entry");
        http.end();
        return true;
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
        Serial.println("Response: " + http.getString());
    }

    http.end();
    return false;
}

void processRFIDCard(String rfidTag) {
    // Step 1: Get user_id from RFID
    currentUserId = getUserIdFromRFID(rfidTag);
    if (currentUserId.length() == 0) {
        Serial.println("[ERROR] Failed to get user_id from RFID");
        return;
    }

    // Step 2: Get sensor data
    if (!getSensorData()) {
        Serial.println("[ERROR] Failed to get sensor data");
        return;
    }

    // Step 3: Get latest session_id
    currentSessionId = getLatestSessionId(currentUserId);
    if (currentSessionId.length() == 0) {
        Serial.println("[ERROR] Failed to get session_id");
        return;
    }

    // Step 4: Create climbing route entry
    if (!createClimbingRoute()) {
        Serial.println("[ERROR] Failed to create climbing route entry");
        return;
    }

    if (DEBUG) {
        Serial.println("\n=== Successfully processed RFID card ===");
        Serial.println("User ID: " + currentUserId);
        Serial.println("Session ID: " + currentSessionId);
        Serial.println("Sensor Color: " + sensorColor);
        Serial.println("Sensor Style: " + sensorStyle);
        Serial.println("Climbing route entry created successfully");
        Serial.println("========================================\n");
    }
}
