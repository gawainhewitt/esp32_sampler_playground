#pragma once

#include <Arduino.h>

struct Sample {
    int16_t* data;          // Sample data in RAM
    uint32_t length;        // Length in samples (not bytes)
    uint8_t midiNote;       // MIDI note that triggers this sample
    bool isLoaded;          // Whether sample is loaded in RAM
    String filename;        // Original filename
    uint16_t sampleRate;    // Original sample rate
    uint8_t channels;       // 1 = mono, 2 = stereo
};

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