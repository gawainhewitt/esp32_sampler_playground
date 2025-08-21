#include "audio_engine.h"
#include "../config.h"
#include "../debug.h"
#include "../storage/sample_loader.h"
#include "../storage/instrument_manager.h"
#include "i2s_manager.h"

Voice voices[MAX_POLYPHONY];
TaskHandle_t audioTask;

void initVoices() {
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        voices[i].isActive = false;
        voices[i].envState = Voice::IDLE;
        voices[i].noteOff = false;
    }
}

Voice* getFreeVoice() {
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        if (!voices[i].isActive) {
            return &voices[i];
        }
    }
    return nullptr; // No free voices
}

void noteOn(uint8_t midiNote, uint8_t velocity) {
    Instrument* instrument = getCurrentInstrument();
    if (!instrument) {
        DEBUG("No instrument selected");
        return;
    }
    
    // Find the best key sample for this MIDI note
    KeySample* keySample = findBestKeySample(instrument, midiNote);
    if (!keySample || !keySample->sample) {
        DEBUGF("No suitable sample found for MIDI note %d\n", midiNote);
        return;
    }

    Voice* voice = getFreeVoice();
    if (!voice) {
        DEBUG("No free voices available");
        return;
    }

    // Calculate pitch shift ratio
    int semitoneOffset = (int)midiNote - (int)keySample->rootNote;
    float pitchRatio = calculatePitchRatio(semitoneOffset);
    
    // Initialize voice
    voice->sample = keySample->sample;
    voice->position = 0;
    voice->positionFloat = 0.0f;
    voice->isActive = true;
    voice->midiNote = midiNote;
    voice->velocity = velocity;
    voice->amplitude = velocity / 127.0f;
    voice->speed = pitchRatio;  // This is the key change - speed based on pitch
    voice->envState = Voice::ATTACK;
    voice->envValue = 0.0f;
    voice->envTarget = 1.0f;
    voice->envRate = 0.01f; // Fast attack
    voice->noteOff = false;

    DEBUGF("Note ON: %d, using sample at note %d (ratio: %.3f)\n", midiNote, keySample->rootNote, pitchRatio);
}

void noteOff(uint8_t midiNote) {
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        if (voices[i].isActive && voices[i].midiNote == midiNote) {
            voices[i].noteOff = true;
            voices[i].envState = Voice::RELEASE;
            voices[i].envTarget = 0.0f;
            voices[i].envRate = 0.002f; // Slow release
            DEBUGF("Note OFF: %d\n", midiNote);
        }
    }
}

void processVoice(Voice& voice, int16_t& leftOut, int16_t& rightOut) {
    if (!voice.isActive || !voice.sample) {
        return;
    }

    // Update envelope
    switch (voice.envState) {
        case Voice::ATTACK:
            voice.envValue += voice.envRate;
            if (voice.envValue >= voice.envTarget) {
                voice.envValue = voice.envTarget;
                voice.envState = Voice::SUSTAIN;
            }
            break;
        case Voice::SUSTAIN:
            if (voice.noteOff) {
                voice.envState = Voice::RELEASE;
                voice.envTarget = 0.0f;
                voice.envRate = 0.002f;
            }
            break;
        case Voice::RELEASE:
            voice.envValue -= voice.envRate;
            if (voice.envValue <= 0.0f) {
                voice.envValue = 0.0f;
                voice.isActive = false;
                voice.envState = Voice::IDLE;
                return;
            }
            break;
        default:
            break;
    }

    // Get sample data with bounds checking
    uint32_t pos = (uint32_t)voice.positionFloat;
    if (pos >= voice.sample->length - 1) {
        voice.isActive = false;
        return;
    }

    // Linear interpolation for smooth pitch shifting
    float frac = voice.positionFloat - pos;
    int16_t sample1L = voice.sample->data[pos * 2];
    int16_t sample1R = voice.sample->data[pos * 2 + 1];
    int16_t sample2L = voice.sample->data[(pos + 1) * 2];
    int16_t sample2R = voice.sample->data[(pos + 1) * 2 + 1];
    
    float interpL = sample1L + frac * (sample2L - sample1L);
    float interpR = sample1R + frac * (sample2R - sample1R);

    // Apply envelope and velocity
    float gain = voice.envValue * voice.amplitude * sampleVolume;
    
    // Add anti-aliasing filter for high pitch ratios (simple)
    if (voice.speed > 2.0f) {
        gain *= 0.7f; // Reduce gain for very high pitches to reduce aliasing
    }
    
    leftOut += (int16_t)(interpL * gain);
    rightOut += (int16_t)(interpR * gain);

    // Advance position by speed (pitch shift)
    voice.positionFloat += voice.speed;
}

void audioTaskCode(void* parameter) {
    const int bufferSize = DMA_BUF_LEN;
    int16_t audioBuffer[bufferSize * 2]; // Stereo
    size_t bytesWritten;

    while (true) {
        // Clear buffer
        memset(audioBuffer, 0, sizeof(audioBuffer));

        // Mix all active voices
        for (int i = 0; i < bufferSize; i++) {
            int16_t leftMix = 0, rightMix = 0;
            
            for (int v = 0; v < MAX_POLYPHONY; v++) {
                if (voices[v].isActive) {
                    processVoice(voices[v], leftMix, rightMix);
                }
            }
            
            // Prevent clipping with soft limiting
            leftMix = constrain(leftMix, -32767, 32767);
            rightMix = constrain(rightMix, -32767, 32767);
            
            audioBuffer[i * 2] = leftMix;
            audioBuffer[i * 2 + 1] = rightMix;
        }

        // Output to I2S
        i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);
        vTaskDelay(1);
    }
}

void setSampleVolume(float volume) {
    sampleVolume = constrain(volume, 0.0f, 2.0f); // Allow boost up to 2x
    DEBUGF("Sample volume: %.1f\n", sampleVolume);
}