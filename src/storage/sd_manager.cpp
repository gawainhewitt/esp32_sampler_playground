#include "sd_manager.h"
#include "../config.h"
#include "../debug.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

void initSD() {
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    
    if (SD.begin(SD_CS_PIN)) {
        DEBUG("SD card initialized successfully");
        DEBUGF("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
    } else {
        DEBUG("Failed to mount SD card");
    }
}