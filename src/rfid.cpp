#include <Arduino.h>        // Arduino core library - provides basic Arduino functionality and abstractions
#include <SPI.h>            // SPI communication library - enables Serial Peripheral Interface protocol communication
#include <MFRC522.h>        // RFID reader library - specialized library for interfacing with MFRC522 RFID readers
#include <Wire.h>           // I2C communication library - enables I2C protocol for OLED communication
#include <Adafruit_GFX.h>   // Graphics library - provides drawing primitives like text, lines, circles
#include <Adafruit_SSD1306.h> // OLED display driver - specific for SSD1306 based 0.96" displays

// OLED Display Configuration
#define SCREEN_WIDTH 128     // OLED display width in pixels
#define SCREEN_HEIGHT 32     // OLED display height in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  // I2C address of the OLED display (typically 0x3C or 0x3D)

// Create display instance
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define the pins for the RC522 on ESP32-C3
#define RST_PIN 4           // RFID reset pin - controls the reset line of the RFID reader
#define SS_PIN  5           // RFID SPI select pin (Slave Select) - selects the RFID reader on the SPI bus

// Define custom SPI pins - ESP32-C3 allows flexible pin assignments for SPI communication
#define SCK_PIN   18        // SPI clock pin - provides the clock signal for synchronized data transfer
#define MISO_PIN  19        // SPI MISO pin (Master In Slave Out) - data line from RFID reader to ESP32
#define MOSI_PIN  10        // SPI MOSI pin (Master Out Slave In) - data line from ESP32 to RFID reader

// Define I2C pins for OLED display - hardware I2C interface
#define SDA_PIN 8           // I2C data line - carries bidirectional data between controller and display
#define SCL_PIN 2           // I2C clock line - provides timing synchronization for data transfer

// Define Relay control pins - these drive the relay switching mechanism
#define RELAY_1_PIN 6       // Pin to control relay 1 - digital output that activates first relay
#define RELAY_2_PIN 7       // Pin to control relay 2 - digital output that activates second relay

// Define Relay power pins - these power the common pins of the relays instead of using 3.3V
#define RELAY_1_POWER_PIN 0  // Pin to provide power to relay 1 common pin - constant HIGH output
#define RELAY_2_POWER_PIN 3  // Pin to provide power to relay 2 common pin - constant HIGH output

// Create MFRC522 instance - object-oriented approach to hardware abstraction
MFRC522 mfrc522(SS_PIN, RST_PIN);  // RFID reader - creates instance with specified pins

// Store the UIDs of your specific RFID tags - security by allowing only specific cards
// The byte arrays store the unique ID of each RFID tag in hexadecimal format
byte rfid1UID[4] = {0x13, 0xA3, 0x50, 0x11};  // First authorized RFID card - 4-byte unique identifier
byte rfid2UID[4] = {0x03, 0x32, 0xC0, 0x0D};  // Second authorized RFID card - 4-byte unique identifier

// Variables to track the LED/Relay states - embedded systems need to track state
bool relay1On = false;      // Status of relay 1 - tracks whether room 1 is occupied
bool relay2On = false;      // Status of relay 2 - tracks whether room 2 is occupied

// Variables to track which tag turned on which relay - implements ownership concept
byte relay1Owner[4] = {0, 0, 0, 0};  // UID of card that activated relay 1 - stores 4-byte ID
byte relay2Owner[4] = {0, 0, 0, 0};  // UID of card that activated relay 2 - stores 4-byte ID
bool relay1HasOwner = false;  // Flag for relay 1 ownership - indicates if relay 1 has an assigned user
bool relay2HasOwner = false;  // Flag for relay 2 ownership - indicates if relay 2 has an assigned user

// Buffer for storing display messages - manages what will be shown on the OLED
String displayMessages[5];  // Circular buffer to hold last 5 messages - mimics serial monitor
int messageIndex = 0;       // Current position in circular buffer - tracks where to add new message

// Display mode flags
bool showingAlert = false;  // Flag to indicate if an alert message is currently displayed
unsigned long alertStartTime = 0; // Timestamp when alert was shown - used for timing alert display duration
#define ALERT_DURATION 3000 // Duration to show alert messages in milliseconds (3 seconds)

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

// Function to add a message to both Serial and OLED display - unified logging system
// Takes a string message and adds it to the circular buffer and updates display
void addMessage(String message) {
  // Print to Serial for USB debugging
  Serial.println(message);
  
  // Add to circular buffer for OLED display
  displayMessages[messageIndex] = message;
  messageIndex = (messageIndex + 1) % 5;  // Circular buffer implementation - wrap around after 5 messages
}

// Function to show an alert message on the OLED - displays important notifications prominently
void showAlert(String message1, String message2 = "") {
  display.clearDisplay();  // Clear the display buffer - prepares for new content
  display.setTextSize(1);  // Set text size to smallest (1) - allows more content to fit
  display.setTextColor(SSD1306_WHITE);  // Set text color to white - standard for monochrome OLED
  
  // Display the alert title with emphasis
  display.setCursor(0, 0);  // Start at top-left
  display.println("! ALERT !");  // Alert header
  display.println("------------------");  // Visual separator
  
  // Display the alert message centered
  display.setCursor(0, 24);  // Position for main message
  display.println(message1);  // First line of alert
  
  if (message2 != "") {  // If there's a second message line
    display.setCursor(0, 34);  // Position for second message line
    display.println(message2);  // Second line of alert
  }
  
  // Display UID information if available
  if (mfrc522.uid.size > 0) {  // If we have a card UID
    display.setCursor(0, 50);  // Position for UID information
    display.print("UID:");  // Label for UID
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      // Format UID bytes with leading zeros
      display.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      display.print(mfrc522.uid.uidByte[i], HEX);  // Display each byte in hexadecimal
    }
  }
  
  display.display();  // Update the display with the alert
  
  // Set alert display flags
  showingAlert = true;  // Set flag that we're in alert mode
  alertStartTime = millis();  // Record current time as alert start time
}

// Function to update the OLED display with current status and messages
void updateDisplay() {
  // Check if we should exit alert mode
  if (showingAlert && (millis() - alertStartTime > ALERT_DURATION)) {
    showingAlert = false;  // Exit alert mode after duration expires
  }
  
  // Skip updating if we're showing an alert and it's still active
  if (showingAlert) return;
  
  // Normal display mode - shows system status and recent messages
  display.clearDisplay();  // Clear the display buffer - prevents ghosting of previous content
  display.setTextSize(1);  // Set text size to smallest (1) - allows more content to fit on screen
  display.setTextColor(SSD1306_WHITE);  // Set text color to white - standard for monochrome OLED
  display.setCursor(0, 0);  // Start at top-left corner - position for title
  
  // Display title and status information - system state summary
  display.println("RFID Access System");
  display.println("------------------");
  display.print("Room 1: ");
  display.println(relay1On ? "Occupied" : "Free");  // Show status of room 1
  display.print("Room 2: ");
  display.println(relay2On ? "Occupied" : "Free");  // Show status of room 2
  display.println("------------------");
  
  // Display last few messages in reverse chronological order (newest at bottom)
  int count = 0;
  for (int i = 0; i < 3; i++) {  // Show up to 3 most recent messages - fits on screen
    int idx = (messageIndex - i - 1 + 5) % 5;  // Calculate index in circular buffer, accounting for wrap-around
    if (displayMessages[idx] != "") {  // Only show non-empty message slots
      display.println(displayMessages[idx]);  // Print the message on a new line
      count++;
    }
  }
  
  display.display();  // Push the buffer to the display - makes changes visible on screen
}

void setup() {
  // Initialize serial communication - crucial for debugging embedded systems
  Serial.begin(115200);  // Sets baud rate to 115200 bits per second
  delay(500);  // Short delay to ensure serial connection is established
  
  // Initialize I2C communication for OLED display - sets up the bus
  Wire.begin(SDA_PIN, SCL_PIN);  // Start I2C with custom pins for ESP32
  
  // Initialize OLED display - prepare the screen
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");  // Error handling if display init fails
    for(;;); // Don't proceed if display initialization fails - prevents running with partial features
  }
  
  // Initial display setup - welcome screen
  display.clearDisplay();  // Clear any artifacts
  display.setTextSize(1);  // Small text size
  display.setTextColor(SSD1306_WHITE);  // White text
  display.setCursor(0, 0);  // Start at top-left
  display.println("Building Lighting");
  display.println("Management System");
  display.println("Initializing...");
  display.display();  // Show initial message
  
  // Initialize the power pins for relays - these replace the 3.3V connection
  pinMode(RELAY_1_POWER_PIN, OUTPUT);  // Set relay 1 power pin as output
  pinMode(RELAY_2_POWER_PIN, OUTPUT);  // Set relay 2 power pin as output
  
  // Set the power pins to HIGH - continuously provide power to relay common pins
  digitalWrite(RELAY_1_POWER_PIN, HIGH);  // Turn on power for relay 1
  digitalWrite(RELAY_2_POWER_PIN, HIGH);  // Turn on power for relay 2
  
  // Initialize the SPI bus with custom pin mapping - configures SPI communication
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);  // Start SPI with custom pins
  
  // Initialize the MFRC522 RFID reader - prepares the RFID hardware
  mfrc522.PCD_Init();  // Initializes the RFID reader in Proximity Coupling Device mode
  
  // Set up the relay pins as outputs - configure GPIO pin modes
  pinMode(RELAY_1_PIN, OUTPUT);  // Sets relay 1 pin as output
  pinMode(RELAY_2_PIN, OUTPUT);  // Sets relay 2 pin as output
  
  // Initialize relays to off state - ensures system starts in known state
  digitalWrite(RELAY_1_PIN, LOW);  // Sets relay 1 to LOW voltage (off)
  digitalWrite(RELAY_2_PIN, LOW);  // Sets relay 2 to LOW voltage (off)
  
  // Test both relays quickly to confirm they're working - hardware validation
  addMessage("Testing relays...");  // Indicate test is starting
  digitalWrite(RELAY_1_PIN, HIGH);  // Turn on relay 1
  delay(500);  // Wait 500ms
  digitalWrite(RELAY_1_PIN, LOW);  // Turn off relay 1
  digitalWrite(RELAY_2_PIN, HIGH);  // Turn on relay 2
  delay(500);  // Wait 500ms
  digitalWrite(RELAY_2_PIN, LOW);  // Turn off relay 2
  
  addMessage("System ready!");  // Indicate system initialization complete
  addMessage("Scan your RFID tag");  // User instruction
  
  // Update display with current room status - initial system state report
  updateDisplay();  // Refresh the display with current information
}

void loop() {
  // Check if we should exit alert mode and update display
  if (showingAlert && (millis() - alertStartTime > ALERT_DURATION)) {
    showingAlert = false;  // Exit alert mode
    updateDisplay();  // Update with normal display
  }

  // Look for new cards - continuous polling for RFID tags
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;  // If no new card is present, exit this loop iteration
  }

  // Select one of the cards - attempt to read the detected card
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;  // If card read fails, exit this loop iteration
  }

  // Build UID string for display - format card ID for readability
  String uidString = "Card: ";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Format output for readability: add leading zero for values less than 0x10
    uidString += (mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uidString += String(mfrc522.uid.uidByte[i], HEX);  // Add each byte in hexadecimal
  }
  addMessage(uidString);  // Add card UID to message log

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
    addMessage("Relay 1 OFF");  // Log the action
    addMessage("Left Room 1");  // User feedback
    updateDisplay();  // Update display with new status
  }
  else if (isRelay2Owner) {
    // This card owns relay 2, so it can turn it off - implements "check-out" functionality
    relay2On = false;  // Update relay 2 state
    relay2HasOwner = false;  // Clear ownership
    digitalWrite(RELAY_2_PIN, LOW);  // Turn off the physical relay
    addMessage("Relay 2 OFF");  // Log the action
    addMessage("Left Room 2");  // User feedback
    updateDisplay();  // Update display with new status
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
        addMessage("Relay 1 ON");  // Log the action
        addMessage("Room 1 assigned");  // User feedback
        updateDisplay();  // Update display with new status
      }
      else {
        // relay 1 is already taken - provide feedback
        addMessage("Room 1 occupied");
        showAlert("Room 1 is already", "occupied");  // Show alert on display
        
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
        addMessage("Relay 2 ON");  // Log the action
        addMessage("Room 2 assigned");  // User feedback
        updateDisplay();  // Update display with new status
      }
      else {
        // relay 2 is already taken - provide feedback
        addMessage("Room 2 occupied");
        showAlert("Room 2 is already", "occupied");  // Show alert on display
        
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
    addMessage("Access denied");
    
    // Check if all rooms are occupied - additional user feedback
    if (relay1On && relay2On) {
      addMessage("All rooms occupied");
      showAlert("ACCESS DENIED", "All rooms occupied");  // Show alert on display
    }
    else {
      showAlert("ACCESS DENIED", "Unauthorized card");  // Show alert on display
      
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

  // Halt PICC and stop encryption - proper RFID card handling
  mfrc522.PICC_HaltA();  // Halts communication with the card
  mfrc522.PCD_StopCrypto1();  // Stops the encryption on the PCD

  // Small delay to prevent multiple reads of the same card - debounce mechanism
  delay(1000);  // Wait 1 second before scanning for a new card
}