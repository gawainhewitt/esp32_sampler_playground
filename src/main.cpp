#include <Arduino.h>
#include "sd_card.h"
#include "audio.h"
#include "debug_config.h"
#include "constants.h"

void setup() {

  debugInit();

  delay(1000);

  debug_printf("\n=== ESP32-S3 Simultaneous Audio Player ===");
  debug_printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
  debug_printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

  setupSDCard();
  
  verifyAudioFiles(); 
  
  // Initialize audio system
  initializeAudioSystem();
  
  // Start background MP3
  startMP3Background();
  
  // Create audio processing task on Core 1 (dedicated to audio)
  createAudioTask();

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Using internal pullup resistor
  debug_printf("Button initialized on pin %d \n", BUTTON_PIN);
  
  
  debug_printf("System ready!");
  debug_printf("MP3 playing continuously, WAV effects every 8 seconds");
  
}

void loop() {
  static bool lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  static bool buttonPressed = false;
  
  bool reading = digitalRead(BUTTON_PIN);
  
  // If button state changed, reset debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // If button has been stable for 10ms, it's a real press
  if ((millis() - lastDebounceTime) > 10) {
    if (reading == LOW && !buttonPressed) {
      // Button just pressed
      buttonPressed = true;
      if (!wavPlaying) {
        triggerWAVEffect();
      }
    } else if (reading == HIGH) {
      buttonPressed = false;
    }
  }
  
  lastButtonState = reading;
  
  // Rest of your existing loop code...
  static unsigned long lastHealthCheck = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastHealthCheck >= HEALTH_CHECK_INTERVAL) {
    debug_printf("Free PSRAM: %d bytes, Free Heap: %d bytes\n", 
                  ESP.getFreePsram(), ESP.getFreeHeap());
    lastHealthCheck = currentTime;
  }
  
  delay(MAIN_LOOP_DELAY); // Shorter delay for more responsive button reading
}

