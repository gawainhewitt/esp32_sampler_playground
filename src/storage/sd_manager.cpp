#include "sd_manager.h"
#include "../config.h"
#include "../debug.h"
#include "FS.h"
#include "SD_MMC.h"

void initSD() {
    DEBUG("Initializing SD card in 4-bit SDMMC mode...");
    
    DEBUGF("Using SDMMC pins - CLK:%d, CMD:%d, D0:%d, D1:%d, D2:%d, D3:%d\n",
           SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
    
    // Set custom pins for SDMMC
    if (!SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3)) {
        DEBUG("Failed to set SDMMC pins!");
        return;
    }
    
    // Add pull-ups (important for SDMMC reliability)
    pinMode(SDMMC_CLK, PULLUP);
    pinMode(SDMMC_CMD, PULLUP);
    pinMode(SDMMC_D0, PULLUP);
    pinMode(SDMMC_D1, PULLUP);
    pinMode(SDMMC_D2, PULLUP);
    pinMode(SDMMC_D3, PULLUP);
    
    // Initialize SD_MMC
    // Parameters: mountpoint, mode1bit (false = 4-bit), format_if_mount_failed, max_files, frequency_khz
    if (SD_MMC.begin("/sdcard", true, false, 20000, 5)) {
        DEBUG("SD card initialized successfully (SDMMC 4-bit)");
        
        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE) {
            DEBUG("No SD card attached");
            return;
        }
        
        // Print card type
        DEBUG("SD Card Type: ");
        if (cardType == CARD_MMC) {
            DEBUG("MMC");
        } else if (cardType == CARD_SD) {
            DEBUG("SDSC");
        } else if (cardType == CARD_SDHC) {
            DEBUG("SDHC");
        } else {
            DEBUG("UNKNOWN");
        }
        
        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        DEBUGF("SD Card Size: %lluMB\n", cardSize);
        DEBUGF("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
        DEBUGF("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
        
    } else {
        DEBUG("Failed to mount SD card (SDMMC)");
        DEBUG("Troubleshooting:");
        DEBUG("  1. Check SD card is inserted and formatted (FAT32)");
        DEBUG("  2. Verify pin connections");
        DEBUG("  3. Check external 10K pull-up resistors on data lines (or verify breakout has them)");
        DEBUG("  4. Try a different SD card");
    }
}