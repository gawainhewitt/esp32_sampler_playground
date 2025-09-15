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

// Add this to your sd_manager.cpp or sample_loader.cpp

// Replace your existing wakeUpSDCard() function with this more aggressive version

bool wakeUpSDCard() {
    DEBUG("Waking up SD card with file test...");
    
    // Test actual file operations, not just directory
    File testFile = SD_MMC.open("/piano_C4.wav");  // Known good file from your setup
    if (testFile) {
        testFile.close();
        DEBUG("File test successful - SD card operational");
        return true;  // File operations working
    }
    
    // If file test fails, go straight to reset
    DEBUG("File test failed, doing immediate SD reset...");
    
    // Perform SD card reset
    SD_MMC.end();
    delay(300); // Slightly longer delay for reliability
    
    if (SD_MMC.begin("/sdcard", false)) { // false = 4-bit mode
        DEBUG("SD reset successful, verifying with file test...");
        
        // Verify reset worked with another file test
        testFile = SD_MMC.open("/piano_C4.wav");
        if (testFile) {
            testFile.close();
            DEBUGF("SD card fully operational after reset (size: %llu MB)\n", 
                   SD_MMC.cardSize() / (1024 * 1024));
            return true;
        } else {
            DEBUG("File test still failing after 4-bit reset, trying 1-bit mode...");
        }
    }
    
    // Fallback to 1-bit mode
    SD_MMC.end();
    delay(300);
    
    if (SD_MMC.begin("/sdcard", true)) { // true = 1-bit mode
        DEBUG("1-bit mode reset successful, verifying...");
        
        testFile = SD_MMC.open("/piano_C4.wav");
        if (testFile) {
            testFile.close();
            DEBUG("1-bit mode operational");
            return true;
        }
    }
    
    DEBUG("All SD reset methods failed");
    return false;
}

// Enhanced version that tries multiple test files (in case one is corrupted)
bool wakeUpSDCardRobust() {
    DEBUG("Robust SD wake-up with multiple file tests...");
    
    // List of known files to test (add files you know exist)
    const char* testFiles[] = {
        "/piano_C4.wav",
        "/piano_C3.wav", 
        "/piano_C5.wav",
        "/kick.wav"
    };
    
    // Try opening multiple files to ensure SD card is truly working
    int successfulTests = 0;
    for (int i = 0; i < 4; i++) {
        File testFile = SD_MMC.open(testFiles[i]);
        if (testFile) {
            testFile.close();
            successfulTests++;
            if (successfulTests >= 2) {
                DEBUGF("Multiple file tests successful (%d/4) - SD card operational\n", successfulTests);
                return true;
            }
        }
    }
    
    DEBUGF("File tests failed (%d/4 succeeded), doing immediate SD reset...\n", successfulTests);
    
    // Reset logic (same as above)
    SD_MMC.end();
    delay(300);
    
    if (SD_MMC.begin("/sdcard", false)) { // 4-bit mode
        DEBUG("SD reset successful, re-testing files...");
        
        // Re-test files after reset
        successfulTests = 0;
        for (int i = 0; i < 4; i++) {
            File testFile = SD_MMC.open(testFiles[i]);
            if (testFile) {
                testFile.close();
                successfulTests++;
            }
        }
        
        if (successfulTests >= 2) {
            DEBUGF("Reset successful - %d/4 files accessible\n", successfulTests);
            return true;
        }
        
        DEBUG("4-bit mode still having issues, trying 1-bit mode...");
    }
    
    // 1-bit fallback
    SD_MMC.end();
    delay(300);
    
    if (SD_MMC.begin("/sdcard", true)) { // 1-bit mode
        DEBUG("1-bit reset successful, testing files...");
        
        successfulTests = 0;
        for (int i = 0; i < 4; i++) {
            File testFile = SD_MMC.open(testFiles[i]);
            if (testFile) {
                testFile.close();
                successfulTests++;
            }
        }
        
        if (successfulTests >= 1) {
            DEBUGF("1-bit mode working - %d/4 files accessible\n", successfulTests);
            return true;
        }
    }
    
    DEBUG("All SD card recovery attempts failed");
    return false;
}

// Simple version for runtime use (less aggressive, faster)
bool quickSDCheck() {
    File testFile = SD_MMC.open("/piano_C4.wav");
    if (testFile) {
        testFile.close();
        return true;
    }
    return false;
}

// Enhanced version with retry and timing
bool wakeUpSDCardWithRetry(int maxAttempts) {
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        DEBUGF("SD wake up attempt %d of %d\n", attempt, maxAttempts);
        
        if (wakeUpSDCard()) {
            if (attempt > 1) {
                DEBUGF("SD card woke up after %d attempts\n", attempt);
            }
            return true;
        }
        
        if (attempt < maxAttempts) {
            DEBUG("Wake up failed, waiting before retry...");
            delay(500 * attempt); // Increasing delay between retries
        }
    }
    
    DEBUG("SD card wake up failed after all attempts");
    return false;
}