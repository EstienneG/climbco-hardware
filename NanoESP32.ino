#include <SPI.h>
#include <MFRC522.h>

// Pin definitions for Nano ESP32
#define SS_PIN   D5   // GPIO5
#define RST_PIN  D4   // GPIO4
#define LED_PIN  D2   // Onboard LED (Optional)

// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Debug flag
#define DEBUG true

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

    // Show card details
    if (DEBUG) {
        Serial.println(F("\n=== Card Detected! ==="));
        Serial.print(F("Card UID: "));
        displayData(mfrc522.uid.uidByte, mfrc522.uid.size);
        
        // Get and print card type
        MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
        Serial.print(F("PICC Type: "));
        Serial.println(mfrc522.PICC_GetTypeName(piccType));
    }

    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    
    digitalWrite(LED_PIN, LOW);
    
    if (DEBUG) Serial.println(F("=== End of Card Reading ===\n"));
}
