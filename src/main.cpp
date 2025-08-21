#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "audio/i2s_manager.h"
#include "audio/audio_engine.h"
#include "storage/sd_manager.h"
#include "storage/sample_loader.h"
#include "storage/instrument_manager.h"
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

    // Load instruments instead of individual samples
    DEBUG("Loading instruments...");
    
    // Load a basic piano instrument
    loadBasicPiano();
    
    // You could also load a drum kit:
    // loadBasicDrumKit();
    
    // Select the first instrument
    selectInstrument(0);

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

    DEBUGF("Loaded %d instruments with %d total samples. Ready for MIDI input!\n", 
           loadedInstruments, loadedSamples);
    
    DEBUG("Commands:");
    DEBUG("  play <note>        - Play note (e.g., 'play 60')");
    DEBUG("  stop <note>        - Stop note");
    DEBUG("  volume <0-2>       - Set sample volume");
    DEBUG("  instrument <0-3>   - Select instrument");
    DEBUG("  status             - Show status");
    DEBUG("  load piano         - Load basic piano");
    DEBUG("  load drums         - Load basic drums");
}

void loop() {
    processMIDI();
    handleSerialCommands();
    delay(1);
}