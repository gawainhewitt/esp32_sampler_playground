#include "mp3_streamer.h"
#include "../config.h"
#include "../debug.h"
#include "FS.h"
#include "SD.h"

extern "C" {
#include "libhelix-mp3/mp3dec.h"
}

// Global variables
MP3StreamBuffer mp3Buffer;
volatile bool mp3Streaming = false;
volatile float mp3Volume = 0.5f;
TaskHandle_t mp3StreamTask = NULL;

// Stream control
String currentMP3File;
File mp3File;
HMP3Decoder mp3Decoder = NULL;

bool initMP3Streamer() {
    DEBUG("Initializing MP3 streamer...");
    
    // Initialize variables first
    mp3Buffer.buffer = NULL;
    mp3Buffer.mutex = NULL;
    mp3Buffer.writePos = 0;
    mp3Buffer.readPos = 0;
    mp3Buffer.hasData = false;
    
    // Allocate circular buffer (4 seconds of 44.1kHz stereo audio)
    const size_t bufferSamples = 44100 * 2 * 4; // 4 seconds, stereo
    mp3Buffer.buffer = (int16_t*)ps_malloc(bufferSamples * sizeof(int16_t));
    if (!mp3Buffer.buffer) {
        DEBUG("Failed to allocate MP3 buffer");
        return false;
    }
    
    mp3Buffer.size = bufferSamples;
    
    // Create mutex for thread safety
    mp3Buffer.mutex = xSemaphoreCreateMutex();
    if (!mp3Buffer.mutex) {
        DEBUG("Failed to create MP3 buffer mutex");
        free(mp3Buffer.buffer);
        mp3Buffer.buffer = NULL;
        return false;
    }
    
    // Clear buffer
    memset(mp3Buffer.buffer, 0, bufferSamples * sizeof(int16_t));
    
    DEBUGF("MP3 buffer allocated: %d samples (%.1f seconds)\n", 
           bufferSamples, (float)bufferSamples / 44100.0f / 2.0f);
    DEBUGF("MP3 mutex created: %p\n", mp3Buffer.mutex);
    
    return true;
}

void startMP3Stream(const char* filename) {
    if (mp3Streaming) {
        DEBUG("Already streaming, stopping current stream...");
        stopMP3Stream();
        vTaskDelay(100 / portTICK_PERIOD_MS); // Wait for cleanup
    }
    
    // Safety check
    if (!mp3Buffer.buffer || !mp3Buffer.mutex) {
        DEBUG("MP3 streamer not properly initialized");
        return;
    }
    
    currentMP3File = filename;
    DEBUGF("Starting MP3 stream for: %s\n", filename);
    
    // Create streaming task with simpler parameters
    BaseType_t result = xTaskCreatePinnedToCore(
        mp3StreamTaskCode,
        "MP3Stream",
        8192,           // Stack size
        NULL,           // Parameters
        2,              // Priority (higher than audio task)
        &mp3StreamTask,
        1               // Core 1 (opposite from audio task on core 0)
    );
    
    if (result != pdPASS) {
        DEBUGF("Failed to create MP3 stream task, error: %d\n", result);
    } else {
        DEBUG("MP3 stream task created successfully");
    }
}

void stopMP3Stream() {
    if (!mp3Streaming) return;
    
    DEBUG("Stopping MP3 stream...");
    mp3Streaming = false;
    
    // Wait for task to finish
    if (mp3StreamTask != NULL) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
        if (mp3StreamTask != NULL) {
            vTaskDelete(mp3StreamTask);
            mp3StreamTask = NULL;
        }
    }
    
    // Reset buffer
    xSemaphoreTake(mp3Buffer.mutex, portMAX_DELAY);
    mp3Buffer.writePos = 0;
    mp3Buffer.readPos = 0;
    mp3Buffer.hasData = false;
    xSemaphoreGive(mp3Buffer.mutex);
    
    DEBUG("MP3 stream stopped");
}

void setMP3Volume(float volume) {
    mp3Volume = constrain(volume, 0.0f, 1.0f);
    DEBUGF("MP3 volume: %.2f\n", mp3Volume);
}

bool readMP3Samples(int16_t* leftOut, int16_t* rightOut) {
    if (!mp3Buffer.hasData) return false;
    
    bool success = false;
    
    xSemaphoreTake(mp3Buffer.mutex, portMAX_DELAY);
    
    if (getAvailableReadSamples() >= 2) { // Need stereo pair
        *leftOut = (int16_t)((float)mp3Buffer.buffer[mp3Buffer.readPos] * mp3Volume);
        *rightOut = (int16_t)((float)mp3Buffer.buffer[mp3Buffer.readPos + 1] * mp3Volume);
        advanceReadPos(2);
        success = true;
    }
    
    xSemaphoreGive(mp3Buffer.mutex);
    return success;
}

void mp3StreamTaskCode(void* parameter) {
    DEBUG("MP3 stream task started");
    
    // Build file path
    String filepath = currentMP3File;
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }
    
    // Open file
    mp3File = SD.open(filepath.c_str());
    if (!mp3File) {
        DEBUGF("Failed to open MP3 file: %s\n", filepath.c_str());
        mp3Streaming = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Create decoder
    mp3Decoder = MP3InitDecoder();
    if (!mp3Decoder) {
        DEBUG("Failed to create MP3 decoder in stream task");
        mp3File.close();
        mp3Streaming = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Allocate working buffers
    const size_t inputBufSize = 16384;
    uint8_t* inputBuffer = (uint8_t*)malloc(inputBufSize);
    int16_t* frameBuffer = (int16_t*)malloc(2304 * sizeof(int16_t));
    
    if (!inputBuffer || !frameBuffer) {
        DEBUG("Failed to allocate stream buffers");
        if (inputBuffer) free(inputBuffer);
        if (frameBuffer) free(frameBuffer);
        MP3FreeDecoder(mp3Decoder);
        mp3File.close();
        mp3Streaming = false;
        vTaskDelete(NULL);
        return;
    }
    
    mp3Streaming = true;
    
    // Read initial data and find sync
    size_t totalBytesRead = mp3File.read(inputBuffer, inputBufSize);
    uint8_t* readPtr = inputBuffer;
    int bytesLeft = totalBytesRead;
    bool firstFrame = true;
    
    DEBUGF("MP3 stream task running, file size: %d\n", mp3File.size());
    
    while (mp3Streaming && mp3File.available()) {
        // Find sync on first frame
        if (firstFrame) {
            int syncOffset = MP3FindSyncWord(readPtr, bytesLeft);
            if (syncOffset < 0) {
                DEBUG("No sync found in stream task");
                break;
            }
            readPtr += syncOffset;
            bytesLeft -= syncOffset;
            firstFrame = false;
        }
        
        // Refill buffer if needed
        if (bytesLeft < 2000) {
            memmove(inputBuffer, readPtr, bytesLeft);
            size_t moreBytes = mp3File.read(inputBuffer + bytesLeft, inputBufSize - bytesLeft);
            readPtr = inputBuffer;
            bytesLeft += moreBytes;
        }
        
        // Wait if circular buffer is getting full (prevent overflow)
        while (mp3Streaming && getAvailableWriteSpace() < 2304) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        
        if (!mp3Streaming) break;
        
        // Decode frame
        int err = MP3Decode(mp3Decoder, &readPtr, &bytesLeft, frameBuffer, 0);
        
        if (err == ERR_MP3_NONE) {
            MP3FrameInfo frameInfo;
            MP3GetLastFrameInfo(mp3Decoder, &frameInfo);
            
            // Write to circular buffer
            xSemaphoreTake(mp3Buffer.mutex, portMAX_DELAY);
            
            size_t samplesToWrite = min((size_t)frameInfo.outputSamps, getAvailableWriteSpace());
            for (size_t i = 0; i < samplesToWrite; i++) {
                mp3Buffer.buffer[(mp3Buffer.writePos + i) % mp3Buffer.size] = frameBuffer[i];
            }
            advanceWritePos(samplesToWrite);
            mp3Buffer.hasData = true;
            
            xSemaphoreGive(mp3Buffer.mutex);
            
        } else {
            // Try to recover by finding next sync
            int syncOffset = MP3FindSyncWord(readPtr, bytesLeft);
            if (syncOffset >= 0) {
                readPtr += syncOffset;
                bytesLeft -= syncOffset;
            } else {
                DEBUG("Stream decode error, no recovery sync found");
                break;
            }
        }
        
        vTaskDelay(1); // Yield to other tasks
    }
    
    // Cleanup - let the task clean up its own resources
    DEBUG("MP3 task cleaning up...");
    free(inputBuffer);
    free(frameBuffer);
    if (mp3Decoder) {
        MP3FreeDecoder(mp3Decoder);
        mp3Decoder = NULL;
    }
    if (mp3File) {
        mp3File.close();
    }
    
    // Reset streaming flag and clear task handle
    mp3Streaming = false;
    mp3StreamTask = NULL;
    
    DEBUG("MP3 stream task finished");
    vTaskDelete(NULL);  // Delete self
}

// Buffer management functions
size_t getAvailableReadSamples() {
    if (mp3Buffer.writePos >= mp3Buffer.readPos) {
        return mp3Buffer.writePos - mp3Buffer.readPos;
    } else {
        return (mp3Buffer.size - mp3Buffer.readPos) + mp3Buffer.writePos;
    }
}

size_t getAvailableWriteSpace() {
    size_t used = getAvailableReadSamples();
    return mp3Buffer.size - used - 1; // -1 to avoid read==write ambiguity
}

void advanceReadPos(size_t samples) {
    mp3Buffer.readPos = (mp3Buffer.readPos + samples) % mp3Buffer.size;
    if (getAvailableReadSamples() == 0) {
        mp3Buffer.hasData = false;
    }
}

void advanceWritePos(size_t samples) {
    mp3Buffer.writePos = (mp3Buffer.writePos + samples) % mp3Buffer.size;
}