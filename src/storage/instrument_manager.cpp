#include "instrument_manager.h"
#include "../config.h"
#include "../debug.h"
#include "sample_loader.h"
#include "sd_manager.h"
#include <math.h>

Instrument instruments[MAX_INSTRUMENTS];
int currentInstrument = 0;
int loadedInstruments = 0;

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

bool loadKeySample(int instrumentIndex, const char* filename, uint8_t rootNote, uint8_t minNote, uint8_t maxNote) {
    if (instrumentIndex >= MAX_INSTRUMENTS || !instruments[instrumentIndex].isLoaded) {
        DEBUG("Invalid instrument index");
        return false;
    }
    
    Instrument* instrument = &instruments[instrumentIndex];
    
    if (instrument->numKeySamples >= MAX_SAMPLES) {
        DEBUGF("Instrument %s is full (max %d samples)\n", instrument->name.c_str(), MAX_SAMPLES);
        return false;
    }
    
    // Load the sample using the existing sample loader
    if (!loadSampleFromSD(filename, rootNote)) {
        DEBUGF("Failed to load sample %s\n", filename);
        return false;
    }
    
    // Find the sample that was just loaded
    Sample* sample = nullptr;
    for (int i = 0; i < loadedSamples; i++) {
        if (samples[i].midiNote == rootNote && samples[i].filename == filename) {
            sample = &samples[i];
            break;
        }
    }
    
    if (!sample) {
        DEBUG("Could not find loaded sample");
        return false;
    }
    
    // Add to instrument's key samples
    KeySample* ks = &instrument->keySamples[instrument->numKeySamples];
    ks->sample = sample;
    ks->rootNote = rootNote;
    ks->minNote = minNote;
    ks->maxNote = maxNote;
    ks->isLoaded = true;
    
    instrument->numKeySamples++;
    
    DEBUGF("Loaded key sample: %s -> root=%d, range=%d-%d\n", filename, rootNote, minNote, maxNote);
    
    return true;
}

int createInstrument(const char* name) {
    if (loadedInstruments >= MAX_INSTRUMENTS) {
        DEBUG("Cannot create more instruments");
        return -1;
    }
    
    Instrument* instrument = &instruments[loadedInstruments];
    instrument->name = name;
    instrument->numKeySamples = 0;
    instrument->isLoaded = true;
    
    // Initialize key samples
    for (int i = 0; i < MAX_SAMPLES; i++) {
        instrument->keySamples[i].isLoaded = false;
        instrument->keySamples[i].sample = nullptr;
    }
    
    DEBUGF("Created instrument: %s (index %d)\n", name, loadedInstruments);
    
    return loadedInstruments++;
}

void selectInstrument(int instrumentIndex) {
    if (instrumentIndex >= 0 && instrumentIndex < loadedInstruments) {
        currentInstrument = instrumentIndex;
        DEBUGF("Selected instrument: %s\n", instruments[currentInstrument].name.c_str());
    }
}

Instrument* getCurrentInstrument() {
    if (currentInstrument >= 0 && currentInstrument < loadedInstruments) {
        return &instruments[currentInstrument];
    }
    return nullptr;
}

void loadBasicPiano() {
    int pianoIndex = createInstrument("Basic Piano");
    if (pianoIndex == -1) return;

    // Wake up SD card before loading samples
    if (!wakeUpSDCardWithRetry()) {
        DEBUG("Failed to wake up SD card for piano loading");
        return;
    }
    
    // Load key samples with appropriate ranges
    // Low range
    loadKeySample(pianoIndex, "piano_C2.wav", 36, 24, 42);    // C2, covers C1-F#2
    loadKeySample(pianoIndex, "piano_C3.wav", 48, 43, 54);    // C3, covers G2-F#3
    loadKeySample(pianoIndex, "piano_C4.wav", 60, 55, 66);    // C4 (middle C), covers G3-F#4
    loadKeySample(pianoIndex, "piano_C5.wav", 72, 67, 78);    // C5, covers G4-F#5
    loadKeySample(pianoIndex, "piano_C6.wav", 84, 79, 96);    // C6, covers G5-C7
    
    DEBUGF("Loaded Basic Piano instrument with %d key samples\n", instruments[pianoIndex].numKeySamples);
}

void loadBasicDrumKit() {
    int drumIndex = createInstrument("Basic Drums");
    if (drumIndex == -1) return;

    // Wake up SD card before loading samples
    if (!wakeUpSDCard()) {
        DEBUG("Failed to wake up SD card for drum kit loading");
        return;
    }
    
    // Load drum samples to specific MIDI notes (GM drum map)
    loadKeySample(drumIndex, "kick.wav", 36, 36, 36);        // Bass Drum 1
    loadKeySample(drumIndex, "snare.wav", 38, 38, 38);       // Acoustic Snare
    loadKeySample(drumIndex, "hihat_closed.wav", 42, 42, 42); // Closed Hi Hat
    loadKeySample(drumIndex, "hihat_open.wav", 46, 46, 46);   // Open Hi Hat
    loadKeySample(drumIndex, "crash.wav", 49, 49, 49);       // Crash Cymbal 1
    loadKeySample(drumIndex, "ride.wav", 51, 51, 51);        // Ride Cymbal 1
    
    DEBUGF("Loaded Basic Drums instrument with %d key samples\n", instruments[drumIndex].numKeySamples);
}