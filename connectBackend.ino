#include <WiFi.h>            // For ESP32 WiFi
#include <HTTPClient.h>      // For HTTP requests
#include <ArduinoJson.h>     // To parse JSON
#include "secrets.h"         // Contains sensitive credentials

String USER_ID = "";  // Will be set after querying the bracelets table
const char* RFID = "RFID";  // Replace with actual RFID value

// Global variables for sensor data
String sensorColor = "";
String sensorStyle = "";

// Timer variables
unsigned long lastRequestTime = 0;
const long requestInterval = 2000; // 2 seconds

void getSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.reconnect();
        return;
    }

    HTTPClient http;
    String requestURL = String(SUPABASE_BASE_URL) + "/sensors?id=eq.1&select=color,style";
    Serial.println("\n[INFO] Querying sensors table: " + requestURL);

    http.begin(requestURL);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("[INFO] Sensors Response: " + response);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (!error && doc.size() > 0) {
            sensorColor = doc[0]["color"].as<String>();
            sensorStyle = doc[0]["style"].as<String>();
            Serial.println("[SUCCESS] Found sensor data - Color: " + sensorColor + ", Style: " + sensorStyle);
        } else {
            Serial.println("[ERROR] No sensor data found with id=1");
        }
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Connect to WiFi
    Serial.println("Connecting to WiFi...");
    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected!");
    
    // Get initial sensor data
    getSensorData();
}

void loop() {
    // Check if 15 seconds have passed since last request
    if (millis() - lastRequestTime >= requestInterval) {
        lastRequestTime = millis(); // Update last request time
        
        // Get latest sensor data
        getSensorData();
        
        // Make the climbing sessions request
        makeRequest();
    }
}

String getUserIdFromRFID() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.reconnect();
        return "";
    }

    HTTPClient http;
    String requestURL = String(SUPABASE_BASE_URL) + "/bracelets?rfid=eq." + RFID + "&select=user_id";
    Serial.println("\n[INFO] Querying bracelets table: " + requestURL);

    http.begin(requestURL);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("[INFO] Bracelets Response: " + response);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (!error && doc.size() > 0) {
            String userId = doc[0]["user_id"];
            Serial.println("[SUCCESS] Found user_id: " + userId);
            http.end();
            return userId;
        } else {
            Serial.println("[ERROR] No bracelet found with RFID: " + String(RFID));
        }
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
    return "";
}

void makeRequest() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected! Reconnecting...");
        WiFi.reconnect();
        return;
    }

    // First get the USER_ID from RFID
    if (USER_ID.length() == 0) {
        USER_ID = getUserIdFromRFID();
        if (USER_ID.length() == 0) {
            Serial.println("[ERROR] Could not get USER_ID from RFID. Aborting request.");
            return;
        }
    }

    HTTPClient http;

    // Construct request URL for climbing sessions
    String requestURL = String(SUPABASE_BASE_URL) + "/climbing_sessions?user_id=eq." + USER_ID + "&limit=1";
    Serial.println("\n[INFO] Sending GET request to: " + requestURL);

    http.begin(requestURL);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        Serial.print("[INFO] HTTP Response Code: ");
        Serial.println(httpResponseCode);

        String response = http.getString();
        Serial.println("[INFO] Response: " + response);

        // Parse JSON response
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);

        if (!error) {
            Serial.println("[INFO] Parsed JSON:");
            Serial.println(response);
            
            // Extract first session
            if (doc.size() > 0) {
                String sessionID = doc[0]["id"];  // Replace with correct field if needed
                Serial.println("[SUCCESS] First session ID: " + sessionID);
            } else {
                Serial.println("[WARNING] No sessions found for this user.");
            }
        } else {
            Serial.println("[ERROR] Failed to parse JSON!");
        }
    } else {
        Serial.print("[ERROR] HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
}
