#pragma once

#include <Arduino.h>
#include "midi_config.h"

void initMIDI();
void processMIDI();
void handleNoteOn(byte channel, byte note, byte velocity);
void handleNoteOff(byte channel, byte note, byte velocity);