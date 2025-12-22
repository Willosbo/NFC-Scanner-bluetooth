#include <Arduino.h>

#ifndef USE_NIMBLE
#define USE_NIMBLE
#endif

#include <NimBLEDevice.h>
#include <BleKeyboard.h>

BleKeyboard bleKeyboard("NFC_Scanner", "SeeedStudio", 100);

#define AUDIO_PIN_POSITIVE 3
#define AUDIO_PIN_NEGATIVE 4

enum State {
  TURN_ON_CARD_SEARCH_COMMAND,
  TURN_ON_CARD_SEARCH_CONFIRM,
  SEARCH_FOR_CARD,
  GET_SERIAL_COMMAND,
  WAIT_FOR_SERIAL,
  KEYBOARD_OUTPUT,
};

State current_state = TURN_ON_CARD_SEARCH_COMMAND;
byte uid[8]{};
byte last_uid[8]{}; 
byte serial_number[14]{};
int readable_string_length = 0;
unsigned long state_time;
unsigned long last_scan_time = 0;
const int TIMEOUT_LIMIT = 300; // More forgiving for hardware latency

void ClearSerial();
int RemoveNonReadableChars(byte *source, int source_len, byte *destination);
void AddCheckSumXOR(byte *data, int len);
void beep();

void setup() {
  setCpuFrequencyMhz(160);
  
  // XIAO ESP32-C3 Pins: 21=TX, 20=RX
  Serial1.begin(19200, SERIAL_8N1, 20, 21);
  Serial1.setTimeout(50); 
  
  pinMode(AUDIO_PIN_POSITIVE, OUTPUT);
  pinMode(AUDIO_PIN_NEGATIVE, OUTPUT);
  
  bleKeyboard.begin();
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); 
  
  delay(500);
  beep(); // Confirm boot
}

void loop() {
  State next_state = current_state;
  
  switch (current_state) {
    case TURN_ON_CARD_SEARCH_COMMAND: {
      ClearSerial();
      byte turn_on_search_command[6] = {0x00, 0x00, 0x03, 0x02, 0x03, 0x02};
      Serial1.write(turn_on_search_command, 6);
      state_time = millis();
      next_state = SEARCH_FOR_CARD; // Jump straight to searching
      break;
    }
    
    case SEARCH_FOR_CARD: {
      // Reduced delay to 20ms - just enough for the radio to settle
      if (millis() - state_time > 20) { 
        if (Serial1.available() >= 5) {
          byte res[20];
          int len = Serial1.readBytes(res, 20);
          
          for(int i = 0; i < len - 5; i++) {
            if (res[i+2] == 0x11) {
              byte current_uid[8];
              memcpy(current_uid, &res[i+5], 8);
              
              // Reduced re-scan lockout to 500ms for rapid scanning
              if (memcmp(current_uid, last_uid, 8) != 0 || (millis() - last_scan_time > 200)) {
                memcpy(uid, current_uid, 8);
                memcpy(last_uid, current_uid, 8);
                next_state = GET_SERIAL_COMMAND;
                break; 
              }
            }
          }
        }
      }
      // Faster refresh of the search command
      if ((millis() - state_time) > 800) next_state = TURN_ON_CARD_SEARCH_COMMAND;
      break;
    }
    
    case GET_SERIAL_COMMAND: {
      ClearSerial();
      byte readblocks_command[15] = {0x00, 0x00, 0x0C, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x06, 0x00};
      memcpy(readblocks_command + 4, uid, 8);
      AddCheckSumXOR(readblocks_command, 15);
      Serial1.write(readblocks_command, 15);
      state_time = millis();
      next_state = WAIT_FOR_SERIAL;
      break;
    }
    
    case WAIT_FOR_SERIAL: {
      if (Serial1.available() >= 4) {
        byte res[30];
        int len = Serial1.readBytes(res, 30);
        for(int i = 0; i < len - 4; i++) {
          if (res[i+2] == 0x23) {
            byte raw_data[14];
            memcpy(raw_data, &res[i+4], 14); 
            readable_string_length = RemoveNonReadableChars(raw_data, 14, serial_number);
            next_state = KEYBOARD_OUTPUT;
            break;
          }
        }
      }
      if ((millis() - state_time) > TIMEOUT_LIMIT) next_state = TURN_ON_CARD_SEARCH_COMMAND;
      break;
    }
    
    case KEYBOARD_OUTPUT: {
      beep(); // Buzz to show the scan was successful
      
      if (bleKeyboard.isConnected()) {
        delay(300); // Give PC/Laptop time to focus
        for(int i = 0; i < readable_string_length; i++) {
          bleKeyboard.write(serial_number[i]);
          delay(20); // Safe typing speed
        }
        bleKeyboard.write(KEY_RETURN);
      }
      
      last_scan_time = millis();
      delay(100); // Short cooldown
      next_state = TURN_ON_CARD_SEARCH_COMMAND;
      break;
    }
  }
  
  current_state = next_state;
  delay(10); // Prevent CPU "spinning" too fast
}

// --- DO NOT REMOVE THESE FUNCTIONS ---

void ClearSerial() {
  while (Serial1.available() > 0) Serial1.read();
}

int RemoveNonReadableChars(byte *source, int source_len, byte *destination) {
  int length_of_readable_chars = 0;
  for (int i = 0; i < source_len; i++) {
    if (source[i] >= 0x20 && source[i] <= 0x7E) {
      destination[length_of_readable_chars] = source[i];
      length_of_readable_chars++;
    }
  }
  return length_of_readable_chars;
}

void AddCheckSumXOR(byte *data, int len) {
  byte check_sum = 0;
  for (byte i = 0; i < len; i++) check_sum ^= data[i];
  data[len - 1] = check_sum;
}

void beep() {
  const int beepFreq = 2500;
  const int beepDuration = 150;
  int halfPeriod = 1000000 / (beepFreq * 2);
  int cycles = (beepDuration * 1000) / (halfPeriod * 2);
  for(int i = 0; i < cycles; i++) {
    digitalWrite(AUDIO_PIN_POSITIVE, HIGH);
    digitalWrite(AUDIO_PIN_NEGATIVE, LOW);
    delayMicroseconds(halfPeriod);
    digitalWrite(AUDIO_PIN_POSITIVE, LOW);
    digitalWrite(AUDIO_PIN_NEGATIVE, HIGH);
    delayMicroseconds(halfPeriod);
  }
  digitalWrite(AUDIO_PIN_POSITIVE, LOW);
  digitalWrite(AUDIO_PIN_NEGATIVE, LOW);
}
