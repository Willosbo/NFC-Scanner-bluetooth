
/* 
 * SETUP INSTRUCTIONS:
 * 
 * 1. Install the "ESP32 BLE Keyboard" library by T-vK
 *    In Arduino IDE: Sketch -> Include Library -> Manage Libraries
 *    Search for "ESP32 BLE Keyboard" and install it
 * 
 * 2. Select board: "ESP32C3 Dev Module" or "XIAO_ESP32C3"
 * 
 * 3. Connect the ESP32C3 to your device:
 *    - On Windows: Settings -> Bluetooth & Devices -> Add Device
 *    - On Mac: System Preferences -> Bluetooth
 *    - On Linux: Bluetooth settings
 *    - Look for "ESP32C3 Keyboard" and pair
 * 
 * BLUETOOTH KEYBOARD USAGE:
 * 
 * Check connection:
 * - bleKeyboard.isConnected()  - Returns true if connected
 * 
 * Basic text typing:
 * - bleKeyboard.print("text")     - Types text without newline
 * - bleKeyboard.println("text")   - Types text with Enter key
 * - bleKeyboard.write('a')        - Types a single character
 * 
 * Key press and release:
 * - bleKeyboard.press(key)        - Press and hold a key
 * - bleKeyboard.release(key)      - Release a specific key
 * - bleKeyboard.releaseAll()      - Release all keys
 * 
 * Special Keys:
 * - KEY_LEFT_CTRL, KEY_RIGHT_CTRL
 * - KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT
 * - KEY_LEFT_ALT, KEY_RIGHT_ALT
 * - KEY_LEFT_GUI, KEY_RIGHT_GUI (Windows/CMD key)
 * - KEY_UP_ARROW, KEY_DOWN_ARROW, KEY_LEFT_ARROW, KEY_RIGHT_ARROW
 * - KEY_BACKSPACE, KEY_TAB, KEY_RETURN (Enter), KEY_ESC
 * - KEY_INSERT, KEY_DELETE, KEY_PAGE_UP, KEY_PAGE_DOWN
 * - KEY_HOME, KEY_END, KEY_CAPS_LOCK
 * - KEY_F1 through KEY_F24
 * 
 * Media Keys:
 * - KEY_MEDIA_PLAY_PAUSE
 * - KEY_MEDIA_NEXT_TRACK
 * - KEY_MEDIA_PREVIOUS_TRACK
 * - KEY_MEDIA_VOLUME_UP
 * - KEY_MEDIA_VOLUME_DOWN
 * - KEY_MEDIA_MUTE
 * 
 * Example - Ctrl+C (Copy):
 * bleKeyboard.press(KEY_LEFT_CTRL);
 * bleKeyboard.press('c');
 * delay(100);
 * bleKeyboard.releaseAll();
 * 
 * Battery Level (optional):
 * bleKeyboard.setBatteryLevel(85); // Set battery level 0-100%
 */

#include <Arduino.h>
#include <BleKeyboard.h>

// Create Bluetooth Keyboard with custom name
BleKeyboard bleKeyboard("NFC_SCANNER", "SEEED", 100);

bool wasConnected = false;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 10000; // 10 seconds

// Ultra-reliable typing with forced key releases
void typeReliablySafe(const char* text, int charDelay = 15) {
  // First, ensure all keys are released before starting
  bleKeyboard.releaseAll();
  delay(50);
  
  for(int i = 0; text[i] != '\0'; i++) {
    // Check if still connected before each character
    if(!bleKeyboard.isConnected()) {
      Serial.println("Connection lost during typing!");
      bleKeyboard.releaseAll(); // Emergency release
      return;
    }
    
    // Write character and immediately release
    bleKeyboard.press(text[i]);
    delay(charDelay);
    bleKeyboard.release(text[i]);
    delay(charDelay); // Double delay for reliability
  }
  
  // Final safety release
  bleKeyboard.releaseAll();
  delay(50);
}

// Safe Enter key press
void pressEnterSafe() {
  if(!bleKeyboard.isConnected()) return;
  
  bleKeyboard.press(KEY_RETURN);
  delay(50);
  bleKeyboard.release(KEY_RETURN);
  delay(50);
  bleKeyboard.releaseAll(); // Extra safety
  delay(100);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Keyboard!");
  
  // Start Bluetooth Keyboard
  bleKeyboard.begin();
  
  // Wait for connection
  Serial.println("Waiting for connection...");
}

void loop() {
  bool isConnected = bleKeyboard.isConnected();
  
  // Detect new connection
  if(isConnected && !wasConnected) {
    Serial.println("Device connected!");
    wasConnected = true;
    lastSendTime = millis();
    
    // Wait for connection to stabilize
    delay(3000); // Increased to 3 seconds
    
    // Emergency release all keys on new connection
    bleKeyboard.releaseAll();
    delay(100);
    
    // Send greeting only once per connection
    Serial.println("Sending hello message...");
    typeReliablySafe("Hello from ESP32C3 Bluetooth Keyboard!");
    pressEnterSafe();
    Serial.println("Hello message sent successfully");
  }
  
  // Detect disconnection
  if(!isConnected && wasConnected) {
    Serial.println("Device disconnected!");
    wasConnected = false;
    
    // Emergency release all keys on disconnect
    bleKeyboard.releaseAll();
  }
  
  // Only send data when connected and enough time has passed
  if(isConnected && wasConnected) {
    unsigned long currentTime = millis();
    
    // Check if it's time to send
    if(currentTime - lastSendTime >= SEND_INTERVAL) {
      // Double-check connection before sending
      if(bleKeyboard.isConnected()) {
        Serial.println("Sending serial number...");
        
        // Emergency release before typing
        bleKeyboard.releaseAll();
        delay(50);
        
        typeReliablySafe("Serial Number ", 15);
        pressEnterSafe();
        
        Serial.println("Serial number sent successfully");
        lastSendTime = currentTime;
      } else {
        Serial.println("Connection lost, skipping send");
      }
    }
    
    delay(100); // Small delay in loop
  } else {
    // Not connected, just wait
    delay(1000);
  }
}

/* 
 * SIGNAL SAFETY FEATURES ADDED:
 * 
 * 1. Explicit press() and release() for each character
 *    - Prevents keys getting stuck in pressed state
 *    - Each character is guaranteed to release
 * 
 * 2. Double delays (charDelay between press and release, then another after)
 *    - Gives BLE stack time to process packets
 *    - Prevents buffer overflow
 * 
 * 3. Connection checking during typing
 *    - Aborts mid-typing if connection drops
 *    - Emergency releaseAll() if disconnection detected
 * 
 * 4. releaseAll() called at critical points:
 *    - Before starting any text
 *    - After completing any text
 *    - On new connection
 *    - On disconnection
 *    - After every Enter key
 * 
 * 5. Longer stabilization delay (3 seconds after connection)
 * 
 * 6. Time-based sending instead of delay()
 *    - Prevents issues if loop gets delayed
 * 
 * RECOMMENDATIONS FOR MOVING AROUND:
 * - Stay within 5-10 meters of the paired device
 * - Avoid walls, metal objects between devices
 * - Don't send data while moving - wait until stationary
 * - If you need longer range, increase charDelay to 20-30ms
 * 
 * FOR NFC USE:
 * Replace the timed sending with your NFC trigger:
 * 
 * if(nfcCardDetected && bleKeyboard.isConnected()) {
 *   bleKeyboard.releaseAll();
 *   delay(50);
 *   typeReliablySafe(nfcData, 15);
 *   pressEnterSafe();
 * }
 */
/* 
 * FIXED RELIABILITY ISSUES:
 * - Added typeReliably() function that sends ONE character at a time
 * - 10ms delay between each character prevents packet loss
 * - 100ms delay after Enter key for better stability
 * - Prevents character drops like "Seria Number" or "Serial NSerial"
 * 
 * TUNING THE DELAYS:
 * If still getting errors, increase charDelay:
 * - typeReliably("text", 15); // 15ms between chars (slower but more reliable)
 * - typeReliably("text", 20); // 20ms between chars (very reliable)
 * - typeReliably("text", 5);  // 5ms between chars (faster but less reliable)
 * 
 * For NFC scanner use:
 * 
 * if(nfcDataAvailable) {
 *   typeReliably(nfcSerialNumber, 10);
 *   delay(50);
 *   bleKeyboard.write(KEY_RETURN);
 *   delay(100);
 * }
 * 
 * ADDITIONAL TIPS:
 * - Keep strings under 50 characters for best reliability
 * - For longer data, add delays every 20-30 characters
 * - Some devices work better with 15-20ms character delays
 */

/* 
 * IMPROVED RELIABILITY FEATURES:
 * - Increased initial connection delay to 2000ms
 * - Using print() + write(KEY_RETURN) instead of println()
 * - Added 50ms delays between text and Enter key
 * - Proper timing between BLE operations
 * 
 * For sending NFC data reliably, use this pattern:
 * 
 * bleKeyboard.print("Your NFC data here");
 * delay(50);
 * bleKeyboard.write(KEY_RETURN);
 * 
 * For key combinations (like Tab between fields):
 * bleKeyboard.print("Field1");
 * delay(50);
 * bleKeyboard.write(KEY_TAB);
 * delay(50);
 * bleKeyboard.print("Field2");
 * delay(50);
 * bleKeyboard.write(KEY_RETURN);
 */
