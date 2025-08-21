#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "driver/i2s.h"
#include <MIDI.h>

// Configuration
#define DEBUG_ON
#define SAMPLE_RATE           44100
#define MAX_POLYPHONY         8           
#define MAX_SAMPLES           16          

// I2S pins for UDA1334A DAC
#define I2S_BCLK_PIN    42
#define I2S_DOUT_PIN    41  
#define I2S_WCLK_PIN    2

// SD Card SPI pins
#define SD_CS_PIN    10
#define SD_MOSI_PIN  11
#define SD_MISO_PIN  13
#define SD_SCK_PIN   12

// MIDI pins
#define MIDIRX_PIN      4
#define MIDITX_PIN      9

// Audio settings
#define DMA_BUF_LEN     64
#define DMA_NUM_BUF     2

// Debug macros
#ifdef DEBUG_ON 
  #define DEBUG(x) Serial.println(x)
  #define DEBUGF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
  #define DEBUG(x)
  #define DEBUGF(x, ...)
#endif

// MIDI setup
struct Serial2MIDISettings : public midi::DefaultSettings {
  static const long BaudRate = 31250;
  static const int8_t RxPin  = MIDIRX_PIN;
  static const int8_t TxPin  = MIDITX_PIN;
};

MIDI_NAMESPACE::SerialMIDI<HardwareSerial> Serial2MIDI(Serial2);
MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial, Serial2MIDISettings>> MIDI((MIDI_NAMESPACE::SerialMIDI<HardwareSerial, Serial2MIDISettings>&)Serial2MIDI);

// Sample structure
struct Sample {
    int16_t* data;          // Sample data in RAM
    uint32_t length;        // Length in samples (not bytes)
    uint8_t midiNote;       // MIDI note that triggers this sample
    bool isLoaded;          // Whether sample is loaded in RAM
    String filename;        // Original filename
    uint16_t sampleRate;    // Original sample rate
    uint8_t channels;       // 1 = mono, 2 = stereo
};

// Voice structure for polyphony
struct Voice {
    Sample* sample;         // Pointer to the sample being played
    uint32_t position;      // Current playback position
    float positionFloat;    // Precise position for pitch shifting
    bool isActive;          // Whether this voice is playing
    uint8_t midiNote;       // MIDI note for this voice
    uint8_t velocity;       // MIDI velocity
    float amplitude;        // Current amplitude
    float speed;            // Playback speed (1.0 = normal)
    
    // Simple ADSR envelope
    enum EnvState { ATTACK, DECAY, SUSTAIN, RELEASE, IDLE };
    EnvState envState;
    float envValue;
    float envTarget;
    float envRate;
    bool noteOff;
};

// Global variables
const i2s_port_t i2s_num = I2S_NUM_0;
Sample samples[MAX_SAMPLES];
Voice voices[MAX_POLYPHONY];
TaskHandle_t audioTask;
int loadedSamples = 0;
float sampleVolume = 1.0f;

// WAV header structure
struct WAVHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

void initI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = DMA_NUM_BUF,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_WCLK_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t result = i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    if (result == ESP_OK) {
        result = i2s_set_pin(i2s_num, &pin_config);
        if (result == ESP_OK) {
            i2s_zero_dma_buffer(i2s_num);
            DEBUG("I2S initialized successfully");
        }
    }
}

void initSD() {
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    if (SD.begin(SD_CS_PIN)) {
        DEBUG("SD card initialized successfully");
        DEBUGF("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
    } else {
        DEBUG("Failed to mount SD card");
    }
}

bool loadSampleFromSD(const char* filename, uint8_t midiNote) {
    if (loadedSamples >= MAX_SAMPLES) {
        DEBUGF("Cannot load more samples (max %d)\n", MAX_SAMPLES);
        return false;
    }

    String filepath = String(filename);
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }

    File file = SD.open(filepath.c_str());
    if (!file) {
        DEBUGF("Failed to open file: %s\n", filepath.c_str());
        return false;
    }

    // Read WAV header
    WAVHeader header;
    if (file.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
        DEBUG("Failed to read WAV header");
        file.close();
        return false;
    }

    // Verify WAV format
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        DEBUG("Invalid WAV file format");
        file.close();
        return false;
    }

    // Find data chunk
    char chunkId[4];
    uint32_t chunkSize;
    bool dataFound = false;
    
    while (file.available() && !dataFound) {
        file.read((uint8_t*)chunkId, 4);
        file.read((uint8_t*)&chunkSize, 4);
        
        if (strncmp(chunkId, "data", 4) == 0) {
            dataFound = true;
        } else {
            file.seek(file.position() + chunkSize);
        }
    }

    if (!dataFound) {
        DEBUG("Data chunk not found");
        file.close();
        return false;
    }

    // Calculate sample count
    uint32_t bytesPerSample = header.bitsPerSample / 8 * header.numChannels;
    uint32_t sampleCount = chunkSize / bytesPerSample;

    // Allocate memory (prefer PSRAM)
    int16_t* sampleData = (int16_t*)ps_malloc(sampleCount * 2 * sizeof(int16_t)); // Always allocate for stereo
    if (!sampleData) {
        DEBUGF("Failed to allocate memory for sample: %s\n", filename);
        file.close();
        return false;
    }

    // Read and convert sample data
    if (header.numChannels == 1) {
        // Mono - read and duplicate to stereo
        int16_t monoSample;
        for (uint32_t i = 0; i < sampleCount; i++) {
            file.read((uint8_t*)&monoSample, sizeof(int16_t));
            sampleData[i * 2] = monoSample;       // Left
            sampleData[i * 2 + 1] = monoSample;   // Right
        }
    } else {
        // Stereo - read directly
        file.read((uint8_t*)sampleData, sampleCount * 2 * sizeof(int16_t));
    }

    file.close();

    // Store sample info
    Sample& sample = samples[loadedSamples];
    sample.data = sampleData;
    sample.length = sampleCount;
    sample.midiNote = midiNote;
    sample.isLoaded = true;
    sample.filename = filename;
    sample.sampleRate = header.sampleRate;
    sample.channels = 2; // Always stereo after conversion

    loadedSamples++;

    DEBUGF("Loaded sample: %s -> MIDI note %d (%d samples, %d Hz)\n", 
           filename, midiNote, sampleCount, header.sampleRate);

    return true;
}

void initVoices() {
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        voices[i].isActive = false;
        voices[i].envState = Voice::IDLE;
        voices[i].noteOff = false;
    }
}

Voice* getFreeVoice() {
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        if (!voices[i].isActive) {
            return &voices[i];
        }
    }
    return nullptr; // No free voices
}

Sample* getSampleForNote(uint8_t midiNote) {
    for (int i = 0; i < loadedSamples; i++) {
        if (samples[i].midiNote == midiNote && samples[i].isLoaded) {
            return &samples[i];
        }
    }
    return nullptr;
}

void noteOn(uint8_t midiNote, uint8_t velocity) {
    Sample* sample = getSampleForNote(midiNote);
    if (!sample) {
        DEBUGF("No sample found for MIDI note %d\n", midiNote);
        return;
    }

    Voice* voice = getFreeVoice();
    if (!voice) {
        DEBUG("No free voices available");
        return;
    }

    // Initialize voice
    voice->sample = sample;
    voice->position = 0;
    voice->positionFloat = 0.0f;
    voice->isActive = true;
    voice->midiNote = midiNote;
    voice->velocity = velocity;
    voice->amplitude = velocity / 127.0f;
    voice->speed = 1.0f; // Normal speed
    voice->envState = Voice::ATTACK;
    voice->envValue = 0.0f;
    voice->envTarget = 1.0f;
    voice->envRate = 0.01f; // Fast attack
    voice->noteOff = false;

    DEBUGF("Note ON: %d, velocity: %d\n", midiNote, velocity);
}

void noteOff(uint8_t midiNote) {
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        if (voices[i].isActive && voices[i].midiNote == midiNote) {
            voices[i].noteOff = true;
            voices[i].envState = Voice::RELEASE;
            voices[i].envTarget = 0.0f;
            voices[i].envRate = 0.002f; // Slow release
            DEBUGF("Note OFF: %d\n", midiNote);
        }
    }
}

void processVoice(Voice& voice, int16_t& leftOut, int16_t& rightOut) {
    if (!voice.isActive || !voice.sample) {
        return;
    }

    // Update envelope
    switch (voice.envState) {
        case Voice::ATTACK:
            voice.envValue += voice.envRate;
            if (voice.envValue >= voice.envTarget) {
                voice.envValue = voice.envTarget;
                voice.envState = Voice::SUSTAIN;
            }
            break;
        case Voice::SUSTAIN:
            if (voice.noteOff) {
                voice.envState = Voice::RELEASE;
                voice.envTarget = 0.0f;
                voice.envRate = 0.002f;
            }
            break;
        case Voice::RELEASE:
            voice.envValue -= voice.envRate;
            if (voice.envValue <= 0.0f) {
                voice.envValue = 0.0f;
                voice.isActive = false;
                voice.envState = Voice::IDLE;
                return;
            }
            break;
        default:
            break;
    }

    // Get sample data
    uint32_t pos = (uint32_t)voice.positionFloat;
    if (pos >= voice.sample->length - 1) {
        voice.isActive = false;
        return;
    }

    // Simple interpolation between samples
    float frac = voice.positionFloat - pos;
    int16_t sample1L = voice.sample->data[pos * 2];
    int16_t sample1R = voice.sample->data[pos * 2 + 1];
    int16_t sample2L = voice.sample->data[(pos + 1) * 2];
    int16_t sample2R = voice.sample->data[(pos + 1) * 2 + 1];
    
    float interpL = sample1L + frac * (sample2L - sample1L);
    float interpR = sample1R + frac * (sample2R - sample1R);

    // Apply envelope and velocity
    float gain = voice.envValue * voice.amplitude * sampleVolume;
    leftOut += (int16_t)(interpL * gain);
    rightOut += (int16_t)(interpR * gain);

    // Advance position
    voice.positionFloat += voice.speed;
}

void audioTaskCode(void* parameter) {
    const int bufferSize = DMA_BUF_LEN;
    int16_t audioBuffer[bufferSize * 2]; // Stereo
    size_t bytesWritten;

    while (true) {
        // Clear buffer
        memset(audioBuffer, 0, sizeof(audioBuffer));

        // Mix all active voices
        for (int i = 0; i < bufferSize; i++) {
            int16_t leftMix = 0, rightMix = 0;
            
            for (int v = 0; v < MAX_POLYPHONY; v++) {
                if (voices[v].isActive) {
                    processVoice(voices[v], leftMix, rightMix);
                }
            }
            
            // Prevent clipping
            audioBuffer[i * 2] = constrain(leftMix, -32767, 32767);
            audioBuffer[i * 2 + 1] = constrain(rightMix, -32767, 32767);
        }

        // Output to I2S
        i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);
        vTaskDelay(1);
    }
}

void handleNoteOn(byte channel, byte note, byte velocity) {
    noteOn(note, velocity);
}

void handleNoteOff(byte channel, byte note, byte velocity) {
    noteOff(note);
}

void setSampleVolume(float volume) {
    sampleVolume = constrain(volume, 0.0f, 2.0f); // Allow boost up to 2x
    DEBUGF("Sample volume: %.1f\n", sampleVolume);
}

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

    // You can load more samples to different MIDI notes
    // loadSampleFromSD("sample_01.wav", 36); // Kick drum
    // loadSampleFromSD("sample_02.wav", 38); // Snare
    // etc.

    // Setup MIDI
    Serial2.begin(31250, SERIAL_8N1, MIDIRX_PIN, MIDITX_PIN);
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.begin(MIDI_CHANNEL_OMNI);

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
    MIDI.read();
    
    // Handle serial commands for testing
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.startsWith("play ")) {
            int note = command.substring(5).toInt();
            if (note >= 0 && note <= 127) {
                noteOn(note, 127);
                DEBUGF("Playing MIDI note %d\n", note);
            }
        }
        else if (command.startsWith("stop ")) {
            int note = command.substring(5).toInt();
            if (note >= 0 && note <= 127) {
                noteOff(note);
                DEBUGF("Stopping MIDI note %d\n", note);
            }
        }
        else if (command.startsWith("volume ")) {
            float vol = command.substring(7).toFloat();
            setSampleVolume(vol);
        }
        else if (command == "status") {
            DEBUGF("Loaded samples: %d\n", loadedSamples);
            int activeVoices = 0;
            for (int i = 0; i < MAX_POLYPHONY; i++) {
                if (voices[i].isActive) activeVoices++;
            }
            DEBUGF("Active voices: %d\n", activeVoices);
            DEBUGF("Sample volume: %.1f\n", sampleVolume);
        }
    }
    
    delay(1);
}