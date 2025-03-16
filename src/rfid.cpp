#include <Arduino.h>        // Includes the Arduino core library that provides basic functions and definitions
#include <SPI.h>            // Includes the Serial Peripheral Interface library for communication with SPI devices
#include <MFRC522.h>        // Includes the library for interfacing with the MFRC522 RFID reader module

// Define the pins for the RC522 on ESP32-C3
#define RST_PIN 4           // Defines GPIO pin 4 as the reset pin for the RFID reader
#define SS_PIN  5           // Defines GPIO pin 5 as the Slave Select pin for SPI communication with the RFID reader

// Define custom SPI pins
#define SCK_PIN   18        // Defines GPIO pin 18 as the Serial Clock pin for SPI communication
#define MISO_PIN  19        // Defines GPIO pin 19 as the Master In Slave Out pin (data from RFID reader to ESP32)
#define MOSI_PIN  10        // Defines GPIO pin 10 as the Master Out Slave In pin (data from ESP32 to RFID reader)

// Define LED pins
#define LED_1_PIN 2         // Defines GPIO pin 2 for controlling LED 1 (indicating Seat 1 status)
#define LED_2_PIN 3         // Defines GPIO pin 3 for controlling LED 2 (indicating Seat 2 status)

// Create an instance of the MFRC522 class
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Creates an RFID reader object with the specified pins

// Store the UIDs of your specific RFID tags
byte rfid1UID[4] = {0x13, 0xA3, 0x50, 0x11};  // Stores the 4-byte UID of the first authorized RFID card
byte rfid2UID[4] = {0x03, 0x32, 0xC0, 0x0D};  // Stores the 4-byte UID of the second authorized RFID card

// Variables to track the LED states
bool led1On = false;        // Boolean flag to track if LED 1 is currently on (Seat 1 occupied)
bool led2On = false;        // Boolean flag to track if LED 2 is currently on (Seat 2 occupied)

// Variables to track which tag turned on which LED
byte led1Owner[4] = {0, 0, 0, 0};  // Array to store the UID of the card that turned on LED 1
byte led2Owner[4] = {0, 0, 0, 0};  // Array to store the UID of the card that turned on LED 2
bool led1HasOwner = false;  // Flag to indicate if LED 1 currently has an owner
bool led2HasOwner = false;  // Flag to indicate if LED 2 currently has an owner

// Function to check if two UIDs match
bool compareUID(byte *uid1, byte *uid2, byte size) {  // Function that compares two byte arrays (UIDs)
  for (byte i = 0; i < size; i++) {  // Iterate through each byte of the UIDs
    if (uid1[i] != uid2[i]) return false;  // If any byte doesn't match, return false
  }
  return true;  // If all bytes match, return true
}

// Function to save the current UID as the owner of an LED
void saveOwner(byte *destination, byte *source, byte size) {  // Function to copy a UID array to another array
  for (byte i = 0; i < size; i++) {  // Iterate through each byte of the UIDs
    destination[i] = source[i];  // Copy each byte from source to destination
  }
}

void setup() {  // Setup function runs once at startup
  // Initialize serial communication
  Serial.begin(115200);  // Start serial communication at 115200 baud rate for debugging
  delay(500);  // Wait 500ms to ensure serial port is ready
  Serial.println("\n--- Building Lighting Management System ---");  // Print header message to serial monitor
  
  // Initialize the SPI bus with custom pin mapping
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);  // Start SPI communication with the custom pins
  
  // Initialize the MFRC522 RFID reader
  mfrc522.PCD_Init();  // Initialize the RFID reader module
  
  // Set up the LED pins as outputs
  pinMode(LED_1_PIN, OUTPUT);  // Configure LED 1 pin as an output
  pinMode(LED_2_PIN, OUTPUT);  // Configure LED 2 pin as an output
  
  // Initialize LEDs to off state
  digitalWrite(LED_1_PIN, LOW);  // Set LED 1 to LOW (0V, off)
  digitalWrite(LED_2_PIN, LOW);  // Set LED 2 to LOW (0V, off)
  
  // Test both LEDs quickly to confirm they're working
  Serial.println("Testing LEDs...");  // Print message indicating LED test
  digitalWrite(LED_1_PIN, HIGH);  // Turn on LED 1 (3.3V)
  delay(500);  // Wait 500ms
  digitalWrite(LED_1_PIN, LOW);  // Turn off LED 1
  digitalWrite(LED_2_PIN, HIGH);  // Turn on LED 2
  delay(500);  // Wait 500ms
  digitalWrite(LED_2_PIN, LOW);  // Turn off LED 2
  
  Serial.println("System ready!");  // Print ready message
  Serial.println("Scan your RFID tag to control the lights");  // Print user instruction
}

void loop() {  // Loop function runs repeatedly after setup
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {  // Check if a new RFID card is in the reader's field
    return;  // If no new card, exit the function and start loop again
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {  // Try to select the card and read its serial number
    return;  // If selection fails, exit the function and start loop again
  }

  // Print the card's UID for debugging
  Serial.print("Card scanned - UID: ");  // Print message prefix
  for (byte i = 0; i < mfrc522.uid.size; i++) {  // Loop through each byte of the card's UID
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");  // Print leading zero for values < 16
    Serial.print(mfrc522.uid.uidByte[i], HEX);  // Print the byte in hexadecimal format
  }
  Serial.println();  // Print a newline character

  // Check if this card is the owner of LED 1
  bool isLed1Owner = led1HasOwner && compareUID(led1Owner, mfrc522.uid.uidByte, 4);  // Check if scanned card is LED 1's owner
  
  // Check if this card is the owner of LED 2
  bool isLed2Owner = led2HasOwner && compareUID(led2Owner, mfrc522.uid.uidByte, 4);  // Check if scanned card is LED 2's owner
  
  // Check if this is one of the authorized cards
  bool isAuthorizedCard = compareUID(rfid1UID, mfrc522.uid.uidByte, 4) || 
                          compareUID(rfid2UID, mfrc522.uid.uidByte, 4);  // Check if card is either of the authorized cards
  
  // Handle the card scan based on authorization and ownership
  if (isLed1Owner) {  // If this card is the owner of LED 1
    // This card owns LED 1, so it can turn it off
    led1On = false;  // Update the LED 1 state to off
    led1HasOwner = false;  // Remove ownership of LED 1
    digitalWrite(LED_1_PIN, LOW);  // Turn off LED 1 physically
    Serial.println("LED 1 is now OFF");  // Print status message
    Serial.println("User has left Seat 1.");  // Print seat status message
  }
  else if (isLed2Owner) {  // If this card is the owner of LED 2
    // This card owns LED 2, so it can turn it off
    led2On = false;  // Update the LED 2 state to off
    led2HasOwner = false;  // Remove ownership of LED 2
    digitalWrite(LED_2_PIN, LOW);  // Turn off LED 2 physically
    Serial.println("LED 2 is now OFF");  // Print status message
    Serial.println("User has left Seat 2.");  // Print seat status message
  }
  else if (isAuthorizedCard) {  // If this is an authorized card but not currently an owner
    // This is an authorized card but doesn't currently own any LED
    if (compareUID(rfid1UID, mfrc522.uid.uidByte, 4)) {  // If it's the first authorized card
      // First RFID card - check if LED 1 is available
      if (!led1On) {  // If LED 1 is not currently on (seat available)
        // LED 1 is available
        led1On = true;  // Update LED 1 state to on
        led1HasOwner = true;  // Set ownership flag
        saveOwner(led1Owner, mfrc522.uid.uidByte, 4);  // Save this card's UID as the owner
        digitalWrite(LED_1_PIN, HIGH);  // Turn on LED 1 physically
        Serial.println("LED 1 is now ON");  // Print status message
        Serial.println("Seat 1 assigned to user.");  // Print seat assignment message
      }
      else {  // If LED 1 is already on (seat occupied)
        // LED 1 is already taken
        Serial.println("Seat 1 is already occupied.");  // Print occupancy message
        
        // Flash LED 1 to indicate it's already taken
        for (int i = 0; i < 2; i++) {  // Loop twice for flashing effect
          digitalWrite(LED_1_PIN, LOW);  // Turn off LED 1
          delay(100);  // Wait 100ms
          digitalWrite(LED_1_PIN, HIGH);  // Turn on LED 1
          delay(100);  // Wait 100ms
        }
      }
    }
    else if (compareUID(rfid2UID, mfrc522.uid.uidByte, 4)) {  // If it's the second authorized card
      // Second RFID card - check if LED 2 is available
      if (!led2On) {  // If LED 2 is not currently on (seat available)
        // LED 2 is available
        led2On = true;  // Update LED 2 state to on
        led2HasOwner = true;  // Set ownership flag
        saveOwner(led2Owner, mfrc522.uid.uidByte, 4);  // Save this card's UID as the owner
        digitalWrite(LED_2_PIN, HIGH);  // Turn on LED 2 physically
        Serial.println("LED 2 is now ON");  // Print status message
        Serial.println("Seat 2 assigned to user.");  // Print seat assignment message
      }
      else {  // If LED 2 is already on (seat occupied)
        // LED 2 is already taken
        Serial.println("Seat 2 is already occupied.");  // Print occupancy message
        
        // Flash LED 2 to indicate it's already taken
        for (int i = 0; i < 2; i++) {  // Loop twice for flashing effect
          digitalWrite(LED_2_PIN, LOW);  // Turn off LED 2
          delay(100);  // Wait 100ms
          digitalWrite(LED_2_PIN, HIGH);  // Turn on LED 2
          delay(100);  // Wait 100ms
        }
      }
    }
  }
  else {  // If the card is not authorized (neither of the known cards)
    // Unauthorized RFID tag
    Serial.println("Unknown RFID tag - Access denied");  // Print access denied message
    
    // Check if all seats are occupied
    if (led1On && led2On) {  // If both LEDs are on (all seats occupied)
      Serial.println("All seats are currently occupied.");  // Print all seats occupied message
      // No LED blinking when all seats are occupied (skips the visual feedback)
    }
    else {  // If at least one seat is available
      // Only flash LEDs when there are available seats but card is unauthorized
      // Flash both LEDs to indicate unauthorized access
      for (int i = 0; i < 3; i++) {  // Loop three times for flashing effect
        digitalWrite(LED_1_PIN, HIGH);  // Turn on LED 1
        digitalWrite(LED_2_PIN, HIGH);  // Turn on LED 2
        delay(100);  // Wait 100ms with LEDs on
        digitalWrite(LED_1_PIN, LOW);  // Turn off LED 1
        digitalWrite(LED_2_PIN, LOW);  // Turn off LED 2
        delay(100);  // Wait 100ms with LEDs off
      }
      
      // Restore the LEDs to their correct states
      digitalWrite(LED_1_PIN, led1On ? HIGH : LOW);  // Restore LED 1 to its previous state
      digitalWrite(LED_2_PIN, led2On ? HIGH : LOW);  // Restore LED 2 to its previous state
    }
  }

  // Halt PICC and stop encryption
  mfrc522.PICC_HaltA();  // Send halt command to the card (puts it in standby)
  mfrc522.PCD_StopCrypto1();  // Stop the encryption used for communication with the card

  // Small delay to prevent multiple reads of the same card
  delay(1000);  // Wait 1 second before starting the next loop iteration
}