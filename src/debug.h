#pragma once

#include <Arduino.h>

#ifdef DEBUG_ON 
  #define DEBUG(x) Serial.println(x)
  #define DEBUGF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
  #define DEBUG(x)
  #define DEBUGF(x, ...)
#endif