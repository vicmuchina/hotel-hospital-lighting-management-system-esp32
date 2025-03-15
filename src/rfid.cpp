#include <Arduino.h>        // Includes the Arduino core library that provides basic functions and definitions
#include <SPI.h>            // Includes the SPI library for serial communication with peripherals
#include <MFRC522.h>        // Includes the MFRC522 library to interface with the RFID reader module

// Define the pins for the RC522 on ESP32-C3
#define RST_PIN 4    // Reset pin for the MFRC522 - controls the reset line of the RFID reader
#define SS_PIN  5    // Slave Select pin for the MFRC522 - selects the RFID reader as the active SPI device

// Define custom SPI pins (since GPIO23 is not available)
#define SCK_PIN   18  // SPI Clock pin - provides the clock signal for synchronous data transfer
#define MISO_PIN  19  // SPI MISO (Master In Slave Out) pin - data line from slave (RFID reader) to master (ESP32)
#define MOSI_PIN  10  // SPI MOSI (Master Out Slave In) pin - data line from master (ESP32) to slave (RFID reader)

// Define LED pins - adjust these if needed based on your ESP32-C3 board
#define LED_1_PIN 2   // LED 1 connected to GPIO2 - first output pin for LED control
#define LED_2_PIN 3   // LED 2 connected to GPIO3 - second output pin for LED control

// Create an instance of the MFRC522 class
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Initializes the RFID reader object with the specified pins

// Store the UIDs of your specific RFID tags
// These are from your test results
byte rfid1UID[4] = {0x13, 0xA3, 0x50, 0x11}; // First card "13 A3 50 11" - stores the unique ID of the first authorized RFID card
byte rfid2UID[4] = {0x03, 0x32, 0xC0, 0x0D}; // Second card "03 32 C0 0D" - stores the unique ID of the second authorized RFID card

// Variables to track the LED states
bool led1On = false;  // Boolean flag to track if LED 1 is currently on or off
bool led2On = false;  // Boolean flag to track if LED 2 is currently on or off

// Function to check if two UIDs match
bool compareUID(byte *uid1, byte *uid2, byte size) {  // Function declaration with parameters for two UIDs and their size
  for (byte i = 0; i < size; i++) {  // Loop through each byte of the UIDs
    if (uid1[i] != uid2[i]) return false;  // If any byte doesn't match, return false immediately
  }
  return true;  // If all bytes match, return true
}

void setup() {  // The setup function runs once when the ESP32 starts or resets
  // Initialize serial communication
  Serial.begin(115200);  // Starts serial communication at 115200 baud rate for debugging output
  delay(500);  // Wait 500 milliseconds to ensure serial port is ready
  Serial.println("\n--- Building Lighting Management System ---");  // Print header message to serial monitor
  
  // Initialize the SPI bus with custom pin mapping
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);  // Start SPI communication with the custom pins defined above
  
  // Initialize the MFRC522 RFID reader
  mfrc522.PCD_Init();  // Initialize the RFID reader module
  
  // Set up the LED pins as outputs
  pinMode(LED_1_PIN, OUTPUT);  // Configure the first LED pin as an output
  pinMode(LED_2_PIN, OUTPUT);  // Configure the second LED pin as an output
  
  // Initialize LEDs to off state
  digitalWrite(LED_1_PIN, LOW);  // Turn off LED 1 (LOW = 0V = off)
  digitalWrite(LED_2_PIN, LOW);  // Turn off LED 2 (LOW = 0V = off)
  
  // Test both LEDs quickly to confirm they're working
  Serial.println("Testing LEDs...");  // Print message indicating LED test is starting
  digitalWrite(LED_1_PIN, HIGH);  // Turn on LED 1 (HIGH = 3.3V = on)
  delay(500);  // Wait 500 milliseconds while LED 1 is on
  digitalWrite(LED_1_PIN, LOW);  // Turn off LED 1
  digitalWrite(LED_2_PIN, HIGH);  // Turn on LED 2
  delay(500);  // Wait 500 milliseconds while LED 2 is on
  digitalWrite(LED_2_PIN, LOW);  // Turn off LED 2
  
  Serial.println("System ready!");  // Print message indicating setup is complete
  Serial.println("Scan your RFID tag to control the lights");  // Print instruction for the user
}

void loop() {  // The loop function runs repeatedly after setup is complete
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {  // Check if a new RFID card is present in the reader's field
    return;  // If no new card is present, exit the loop and start again
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {  // Try to select the detected card and read its serial number
    return;  // If card selection fails, exit the loop and start again
  }

  // Print the card's UID for debugging
  Serial.print("Card scanned - UID: ");  // Print message indicating a card was successfully scanned
  for (byte i = 0; i < mfrc522.uid.size; i++) {  // Loop through each byte of the card's UID
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");  // Print a leading zero for values less than 0x10
    Serial.print(mfrc522.uid.uidByte[i], HEX);  // Print the byte value in hexadecimal format
  }
  Serial.println();  // Print a newline character to end the UID output

  // Check which RFID tag was scanned and control the corresponding LED
  if (compareUID(rfid1UID, mfrc522.uid.uidByte, 4)) {  // Check if the scanned card matches the first authorized card
    // First RFID tag controls LED 1
    led1On = !led1On;  // Toggle LED 1 state (if it was on, turn it off; if it was off, turn it on)
    digitalWrite(LED_1_PIN, led1On ? HIGH : LOW);  // Set the LED 1 pin based on the new state
    Serial.print("LED 1 is now ");  // Print part of the status message
    Serial.println(led1On ? "ON" : "OFF");  // Print whether LED 1 is now on or off
    
    // Provide visual feedback for seat assignment
    if (led1On) {  // If LED 1 is now on
      Serial.println("Seat 1 assigned to user.");  // Print message indicating seat 1 is assigned
    } else {  // If LED 1 is now off
      Serial.println("User has left Seat 1.");  // Print message indicating seat 1 is vacant
    }
  }
  else if (compareUID(rfid2UID, mfrc522.uid.uidByte, 4)) {  // Check if the scanned card matches the second authorized card
    // Second RFID tag controls LED 2
    led2On = !led2On;  // Toggle LED 2 state (if it was on, turn it off; if it was off, turn it on)
    digitalWrite(LED_2_PIN, led2On ? HIGH : LOW);  // Set the LED 2 pin based on the new state
    Serial.print("LED 2 is now ");  // Print part of the status message
    Serial.println(led2On ? "ON" : "OFF");  // Print whether LED 2 is now on or off
    
    // Provide visual feedback for seat assignment
    if (led2On) {  // If LED 2 is now on
      Serial.println("Seat 2 assigned to user.");  // Print message indicating seat 2 is assigned
    } else {  // If LED 2 is now off
      Serial.println("User has left Seat 2.");  // Print message indicating seat 2 is vacant
    }
  }
  else {  // If the scanned card doesn't match any authorized card
    // Unknown RFID tag
    Serial.println("Unknown RFID tag - Access denied");  // Print message indicating unauthorized access
    
    // Flash both LEDs to indicate unauthorized access
    for (int i = 0; i < 3; i++) {  // Loop 3 times to flash the LEDs
      digitalWrite(LED_1_PIN, HIGH);  // Turn on LED 1
      digitalWrite(LED_2_PIN, HIGH);  // Turn on LED 2
      delay(100);  // Wait 100 milliseconds with both LEDs on
      digitalWrite(LED_1_PIN, LOW);  // Turn off LED 1
      digitalWrite(LED_2_PIN, LOW);  // Turn off LED 2
      delay(100);  // Wait 100 milliseconds with both LEDs off
    }
  }

  // Halt PICC and stop encryption
  mfrc522.PICC_HaltA();  // Command the card to go into sleep mode
  mfrc522.PCD_StopCrypto1();  // Stop the encryption used for communication with the card

  // Small delay to prevent multiple reads of the same card
  delay(1000);  // Wait 1 second before checking for another card scan
}