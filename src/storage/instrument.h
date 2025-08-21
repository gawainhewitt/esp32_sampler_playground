#pragma once

#include <Arduino.h>
#include "../config.h"
#include "sample.h"

struct KeySample {
    Sample* sample;          // Pointer to the actual sample data
    uint8_t rootNote;        // The MIDI note this sample was recorded at
    uint8_t minNote;         // Minimum MIDI note this sample should cover
    uint8_t maxNote;         // Maximum MIDI note this sample should cover
    bool isLoaded;           // Whether this key sample is loaded
};

struct Instrument {
    String name;             // Instrument name
    KeySample keySamples[MAX_SAMPLES];  // Array of key samples
    int numKeySamples;       // Number of loaded key samples
    bool isLoaded;           // Whether this instrument is loaded
};

// Function to calculate pitch ratio from semitone difference
float calculatePitchRatio(int semitoneOffset);

// Function to find the best key sample for a given MIDI note
KeySample* findBestKeySample(Instrument* instrument, uint8_t midiNote);