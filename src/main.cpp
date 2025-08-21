#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "audio/i2s_manager.h"
#include "audio/audio_engine.h"
#include "storage/sd_manager.h"
#include "storage/sample_loader.h"
#include "midi/midi_handler.h"
#include "utils/serial_commands.h"

// Global variables (defined here)
float sampleVolume = 1.0f;
int loadedSamples = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DEBUG("ESP32-S3 RAM Sampler Starting...");

    // Initialize hardware
    initI2S();
    initSD();
    initVoices();

    // Load samples (map your WAV files to MIDI notes)
    // C major scale starting at middle C (MIDI note 60)
    loadSampleFromSD("C.wav", 60);  // C4
    loadSampleFromSD("D.wav", 62);  // D4
    loadSampleFromSD("E.wav", 64);  // E4
    loadSampleFromSD("F.wav", 65);  // F4
    loadSampleFromSD("G.wav", 67);  // G4
    loadSampleFromSD("A.wav", 69);  // A4
    loadSampleFromSD("B.wav", 71);  // B4

    // Setup MIDI
    initMIDI();

    // Create audio task
    xTaskCreatePinnedToCore(
        audioTaskCode,
        "AudioTask",
        4096,
        NULL,
        1,
        &audioTask,
        0
    );

    DEBUGF("Loaded %d samples. Ready for MIDI input!\n", loadedSamples);
    DEBUG("Commands:");
    DEBUG("  play <note>     - Play sample (e.g., 'play 60')");
    DEBUG("  stop <note>     - Stop sample");
    DEBUG("  volume <0-2>    - Set sample volume");
    DEBUG("  status          - Show status");
}

void loop() {
    processMIDI();
    handleSerialCommands();
    delay(1);
}