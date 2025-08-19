#include "debug_config.h"
#include <Arduino.h>

void debugInit() {
    if (ENABLE_SERIAL_DEBUG) {
        Serial.begin(115200);
        delay(1000);
        Serial.println("Starting Audio project...");
    }
}

void debug_printf(const char* format, ...) {
    if (ENABLE_SERIAL_DEBUG) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}