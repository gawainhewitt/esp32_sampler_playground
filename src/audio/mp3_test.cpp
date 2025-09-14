#include "mp3_test.h"
#include "../config.h"
#include "../debug.h"
#include "FS.h"
#include "SD.h"

extern "C" {
#include "libhelix-mp3/mp3dec.h"
}

void testMP3Decode(const char* filename) {
    DEBUG("=== MP3 Decode Test ===");
    
    // Build full path
    String filepath = String(filename);
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }
    
    DEBUGF("Opening file: %s\n", filepath.c_str());
    
    // Open the file
    File file = SD.open(filepath.c_str());
    if (!file) {
        DEBUGF("Failed to open MP3 file: %s\n", filepath.c_str());
        return;
    }
    
    DEBUGF("File size: %d bytes\n", file.size());
    
    // Create MP3 decoder
    HMP3Decoder mp3Decoder = MP3InitDecoder();
    if (!mp3Decoder) {
        DEBUG("Failed to create MP3 decoder");
        file.close();
        return;
    }
    
    // Allocate input buffer
    const size_t inputBufSize = 8192;
    uint8_t* inputBuffer = (uint8_t*)ps_malloc(inputBufSize);
    if (!inputBuffer) {
        DEBUG("Failed to allocate input buffer");
        MP3FreeDecoder(mp3Decoder);
        file.close();
        return;
    }
    
    // Allocate output buffer (max 1152 samples per channel, stereo = 2304 samples)
    int16_t* outputBuffer = (int16_t*)ps_malloc(2304 * sizeof(int16_t));
    if (!outputBuffer) {
        DEBUG("Failed to allocate output buffer");
        free(inputBuffer);
        MP3FreeDecoder(mp3Decoder);
        file.close();
        return;
    }
    
    // Read some data from file
    size_t bytesRead = file.read(inputBuffer, inputBufSize);
    DEBUGF("Read %d bytes from file\n", bytesRead);
    
    if (bytesRead == 0) {
        DEBUG("No data read from file");
        free(inputBuffer);
        free(outputBuffer);
        MP3FreeDecoder(mp3Decoder);
        file.close();
        return;
    }
    
    // Try to find sync and decode first frame
    uint8_t* readPtr = inputBuffer;
    int bytesLeft = bytesRead;
    
    // Find MP3 sync word
    int syncOffset = MP3FindSyncWord(readPtr, bytesLeft);
    if (syncOffset < 0) {
        DEBUG("No MP3 sync word found - not a valid MP3 file");
        free(inputBuffer);
        free(outputBuffer);
        MP3FreeDecoder(mp3Decoder);
        file.close();
        return;
    }
    
    DEBUGF("Found MP3 sync at offset %d\n", syncOffset);
    readPtr += syncOffset;
    bytesLeft -= syncOffset;
    
    // Try to decode first frame
    int err = MP3Decode(mp3Decoder, &readPtr, &bytesLeft, outputBuffer, 0);
    
    if (err == ERR_MP3_NONE) {
        DEBUG("Successfully decoded first MP3 frame!");
        
        // Get frame info
        MP3FrameInfo frameInfo;
        MP3GetLastFrameInfo(mp3Decoder, &frameInfo);
        
        DEBUGF("Sample Rate: %d Hz\n", frameInfo.samprate);
        DEBUGF("Channels: %d\n", frameInfo.nChans);
        DEBUGF("Bitrate: %d kbps\n", frameInfo.bitrate);
        DEBUGF("Output samples: %d\n", frameInfo.outputSamps);
        DEBUGF("Layer: %d\n", frameInfo.layer);
        DEBUGF("Version: %d\n", frameInfo.version);
        
        // Show first few decoded sample values
        DEBUG("First 10 decoded samples:");
        for (int i = 0; i < min(10, frameInfo.outputSamps); i++) {
            DEBUGF("  Sample %d: %d\n", i, outputBuffer[i]);
        }
        
        // Calculate how much input data was consumed
        int bytesConsumed = inputBufSize - syncOffset - bytesLeft;
        DEBUGF("Bytes consumed: %d\n", bytesConsumed);
        
    } else {
        DEBUGF("MP3 decode failed with error: %d\n", err);
        switch (err) {
            case ERR_MP3_INDATA_UNDERFLOW:
                DEBUG("Error: Not enough input data");
                break;
            case ERR_MP3_MAINDATA_UNDERFLOW:
                DEBUG("Error: Not enough main data");
                break;
            case ERR_MP3_FREE_BITRATE_SYNC:
                DEBUG("Error: Free bitrate sync");
                break;
            case ERR_MP3_INVALID_FRAMEHEADER:
                DEBUG("Error: Invalid frame header");
                break;
            default:
                DEBUG("Error: Unknown decode error");
                break;
        }
    }
    
    // Clean up
    free(inputBuffer);
    free(outputBuffer);
    MP3FreeDecoder(mp3Decoder);
    file.close();
    
    DEBUG("=== MP3 Test Complete ===");
}

void testMP3Stream(const char* filename) {
    DEBUG("=== MP3 Stream Test ===");
    
    // Build full path
    String filepath = String(filename);
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }
    
    DEBUGF("Opening file: %s\n", filepath.c_str());
    
    // Open the file
    File file = SD.open(filepath.c_str());
    if (!file) {
        DEBUGF("Failed to open MP3 file: %s\n", filepath.c_str());
        return;
    }
    
    DEBUGF("File size: %d bytes\n", file.size());
    
    // Create MP3 decoder
    HMP3Decoder mp3Decoder = MP3InitDecoder();
    if (!mp3Decoder) {
        DEBUG("Failed to create MP3 decoder");
        file.close();
        return;
    }
    
    // Allocate buffers
    const size_t inputBufSize = 16384;  // Larger buffer for streaming
    uint8_t* inputBuffer = (uint8_t*)ps_malloc(inputBufSize);
    int16_t* outputBuffer = (int16_t*)ps_malloc(2304 * sizeof(int16_t));
    
    if (!inputBuffer || !outputBuffer) {
        DEBUG("Failed to allocate buffers");
        if (inputBuffer) free(inputBuffer);
        if (outputBuffer) free(outputBuffer);
        MP3FreeDecoder(mp3Decoder);
        file.close();
        return;
    }
    
    // Read initial data
    size_t totalBytesRead = file.read(inputBuffer, inputBufSize);
    DEBUGF("Initial read: %d bytes\n", totalBytesRead);
    
    uint8_t* readPtr = inputBuffer;
    int bytesLeft = totalBytesRead;
    int frameCount = 0;
    int totalSamples = 0;
    bool firstFrame = true;
    
    // Stream decode loop
    const int maxFramesToDecode = 15;
    
    while (frameCount < maxFramesToDecode && bytesLeft > 0) {
        // Find sync if this is the first frame
        if (firstFrame) {
            int syncOffset = MP3FindSyncWord(readPtr, bytesLeft);
            if (syncOffset < 0) {
                DEBUG("No MP3 sync word found");
                break;
            }
            readPtr += syncOffset;
            bytesLeft -= syncOffset;
            firstFrame = false;
            DEBUGF("Found sync at offset %d\n", syncOffset);
        }
        
        // Check if we need more data
        if (bytesLeft < 1000) { // Need at least 1000 bytes for a frame
            // Move remaining data to start of buffer
            memmove(inputBuffer, readPtr, bytesLeft);
            
            // Read more data
            size_t moreBytes = file.read(inputBuffer + bytesLeft, inputBufSize - bytesLeft);
            if (moreBytes == 0) {
                DEBUG("End of file reached");
                break;
            }
            
            readPtr = inputBuffer;
            bytesLeft += moreBytes;
            DEBUGF("Refilled buffer: %d bytes available\n", bytesLeft);
        }
        
        // Decode frame
        int err = MP3Decode(mp3Decoder, &readPtr, &bytesLeft, outputBuffer, 0);
        
        if (err == ERR_MP3_NONE) {
            frameCount++;
            
            // Get frame info
            MP3FrameInfo frameInfo;
            MP3GetLastFrameInfo(mp3Decoder, &frameInfo);
            totalSamples += frameInfo.outputSamps;
            
            DEBUGF("Frame %d: %d samples, %d bytes left\n", 
                   frameCount, frameInfo.outputSamps, bytesLeft);
            
            // Show sample values from different frames to prove we're getting real audio
            if (frameCount == 1) {
                DEBUG("Frame 1 samples (start):");
                for (int i = 0; i < min(6, frameInfo.outputSamps); i++) {
                    DEBUGF("  [%d] = %d\n", i, outputBuffer[i]);
                }
            }
            else if (frameCount == 5) {
                DEBUG("Frame 5 samples (middle):");
                for (int i = 0; i < min(6, frameInfo.outputSamps); i++) {
                    DEBUGF("  [%d] = %d\n", i, outputBuffer[i]);
                }
            }
            else if (frameCount == 10) {
                DEBUG("Frame 10 samples:");
                // Show middle samples to see if there's audio variation
                int start = frameInfo.outputSamps / 2;
                for (int i = start; i < min(start + 6, frameInfo.outputSamps); i++) {
                    DEBUGF("  [%d] = %d\n", i, outputBuffer[i]);
                }
            }
            
        } else {
            DEBUGF("Decode error %d at frame %d\n", err, frameCount + 1);
            
            if (err == ERR_MP3_INDATA_UNDERFLOW) {
                DEBUG("Need more input data");
                break;
            } else {
                // Try to find next sync word to recover
                int syncOffset = MP3FindSyncWord(readPtr, bytesLeft);
                if (syncOffset >= 0) {
                    readPtr += syncOffset;
                    bytesLeft -= syncOffset;
                    DEBUGF("Found next sync at +%d, continuing\n", syncOffset);
                } else {
                    DEBUG("No more sync words found");
                    break;
                }
            }
        }
    }
    
    DEBUG("\n=== Stream Results ===\n");
    DEBUGF("Frames decoded: %d\n", frameCount);
    DEBUGF("Total samples: %d\n", totalSamples);
    DEBUGF("Duration: %.2f seconds\n", (float)totalSamples / 44100.0f / 2.0f); // stereo
    DEBUGF("Streaming: %s\n", (frameCount >= 5) ? "SUCCESS" : "NEEDS WORK");
    
    // Clean up
    free(inputBuffer);
    free(outputBuffer);
    MP3FreeDecoder(mp3Decoder);
    file.close();
    
    DEBUG("=== MP3 Stream Test Complete ===");
}