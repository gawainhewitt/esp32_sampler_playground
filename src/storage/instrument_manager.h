#pragma once

#include <Arduino.h>
#include "instrument.h"

#define MAX_INSTRUMENTS 4

extern Instrument instruments[MAX_INSTRUMENTS];
extern int currentInstrument;
extern int loadedInstruments;

// Load a key sample into an instrument
bool loadKeySample(int instrumentIndex, const char* filename, uint8_t rootNote, uint8_t minNote, uint8_t maxNote);

// Create a new instrument
int createInstrument(const char* name);

// Select current instrument
void selectInstrument(int instrumentIndex);

// Get the current instrument
Instrument* getCurrentInstrument();

// Load a basic piano instrument (example)
void loadBasicPiano();

// Load a drum kit instrument (example)
void loadBasicDrumKit();