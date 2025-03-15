#include <Arduino.h>  // Include the Arduino core library for basic functions
#include <SPI.h>      // Include the SPI library for communication with the RFID reader
#include <MFRC522.h>  // Include the MFRC522 library for handling the RFID reader

// Define the pins for the RC522 on ESP32-C3
#define RST_PIN 4    // Reset pin for the MFRC522
#define SS_PIN  5    // Slave Select pin for the MFRC522

// Define custom SPI pins (since GPIO23 is not available)
#define SCK_PIN   18  // SPI Clock pin
#define MISO_PIN  19  // SPI MISO (Master In Slave Out) pin
#define MOSI_PIN  10  // SPI MOSI (Master Out Slave In) pin (alternate pin)

// Create an instance of the MFRC522 class with the defined Slave Select and Reset pins
MFRC522 mfrc522(SS_PIN, RST_PIN);  

// The setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);  // Start serial communication at a baud rate of 115200
  while (!Serial) {
    // Wait for the Serial to be ready (useful on some boards)
  }
  
  // Initialize the SPI bus with custom pin mapping
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Initialize the MFRC522 RFID reader
  mfrc522.PCD_Init();
  Serial.println("Ready to read/write RFID tags");  // Print a message indicating readiness
}

// The loop function runs repeatedly after setup
void loop() {
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;  // If no card is present, exit the loop and check again
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;  // If reading the UID fails, exit the loop and check again
  }

  // Print the card's UID
  Serial.print("Card UID: ");  // Print a label for the UID
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Loop through each byte of the UID
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");  // Print leading zero for single-digit hex
    Serial.print(mfrc522.uid.uidByte[i], HEX);  // Print the UID byte in hexadecimal format
  }
  Serial.println();  // Print a newline after the UID

  // Check if the card is a MIFARE Classic type
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);  // Get the type of the card
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    // If the card is not a supported MIFARE type, print a message and halt the card
    Serial.println("Only MIFARE Classic cards are supported.");
    mfrc522.PICC_HaltA();  // Halt the PICC (Proximity Integrated Circuit Card)
    return;  // Exit the loop
  }

  // Define the sector and block to write to
  byte sector    = 1;  // Sector 1 (MIFARE Classic cards are divided into sectors)
  byte blockAddr = 4;  // Block 4 (first block in sector 1)

  // Define the default key (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF)
  MFRC522::MIFARE_Key key;  // Create a key object for MIFARE authentication
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;  // Set all bytes of the key to 0xFF (default key)
  }

  // Authenticate with Key A
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A,  // Command for Key A authentication
      blockAddr,  // Block address to authenticate
      &key,  // Pointer to the key object
      &mfrc522.uid  // Pointer to the UID of the card
  );
  if (status != MFRC522::STATUS_OK) {
    // If authentication fails, print an error message
    Serial.print("Authentication failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));  // Print the status code name
    mfrc522.PICC_HaltA();  // Halt the PICC
    return;  // Exit the loop
  }

  // Data to write (16 bytes for a MIFARE Classic block)
  byte dataBlock[16] = {
    'H', 'e', 'l', 'l', 'o', ',', ' ', 'W',  // Example data to write
    'o', 'r', 'l', 'd', '!', 0, 0, 0  // Fill the rest with zeros
  };

  // Write data to the block
  status = mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);  // Write the data block to the specified block address
  if (status != MFRC522::STATUS_OK) {
    // If writing fails, print an error message
    Serial.print("Write failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));  // Print the status code name
  } else {
    Serial.println("Write successful!");  // Print success message

    // Read back the data to verify
    byte buffer[18];   // Buffer to hold the read data (16 bytes + 2 bytes for CRC)
    byte size = sizeof(buffer);  // Size of the buffer

    // Attempt to read the data back from the block
    status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);  // Read the data into the buffer
    if (status == MFRC522::STATUS_OK) {
      Serial.print("Data read: ");  // Print a label for the read data
      for (byte i = 0; i < 16; i++) {
        Serial.write(buffer[i]);  // Write each byte of the buffer to the serial output
      }
      Serial.println();  // Print a newline after the data
    } else {
      // If reading fails, print an error message
      Serial.print("Read failed: ");
      Serial.println(mfrc522.GetStatusCodeName(status));  // Print the status code name
    }
  }

  // Halt PICC and stop encryption
  mfrc522.PICC_HaltA();  // Halt the PICC
  mfrc522.PCD_StopCrypto1();  // Stop encryption on the reader

  // Small delay before the next loop iteration
  delay(2000);  // Wait for 2 seconds before repeating the loop
}
