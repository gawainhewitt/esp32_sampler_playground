#include "sample_loader.h"
#include "../config.h"
#include "../debug.h"
#include "FS.h"
#include "SD.h"

Sample samples[MAX_SAMPLES];

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