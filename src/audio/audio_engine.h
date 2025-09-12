#pragma once

#include <Arduino.h>
#include "../audio/voice.h"
#include "../storage/sample.h"

extern Voice voices[];
extern TaskHandle_t audioTask;

void initVoices();
Voice* getFreeVoice();
void noteOn(uint8_t midiNote, uint8_t velocity);
void noteOff(uint8_t midiNote);
void processVoice(Voice& voice, int16_t& leftOut, int16_t& rightOut);
void audioTaskCode(void* parameter);
void setSampleVolume(float volume);