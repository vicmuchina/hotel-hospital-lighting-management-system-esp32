#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

// Define the pins for the RC522 on ESP32-C3
#define RST_PIN 4    // Reset pin for the MFRC522
#define SS_PIN  5    // Slave Select pin for the MFRC522

// Define custom SPI pins (since GPIO23 is not available)
#define SCK_PIN   18  // SPI Clock pin
#define MISO_PIN  19  // SPI MISO (Master In Slave Out) pin
#define MOSI_PIN  10  // SPI MOSI (Master Out Slave In) pin (alternate pin)

// Define LED pins - adjust these if needed based on your ESP32-C3 board
#define LED_1_PIN 2   // LED 1 connected to GPIO2
#define LED_2_PIN 3   // LED 2 connected to GPIO3

// Create an instance of the MFRC522 class
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Store the UIDs of your specific RFID tags
// These are from your test results
byte rfid1UID[4] = {0x13, 0xA3, 0x50, 0x11}; // First card "13 A3 50 11"
byte rfid2UID[4] = {0x03, 0x32, 0xC0, 0x0D}; // Second card "03 32 C0 0D"

// Variables to track the LED states
bool led1On = false;
bool led2On = false;

// Function to check if two UIDs match
bool compareUID(byte *uid1, byte *uid2, byte size) {
  for (byte i = 0; i < size; i++) {
    if (uid1[i] != uid2[i]) return false;
  }
  return true;
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- Building Lighting Management System ---");
  
  // Initialize the SPI bus with custom pin mapping
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Initialize the MFRC522 RFID reader
  mfrc522.PCD_Init();
  
  // Set up the LED pins as outputs
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  
  // Initialize LEDs to off state
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);
  
  // Test both LEDs quickly to confirm they're working
  Serial.println("Testing LEDs...");
  digitalWrite(LED_1_PIN, HIGH);
  delay(500);
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, HIGH);
  delay(500);
  digitalWrite(LED_2_PIN, LOW);
  
  Serial.println("System ready!");
  Serial.println("Scan your RFID tag to control the lights");
}

void loop() {
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;  // Continue checking
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;  // Continue checking
  }

  // Print the card's UID for debugging
  Serial.print("Card scanned - UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Check which RFID tag was scanned and control the corresponding LED
  if (compareUID(rfid1UID, mfrc522.uid.uidByte, 4)) {
    // First RFID tag controls LED 1
    led1On = !led1On;  // Toggle LED 1 state
    digitalWrite(LED_1_PIN, led1On ? HIGH : LOW);
    Serial.print("LED 1 is now ");
    Serial.println(led1On ? "ON" : "OFF");
    
    // Provide visual feedback for seat assignment
    if (led1On) {
      Serial.println("Seat 1 assigned to user.");
    } else {
      Serial.println("User has left Seat 1.");
    }
  }
  else if (compareUID(rfid2UID, mfrc522.uid.uidByte, 4)) {
    // Second RFID tag controls LED 2
    led2On = !led2On;  // Toggle LED 2 state
    digitalWrite(LED_2_PIN, led2On ? HIGH : LOW);
    Serial.print("LED 2 is now ");
    Serial.println(led2On ? "ON" : "OFF");
    
    // Provide visual feedback for seat assignment
    if (led2On) {
      Serial.println("Seat 2 assigned to user.");
    } else {
      Serial.println("User has left Seat 2.");
    }
  }
  else {
    // Unknown RFID tag
    Serial.println("Unknown RFID tag - Access denied");
    
    // Flash both LEDs to indicate unauthorized access
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_1_PIN, HIGH);
      digitalWrite(LED_2_PIN, HIGH);
      delay(100);
      digitalWrite(LED_1_PIN, LOW);
      digitalWrite(LED_2_PIN, LOW);
      delay(100);
    }
  }

  // Halt PICC and stop encryption
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // Small delay to prevent multiple reads of the same card
  delay(1000);
}