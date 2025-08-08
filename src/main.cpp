#include <Arduino.h>
#include "sd_card.h"
#include "audio.h"
#include "debug_config.h"

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
  
  
  debug_printf("System ready!");
  debug_printf("MP3 playing continuously, WAV effects every 8 seconds");
  
}

void loop() {
  // Main loop handles timing and triggers
  unsigned long currentTime = millis();
  
  // Trigger WAV effect periodically
  if (!wavPlaying && (currentTime - lastWavTime) >= WAV_INTERVAL) {
    triggerWAVEffect();
    lastWavTime = currentTime;
  }
  
  // Monitor system health
  static unsigned long lastHealthCheck = 0;
  if (currentTime - lastHealthCheck >= 10000) { // Every 10 seconds
    debug_printf("Free PSRAM: %d bytes, Free Heap: %d bytes\n", 
                  ESP.getFreePsram(), ESP.getFreeHeap());
    lastHealthCheck = currentTime;
  }
  
  delay(100); // Main loop doesn't need to be fast
}


