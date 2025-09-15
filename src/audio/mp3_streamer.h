#pragma once

#include <Arduino.h>

// Circular buffer for decoded MP3 audio
struct MP3StreamBuffer {
    int16_t* buffer;           // Audio data (stereo interleaved)
    volatile size_t writePos;  // Where to write new data
    volatile size_t readPos;   // Where to read data for playback
    size_t size;               // Total buffer size in samples
    volatile bool hasData;     // Whether buffer contains audio data
    SemaphoreHandle_t mutex;   // Thread safety
};

// MP3 streaming control
extern MP3StreamBuffer mp3Buffer;
extern volatile bool mp3Streaming;
extern volatile float mp3Volume;
extern TaskHandle_t mp3StreamTask;

// Functions
bool initMP3Streamer();
void startMP3Stream(const char* filename);
void stopMP3Stream();
void setMP3Volume(float volume);
bool readMP3Samples(int16_t* leftOut, int16_t* rightOut); // Read one stereo sample
void mp3StreamTaskCode(void* parameter);

// Internal buffer management
size_t getAvailableReadSamples();
size_t getAvailableWriteSpace();
void advanceReadPos(size_t samples);
void advanceWritePos(size_t samples);