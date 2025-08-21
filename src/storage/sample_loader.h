#pragma once

#include <Arduino.h>
#include "sample.h"

extern Sample samples[];

bool loadSampleFromSD(const char* filename, uint8_t midiNote);