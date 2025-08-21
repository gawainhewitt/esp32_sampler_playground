#pragma once

#include <Arduino.h>

struct Sample; // Forward declaration

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