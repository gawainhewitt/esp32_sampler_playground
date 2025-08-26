#include "sd_manager.h"
#include "../config.h"
#include "../debug.h"
#include "FS.h"
#include "SD_MMC.h"

void printCardInfo();

void initSD() {
    DEBUG("Initializing SD card...");
    
    // Option 1: Try using the pins defined in config.h
    DEBUG("Trying custom pin configuration...");
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
    
    if (SD_MMC.begin("/sdcard", false)) {  // false = 4-bit mode
        DEBUG("SD card initialized successfully with custom pins (4-bit mode)");
        DEBUGF("SD Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
        printCardInfo();
        return;
    }
    
    DEBUG("Custom pins failed, trying 1-bit mode with custom pins...");
    SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0);  // CLK, CMD, D0 only
    
    if (SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
        DEBUG("SD card initialized successfully with custom pins (1-bit mode)");
        DEBUGF("SD Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
        printCardInfo();
        return;
    }
    
    // Option 2: Try default ESP32-S3 pins (if your board uses them)
    DEBUG("Custom pins failed, trying default ESP32-S3 pins...");
    SD_MMC.setPins(39, 38, 40, 41, 42, 43);  // Standard ESP32-S3 pins
    
    if (SD_MMC.begin("/sdcard", false)) {  // false = 4-bit mode
        DEBUG("SD card initialized successfully with default pins (4-bit mode)");
        DEBUGF("SD Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
        printCardInfo();
        return;
    }
    
    DEBUG("Default pins 4-bit failed, trying 1-bit mode...");
    SD_MMC.setPins(39, 38, 40);  // CLK, CMD, D0 only
    
    if (SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
        DEBUG("SD card initialized successfully with default pins (1-bit mode)");
        DEBUGF("SD Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
        printCardInfo();
        return;
    }
    
    // Option 3: Try without setting pins (let ESP-IDF use defaults)
    DEBUG("Trying without explicit pin configuration...");
    if (SD_MMC.begin()) {
        DEBUG("SD card initialized with ESP-IDF defaults");
        DEBUGF("SD Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
        printCardInfo();
        return;
    }
    
    // All methods failed
    DEBUG("==== SD CARD INITIALIZATION FAILED ====");
    DEBUG("Possible issues:");
    DEBUG("1. Check SD card is properly inserted");
    DEBUG("2. Verify SD card is working (test in computer)");
    DEBUG("3. Check power supply (SD needs 3.3V)");
    DEBUG("4. Add pull-up resistors (10kΩ) to CMD and DATA lines");
    DEBUG("5. Check wiring connections");
    DEBUG("6. Try different SD card (Class 10 recommended)");
    DEBUG("7. Ensure SD card is formatted as FAT32");
    DEBUG("=========================================");
}

void printCardInfo() {
    uint8_t cardType = SD_MMC.cardType();
    
    if (cardType == CARD_NONE) {
        DEBUG("SD Card Type: No card");
    } else if (cardType == CARD_MMC) {
        DEBUG("SD Card Type: MMC");
    } else if (cardType == CARD_SD) {
        DEBUG("SD Card Type: SD");
    } else if (cardType == CARD_SDHC) {
        DEBUG("SD Card Type: SDHC");
    } else {
        DEBUG("SD Card Type: Unknown");
    }
    
    DEBUGF("SD Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
    
    // List root directory contents
    DEBUG("Root directory contents:");
    File root = SD_MMC.open("/");
    if (root) {
        File file = root.openNextFile();
        int fileCount = 0;
        while (file) {
            if (file.isDirectory()) {
                DEBUGF("  DIR:  %s\n", file.name());
            } else {
                DEBUGF("  FILE: %s (%d bytes)\n", file.name(), file.size());
            }
            file.close();
            file = root.openNextFile();
            fileCount++;
            if (fileCount > 20) {  // Prevent spam
                DEBUG("  ... (more files)");
                break;
            }
        }
        root.close();
        if (fileCount == 0) {
            DEBUG("  (empty)");
        }
    }
}