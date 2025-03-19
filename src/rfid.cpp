#include <Arduino.h>        // Arduino core library - provides basic Arduino functionality and abstractions
#include <SPI.h>            // SPI communication library - enables Serial Peripheral Interface protocol communication
#include <MFRC522.h>        // RFID reader library - specialized library for interfacing with MFRC522 RFID readers

// Define the pins for the RC522 on ESP32-C3
// In embedded systems, we must map physical microcontroller pins to our peripherals
#define RST_PIN 4           // RFID reset pin - controls the reset line of the RFID reader
#define SS_PIN  5           // RFID SPI select pin (Slave Select) - selects the RFID reader on the SPI bus

// Define custom SPI pins - ESP32-C3 allows flexible pin assignments for SPI communication
#define SCK_PIN   18        // SPI clock pin - provides the clock signal for synchronized data transfer
#define MISO_PIN  19        // SPI MISO pin (Master In Slave Out) - data line from RFID reader to ESP32
#define MOSI_PIN  10        // SPI MOSI pin (Master Out Slave In) - data line from ESP32 to RFID reader

// Define Relay control pins - these control the actual lighting outputs
#define RELAY_1_PIN 6       // Pin to control relay 1 - digital output that activates first relay
#define RELAY_2_PIN 7       // Pin to control relay 2 - digital output that activates second relay

// Create MFRC522 instance - object-oriented approach to hardware abstraction
MFRC522 mfrc522(SS_PIN, RST_PIN);  // RFID reader - creates instance with specified pins

// Store the UIDs of your specific RFID tags - security by allowing only specific cards
// The byte arrays store the unique ID of each RFID tag in hexadecimal format
byte rfid1UID[4] = {0x13, 0xA3, 0x50, 0x11};  // First authorized RFID card - 4-byte unique identifier
byte rfid2UID[4] = {0x03, 0x32, 0xC0, 0x0D};  // Second authorized RFID card - 4-byte unique identifier

// Variables to track the LED/Relay states - embedded systems need to track state
bool relay1On = false;      // Status of relay 1 - tracks whether seat 1 is occupied
bool relay2On = false;      // Status of relay 2 - tracks whether seat 2 is occupied

// Variables to track which tag turned on which relay - implements ownership concept
byte relay1Owner[4] = {0, 0, 0, 0};  // UID of card that activated relay 1 - stores 4-byte ID
byte relay2Owner[4] = {0, 0, 0, 0};  // UID of card that activated relay 2 - stores 4-byte ID
bool relay1HasOwner = false;  // Flag for relay 1 ownership - indicates if relay 1 has an assigned user
bool relay2HasOwner = false;  // Flag for relay 2 ownership - indicates if relay 2 has an assigned user

// Function to check if two UIDs match - helper function for authentication
// Takes two byte arrays and their size, returns true if all bytes match
bool compareUID(byte *uid1, byte *uid2, byte size) {
  for (byte i = 0; i < size; i++) {
    if (uid1[i] != uid2[i]) return false;  // If any byte doesn't match, return false
  }
  return true;  // If all bytes match, return true
}

// Function to save the current UID as the owner of a relay - ownership assignment
// Copies bytes from source array to destination array
void saveOwner(byte *destination, byte *source, byte size) {
  for (byte i = 0; i < size; i++) {
    destination[i] = source[i];  // Copy each byte from source to destination
  }
}

void setup() {
  // Initialize serial communication - crucial for debugging embedded systems
  Serial.begin(115200);  // Sets baud rate to 115200 bits per second
  delay(500);  // Short delay to ensure serial connection is established
  Serial.println("\n--- Building Lighting Management System ---");  // Print system header
  
  // Initialize the SPI bus with custom pin mapping - configures SPI communication
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);  // Start SPI with custom pins
  
  // Initialize the MFRC522 RFID reader - prepares the RFID hardware
  mfrc522.PCD_Init();  // Initializes the RFID reader in Proximity Coupling Device mode
  
  // Set up the relay pins as outputs - configure GPIO pin modes
  pinMode(RELAY_1_PIN, OUTPUT);  // Sets relay 1 pin as output
  pinMode(RELAY_2_PIN, OUTPUT);  // Sets relay 2 pin as output
  
  // Initialize relays to off state - ensures system starts in known state
  // For solid state relays, typically HIGH turns on the relay
  digitalWrite(RELAY_1_PIN, LOW);  // Sets relay 1 to LOW voltage (off)
  digitalWrite(RELAY_2_PIN, LOW);  // Sets relay 2 to LOW voltage (off)
  
  // Test both relays quickly to confirm they're working - hardware validation
  Serial.println("Testing relays...");  // Indicate test is starting
  digitalWrite(RELAY_1_PIN, HIGH);  // Turn on relay 1
  delay(500);  // Wait 500ms
  digitalWrite(RELAY_1_PIN, LOW);  // Turn off relay 1
  digitalWrite(RELAY_2_PIN, HIGH);  // Turn on relay 2
  delay(500);  // Wait 500ms
  digitalWrite(RELAY_2_PIN, LOW);  // Turn off relay 2
  
  Serial.println("System ready!");  // Indicate system initialization complete
  Serial.println("Scan your RFID tag to control the lights");  // User instruction
  
  // Print current seat status - initial system state report
  Serial.println("\nCurrent status:");
  Serial.println("Seat 1: Free");  // Initial status of seat 1
  Serial.println("Seat 2: Free");  // Initial status of seat 2
}

void loop() {
  // Look for new cards - continuous polling for RFID tags
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;  // If no new card is present, exit this loop iteration
  }

  // Select one of the cards - attempt to read the detected card
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;  // If card read fails, exit this loop iteration
  }

  // Print the card's UID for debugging - helps with system monitoring
  Serial.print("Card scanned - UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Format output for readability: add leading zero for values less than 0x10
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);  // Print each byte in hexadecimal
  }
  Serial.println();  // End the line after printing the UID

  // Check if this card is the owner of relay 1 - ownership verification
  bool isRelay1Owner = relay1HasOwner && compareUID(relay1Owner, mfrc522.uid.uidByte, 4);
  
  // Check if this card is the owner of relay 2 - ownership verification
  bool isRelay2Owner = relay2HasOwner && compareUID(relay2Owner, mfrc522.uid.uidByte, 4);
  
  // Check if this is one of the authorized cards - authentication check
  bool isAuthorizedCard = compareUID(rfid1UID, mfrc522.uid.uidByte, 4) || 
                          compareUID(rfid2UID, mfrc522.uid.uidByte, 4);
  
  // Handle the card scan based on authorization and ownership - core business logic
  if (isRelay1Owner) {
    // This card owns relay 1, so it can turn it off - implements "check-out" functionality
    relay1On = false;  // Update relay 1 state
    relay1HasOwner = false;  // Clear ownership
    digitalWrite(RELAY_1_PIN, LOW);  // Turn off the physical relay
    Serial.println("Relay 1 is now OFF");  // Log the action
    Serial.println("User has left Seat 1.");  // User feedback
  }
  else if (isRelay2Owner) {
    // This card owns relay 2, so it can turn it off - implements "check-out" functionality
    relay2On = false;  // Update relay 2 state
    relay2HasOwner = false;  // Clear ownership
    digitalWrite(RELAY_2_PIN, LOW);  // Turn off the physical relay
    Serial.println("Relay 2 is now OFF");  // Log the action
    Serial.println("User has left Seat 2.");  // User feedback
  }
  else if (isAuthorizedCard) {
    // This is an authorized card but doesn't currently own any relay - handling "check-in"
    if (compareUID(rfid1UID, mfrc522.uid.uidByte, 4)) {
      // First RFID card - check if relay 1 is available
      if (!relay1On) {
        // relay 1 is available - assign it to this user
        relay1On = true;  // Update relay 1 state
        relay1HasOwner = true;  // Set ownership flag
        saveOwner(relay1Owner, mfrc522.uid.uidByte, 4);  // Save user's UID as owner
        digitalWrite(RELAY_1_PIN, HIGH);  // Turn on the physical relay
        Serial.println("Relay 1 is now ON");  // Log the action
        Serial.println("Seat 1 assigned to user.");  // User feedback
      }
      else {
        // relay 1 is already taken - provide feedback
        Serial.println("Seat 1 is already occupied.");
        
        // Flash relay 1 to indicate it's already taken - visual feedback
        for (int i = 0; i < 2; i++) {
          digitalWrite(RELAY_1_PIN, LOW);  // Turn off briefly
          delay(100);  // Short delay
          digitalWrite(RELAY_1_PIN, HIGH);  // Turn back on
          delay(100);  // Short delay
        }
        // Return to proper state - ensure relay stays in the correct state
        digitalWrite(RELAY_1_PIN, HIGH);
      }
    }
    else if (compareUID(rfid2UID, mfrc522.uid.uidByte, 4)) {
      // Second RFID card - check if relay 2 is available
      if (!relay2On) {
        // relay 2 is available - assign it to this user
        relay2On = true;  // Update relay 2 state
        relay2HasOwner = true;  // Set ownership flag
        saveOwner(relay2Owner, mfrc522.uid.uidByte, 4);  // Save user's UID as owner
        digitalWrite(RELAY_2_PIN, HIGH);  // Turn on the physical relay
        Serial.println("Relay 2 is now ON");  // Log the action
        Serial.println("Seat 2 assigned to user.");  // User feedback
      }
      else {
        // relay 2 is already taken - provide feedback
        Serial.println("Seat 2 is already occupied.");
        
        // Flash relay 2 to indicate it's already taken - visual feedback
        for (int i = 0; i < 2; i++) {
          digitalWrite(RELAY_2_PIN, LOW);  // Turn off briefly
          delay(100);  // Short delay
          digitalWrite(RELAY_2_PIN, HIGH);  // Turn back on
          delay(100);  // Short delay
        }
        // Return to proper state - ensure relay stays in the correct state
        digitalWrite(RELAY_2_PIN, HIGH);
      }
    }
  }
  else {
    // Unauthorized RFID tag - security enforcement
    Serial.println("Unknown RFID tag - Access denied");
    
    // Check if all seats are occupied - additional user feedback
    if (relay1On && relay2On) {
      Serial.println("All seats are currently occupied.");
    }
    else {
      // Flash both relays to indicate unauthorized access - visual alarm
      for (int i = 0; i < 3; i++) {
        digitalWrite(RELAY_1_PIN, HIGH);  // Turn on relay 1
        digitalWrite(RELAY_2_PIN, HIGH);  // Turn on relay 2
        delay(100);  // Short delay
        digitalWrite(RELAY_1_PIN, LOW);  // Turn off relay 1
        digitalWrite(RELAY_2_PIN, LOW);  // Turn off relay 2
        delay(100);  // Short delay
      }
      
      // Restore the relays to their correct states - recover from alarm
      digitalWrite(RELAY_1_PIN, relay1On ? HIGH : LOW);  // Restore relay 1 to its correct state
      digitalWrite(RELAY_2_PIN, relay2On ? HIGH : LOW);  // Restore relay 2 to its correct state
    }
  }

  // Print current status after any change - system state report
  Serial.println("\nCurrent status:");
  Serial.println(relay1On ? "Seat 1: Occupied" : "Seat 1: Free");  // Status of seat 1
  Serial.println(relay2On ? "Seat 2: Occupied" : "Seat 2: Free");  // Status of seat 2

  // Halt PICC and stop encryption - proper RFID card handling
  mfrc522.PICC_HaltA();  // Halts communication with the card
  mfrc522.PCD_StopCrypto1();  // Stops the encryption on the PCD

  // Small delay to prevent multiple reads of the same card - debounce mechanism
  delay(1000);  // Wait 1 second before scanning for a new card
}