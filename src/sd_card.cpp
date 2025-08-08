#include "sd_card.h"
#include "debug_config.h"

void setupSDCard() {
    // Initialize SD card with high-speed SPI
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
    if (!SD.begin(SD_CS_PIN, SPI, 25000000)) { // 25MHz SPI for fast SD access
        debug_printf("SD Card initialization failed!");
        while (1);
    }
    
    debug_printf("SD Card initialized");
}