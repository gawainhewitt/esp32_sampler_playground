#include "sample_loader.h"
#include "../config.h"
#include "../debug.h"
#include "FS.h"
#include "SD_MMC.h"
#include "sd_manager.h"

Sample samples[MAX_SAMPLES];

bool loadSampleFromSD(const char* filename, uint8_t midiNote) {
    if (loadedSamples >= MAX_SAMPLES) {
        DEBUGF("Cannot load more samples (max %d)\n", MAX_SAMPLES);
        return false;
    }

    // Wake up SD card before loading
    if (!wakeUpSDCardWithRetry(2)) {
        DEBUG("Failed to wake up SD card, aborting sample load");
        return false;
    }

    String filepath = String(filename);
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }

    DEBUGF("Attempting to load: %s\n", filepath.c_str());

    // Multiple retry attempts with delays
    File file;
    int maxRetries = 5;
    int retryDelay = 100;
    
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        if (attempt > 0) {
            DEBUGF("Retry attempt %d for %s\n", attempt + 1, filepath.c_str());
            delay(retryDelay * attempt); // Increasing delay
        }
        
        file = SD_MMC.open(filepath.c_str());
        if (file) {
            DEBUGF("Successfully opened %s on attempt %d\n", filepath.c_str(), attempt + 1);
            break;
        }
        
        // Try to reinitialize SD card if multiple failures
        if (attempt == 2) {
            DEBUG("Attempting SD card reset...");
            SD_MMC.end();
            delay(500);
            SD_MMC.begin("/sdcard", false); // Reinitialize
            delay(200);
        }
    }
    
    if (!file) {
        DEBUGF("Failed to open file after %d attempts: %s\n", maxRetries, filepath.c_str());
        return false;
    }

    // Check file size before proceeding
    size_t fileSize = file.size();
    if (fileSize < sizeof(WAVHeader)) {
        DEBUGF("File too small to be valid WAV: %s (%d bytes)\n", filepath.c_str(), fileSize);
        file.close();
        return false;
    }

    // Read WAV header with error checking
    WAVHeader header;
    size_t headerBytesRead = 0;
    
    for (int attempt = 0; attempt < 3; attempt++) {
        file.seek(0);
        headerBytesRead = file.read((uint8_t*)&header, sizeof(header));
        
        if (headerBytesRead == sizeof(header)) {
            break;
        }
        
        DEBUGF("Header read attempt %d: got %d bytes, expected %d\n", 
               attempt + 1, headerBytesRead, sizeof(header));
        delay(50);
    }
    
    if (headerBytesRead != sizeof(header)) {
        DEBUG("Failed to read WAV header after multiple attempts");
        file.close();
        return false;
    }

    // Verify WAV format
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        DEBUGF("Invalid WAV file format: %s\n", filepath.c_str());
        DEBUGF("RIFF: %.4s, WAVE: %.4s\n", header.riff, header.wave);
        file.close();
        return false;
    }

    DEBUGF("WAV Header - Channels: %d, Sample Rate: %d, Bits: %d\n", 
           header.numChannels, header.sampleRate, header.bitsPerSample);

    // Find data chunk with robust searching
    char chunkId[5] = {0}; // Null-terminated for safety
    uint32_t chunkSize;
    bool dataFound = false;
    int searchAttempts = 0;
    const int maxSearchAttempts = 50; // Prevent infinite loops
    
    while (file.available() >= 8 && !dataFound && searchAttempts < maxSearchAttempts) {
        size_t idBytesRead = file.read((uint8_t*)chunkId, 4);
        size_t sizeBytesRead = file.read((uint8_t*)&chunkSize, 4);
        
        if (idBytesRead != 4 || sizeBytesRead != 4) {
            DEBUG("Failed to read chunk header");
            break;
        }
        
        chunkId[4] = 0; // Ensure null termination
        DEBUGF("Found chunk: '%.4s' size: %d\n", chunkId, chunkSize);
        
        if (strncmp(chunkId, "data", 4) == 0) {
            dataFound = true;
            DEBUGF("Data chunk found, size: %d bytes\n", chunkSize);
        } else {
            // Skip this chunk
            if (file.position() + chunkSize > fileSize) {
                DEBUG("Chunk size exceeds file bounds, stopping search");
                break;
            }
            file.seek(file.position() + chunkSize);
        }
        searchAttempts++;
    }

    if (!dataFound) {
        DEBUGF("Data chunk not found in %s after %d chunks\n", filepath.c_str(), searchAttempts);
        file.close();
        return false;
    }

    // Validate data chunk size
    if (chunkSize == 0 || chunkSize > fileSize) {
        DEBUGF("Invalid data chunk size: %d (file size: %d)\n", chunkSize, fileSize);
        file.close();
        return false;
    }

    // Calculate sample count and validate
    uint32_t bytesPerSample = (header.bitsPerSample / 8) * header.numChannels;
    if (bytesPerSample == 0) {
        DEBUG("Invalid bytes per sample calculation");
        file.close();
        return false;
    }
    
    uint32_t sampleCount = chunkSize / bytesPerSample;
    DEBUGF("Sample count: %d, bytes per sample: %d\n", sampleCount, bytesPerSample);

    // Allocate memory (prefer PSRAM)
    size_t allocSize = sampleCount * 2 * sizeof(int16_t); // Always stereo
    int16_t* sampleData = (int16_t*)ps_malloc(allocSize);
    
    if (!sampleData) {
        // Fallback to regular malloc
        sampleData = (int16_t*)malloc(allocSize);
        if (!sampleData) {
            DEBUGF("Failed to allocate %d bytes for sample: %s\n", allocSize, filename);
            file.close();
            return false;
        }
        DEBUG("Using regular RAM (PSRAM allocation failed)");
    } else {
        DEBUG("Using PSRAM for sample storage");
    }

    // Read and convert sample data with progress indication
    bool readSuccess = true;
    DEBUGF("Reading %d samples...\n", sampleCount);
    
    if (header.numChannels == 1) {
        // Mono - read and duplicate to stereo
        for (uint32_t i = 0; i < sampleCount && readSuccess; i++) {
            int16_t monoSample;
            if (file.read((uint8_t*)&monoSample, sizeof(int16_t)) == sizeof(int16_t)) {
                sampleData[i * 2] = monoSample;       // Left
                sampleData[i * 2 + 1] = monoSample;   // Right
            } else {
                DEBUGF("Read error at sample %d\n", i);
                readSuccess = false;
            }
            
            // Progress indicator for large files
            if (sampleCount > 0 && i % 22050 == 0 && i > 0) { // Every 0.5 seconds worth
                DEBUGF("Progress: %d%%\n", (i * 100) / sampleCount);
                delay(1); // Yield to other tasks
            }
        }
    } else {
        // Stereo - read directly with chunked approach
        const uint32_t chunkSize = 1024; // Read in chunks to handle large files
        uint32_t samplesRead = 0;
        
        while (samplesRead < sampleCount && readSuccess) {
            uint32_t samplesToRead = min(chunkSize, sampleCount - samplesRead);
            uint32_t bytesToRead = samplesToRead * 2 * sizeof(int16_t);
            
            size_t bytesActuallyRead = file.read((uint8_t*)&sampleData[samplesRead * 2], bytesToRead);
            
            if (bytesActuallyRead == bytesToRead) {
                samplesRead += samplesToRead;
                
                // Progress indicator
                if (sampleCount > 0 && samplesRead % 22050 == 0 && samplesRead > 0) {
                    DEBUGF("Progress: %d%%\n", (samplesRead * 100) / sampleCount);
                    delay(1);
                }
            } else {
                DEBUGF("Read error: expected %d bytes, got %d\n", bytesToRead, bytesActuallyRead);
                readSuccess = false;
            }
        }
    }

    file.close();

    if (!readSuccess) {
        free(sampleData);
        DEBUG("Failed to read sample data completely");
        return false;
    }

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

    DEBUGF("✓ Loaded sample: %s -> MIDI note %d (%d samples, %d Hz)\n", 
           filename, midiNote, sampleCount, header.sampleRate);

    return true;
}