#include "instrument.h"

float calculatePitchRatio(int semitoneOffset) {
    // Calculate pitch ratio using 12-tone equal temperament
    // Each semitone is 2^(1/12) ratio
    return pow(2.0f, semitoneOffset / 12.0f);
}

KeySample* findBestKeySample(Instrument* instrument, uint8_t midiNote) {
    if (!instrument || !instrument->isLoaded) {
        return nullptr;
    }
    
    // First, look for a key sample that covers this note directly
    for (int i = 0; i < instrument->numKeySamples; i++) {
        KeySample* ks = &instrument->keySamples[i];
        if (ks->isLoaded && midiNote >= ks->minNote && midiNote <= ks->maxNote) {
            return ks;
        }
    }
    
    // If no direct match, find the closest key sample
    KeySample* closest = nullptr;
    int smallestDistance = 128;
    
    for (int i = 0; i < instrument->numKeySamples; i++) {
        KeySample* ks = &instrument->keySamples[i];
        if (ks->isLoaded) {
            int distance = abs((int)midiNote - (int)ks->rootNote);
            if (distance < smallestDistance) {
                smallestDistance = distance;
                closest = ks;
            }
        }
    }
    
    return closest;
}