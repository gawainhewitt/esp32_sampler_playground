#include "audio.h"
#include "debug_config.h"

// Audio system objects
AudioOutputI2S *i2s = nullptr;
AudioOutputMixer *mixer = nullptr;
AudioOutputMixerStub *mp3Channel = nullptr;
AudioOutputMixerStub *wavChannel = nullptr;

// MP3 background music (continuous)
AudioFileSourceSD *mp3FileSource = nullptr;
AudioFileSourceBuffer *mp3Buffer = nullptr;
AudioGeneratorMP3 *mp3Generator = nullptr;

// WAV sound effects (triggered)
AudioFileSourceSD *wavFileSource = nullptr;
AudioFileSourceBuffer *wavBuffer = nullptr;
AudioGeneratorWAV *wavGenerator = nullptr;

// Timing and state
unsigned long lastWavTime = 0;
const unsigned long WAV_INTERVAL = 8000; // Play WAV every 8 seconds
bool mp3Playing = false;
bool wavPlaying = false;

// Task handles for dual-core processing
TaskHandle_t audioTaskHandle;

void verifyAudioFiles() {
    // Verify audio files exist
    if (!SD.exists(MP3_FILE)) {
        debug_printf("MP3 file %s not found!\n", MP3_FILE);
        while(1);
    }
    if (!SD.exists(WAV_FILE)) {
        debug_printf("WAV file %s not found!\n", WAV_FILE);
        while(1);
    }
    
    debug_printf("Audio files verified \n");
}

void initializeAudioSystem() {
  debug_printf("Initializing audio system... \n");
  
  // Create I2S output
  i2s = new AudioOutputI2S();
  i2s->SetPinout(I2S_BCLK_PIN, I2S_WCLK_PIN, I2S_DOUT_PIN);
  i2s->SetGain(0.2); // Master volume
  
  // Create mixer with larger buffer for smoother mixing
  mixer = new AudioOutputMixer(MIXER_BUFFER_SIZE, i2s);
  
  // Create separate channels for MP3 and WAV
  mp3Channel = mixer->NewInput();
  mp3Channel->SetGain(0.6); // Background music volume
  
  wavChannel = mixer->NewInput();
  wavChannel->SetGain(0.8); // Sound effects volume (louder)
  
  debug_printf("Free PSRAM after audio init: %d bytes\n", ESP.getFreePsram());
  debug_printf("Audio mixer system ready \n");
}

void startMP3Background() {
  debug_printf("Starting MP3 background with large PSRAM buffer... \n");
  
  // Create MP3 file source
  mp3FileSource = new AudioFileSourceSD(MP3_FILE);
  if (!mp3FileSource) {
    debug_printf("Failed to create MP3 file source \n");
    return;
  }
  
  // Create large PSRAM buffer for MP3
  mp3Buffer = new AudioFileSourceBuffer(mp3FileSource, MP3_BUFFER_SIZE);
  if (!mp3Buffer) {
    debug_printf("Failed to create MP3 PSRAM buffer \n");
    delete mp3FileSource;
    return;
  }
  
  // Create MP3 generator
  mp3Generator = new AudioGeneratorMP3();
  if (!mp3Generator) {
    debug_printf("Failed to create MP3 generator \n");
    delete mp3Buffer;
    delete mp3FileSource;
    return;
  }
  
  // Start MP3 playback
  if (mp3Generator->begin(mp3Buffer, mp3Channel)) {
    mp3Playing = true;
    debug_printf("MP3 started with %dKB PSRAM buffer\n", MP3_BUFFER_SIZE / 1024);
    debug_printf("Free PSRAM after MP3 start: %d bytes\n", ESP.getFreePsram());
  } else {
    debug_printf("Failed to start MP3 playback \n");
    cleanupMP3();
  }
}

void createAudioTask() {
    xTaskCreatePinnedToCore(
        audioProcessingTask,
        "AudioTask",
        16384,        // Large stack for audio processing
        NULL,
        5,            // High priority
        &audioTaskHandle,
        1             // Pin to Core 1
    );
}

void triggerWAVEffect() {
  if (wavPlaying) {
    debug_printf("WAV already playing, skipping trigger \n");
    return;
  }
  
  debug_printf("Triggering WAV effect with PSRAM buffer... \n");
  
  // Create WAV file source
  wavFileSource = new AudioFileSourceSD(WAV_FILE);
  if (!wavFileSource) {
    debug_printf("Failed to create WAV file source \n");
    return;
  }
  
  // Create PSRAM buffer for WAV
  wavBuffer = new AudioFileSourceBuffer(wavFileSource, WAV_BUFFER_SIZE);
  if (!wavBuffer) {
    debug_printf("Failed to create WAV PSRAM buffer \n");
    delete wavFileSource;
    return;
  }
  
  // Create WAV generator
  wavGenerator = new AudioGeneratorWAV();
  if (!wavGenerator) {
    debug_printf("Failed to create WAV generator \n");
    delete wavBuffer;
    delete wavFileSource;
    return;
  }
  
  // Start WAV playback
  if (wavGenerator->begin(wavBuffer, wavChannel)) {
    wavPlaying = true;
    debug_printf("WAV effect started with %dKB PSRAM buffer\n", WAV_BUFFER_SIZE / 1024);
  } else {
    debug_printf("Failed to start WAV playback \n");
    cleanupWAV();
  }
}

// Dedicated audio processing task running on Core 1
void audioProcessingTask(void *parameter) {
  debug_printf("Audio processing task started on Core 1 \n");
  
  while (true) {
    bool audioActive = false;
    
    // Process MP3 background music
    if (mp3Playing && mp3Generator && mp3Generator->isRunning()) {
      if (mp3Generator->loop()) {
        audioActive = true;
      } else {
        // MP3 finished, restart it
        debug_printf("MP3 finished, restarting... \n");
        restartMP3();
      }
    }
    
    // Process WAV sound effects
    if (wavPlaying && wavGenerator && wavGenerator->isRunning()) {
      if (wavGenerator->loop()) {
        audioActive = true;
      } else {
        // WAV finished, clean it up
        debug_printf("WAV effect finished \n");
        cleanupWAV();
      }
    }
    
    // If no audio is active, ensure MP3 is running
    if (!audioActive && !mp3Playing) {
      debug_printf("No audio active, restarting MP3... \n");
      startMP3Background();
    }
    
    // Small delay to prevent task watchdog issues
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void restartMP3() {
  cleanupMP3();
  vTaskDelay(100 / portTICK_PERIOD_MS); // Brief pause
  startMP3Background();
}

void cleanupMP3() {
  if (mp3Generator) {
    mp3Generator->stop();
    delete mp3Generator;
    mp3Generator = nullptr;
  }
  if (mp3Buffer) {
    delete mp3Buffer;
    mp3Buffer = nullptr;
  }
  if (mp3FileSource) {
    delete mp3FileSource;
    mp3FileSource = nullptr;
  }
  mp3Playing = false;
}

void cleanupWAV() {
  if (wavGenerator) {
    wavGenerator->stop();
    delete wavGenerator;
    wavGenerator = nullptr;
  }
  if (wavBuffer) {
    delete wavBuffer;
    wavBuffer = nullptr;
  }
  if (wavFileSource) {
    delete wavFileSource;
    wavFileSource = nullptr;
  }
  wavPlaying = false;
}
