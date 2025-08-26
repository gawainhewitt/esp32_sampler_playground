#include "serial_commands.h"
#include "../config.h"
#include "../debug.h"
#include "../audio/audio_engine.h"
#include "../storage/instrument_manager.h"
#include "FS.h"
#include "SD_MMC.h"

void listSDContents(const char* dirname = "/", int maxDepth = 2, int currentDepth = 0) {
    if (currentDepth >= maxDepth) return;
    
    File root = SD_MMC.open(dirname);
    if (!root) {
        DEBUGF("Failed to open directory: %s\n", dirname);
        return;
    }
    
    if (!root.isDirectory()) {
        DEBUG("Not a directory");
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file) {
        // Print indentation
        for (int i = 0; i < currentDepth; i++) {
            Serial.print("  ");
        }
        
        if (file.isDirectory()) {
            DEBUGF("DIR:  %s/\n", file.name());
            // Recursively list subdirectories
            String subPath = String(dirname) + String(file.name()) + "/";
            listSDContents(subPath.c_str(), maxDepth, currentDepth + 1);
        } else {
            DEBUGF("FILE: %s (%d bytes)\n", file.name(), file.size());
        }
        file.close();
        file = root.openNextFile();
        
        delay(10); // Small delay to prevent watchdog issues
    }
    root.close();
}

void testSDCardAccess() {
    DEBUG("=== SD Card Diagnostic ===");
    
    // Test basic card info
    DEBUGF("Card Size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
    DEBUGF("Card Type: %d\n", SD_MMC.cardType());
    
    // List all files
    DEBUG("Complete directory listing:");
    listSDContents("/", 3, 0);
    
    // Test specific file access
    DEBUG("\nTesting specific drum files:");
    const char* drumFiles[] = {
        "/kick.wav",
        "/snare.wav", 
        "/hihat_closed.wav",
        "/hihat_open.wav",
        "/crash.wav",
        "/ride.wav"
    };
    
    for (int i = 0; i < 6; i++) {
        File testFile = SD_MMC.open(drumFiles[i]);
        if (testFile) {
            DEBUGF("✓ %s exists (%d bytes)\n", drumFiles[i], testFile.size());
            testFile.close();
        } else {
            DEBUGF("✗ %s missing or inaccessible\n", drumFiles[i]);
        }
        delay(50); // Prevent rapid SD access
    }
    
    DEBUG("=== End Diagnostic ===");
}

void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.startsWith("play ")) {
            int note = command.substring(5).toInt();
            if (note >= 0 && note <= 127) {
                noteOn(note, 127);
                DEBUGF("Playing MIDI note %d\n", note);
            }
        }
        else if (command.startsWith("stop ")) {
            int note = command.substring(5).toInt();
            if (note >= 0 && note <= 127) {
                noteOff(note);
                DEBUGF("Stopping MIDI note %d\n", note);
            }
        }
        else if (command.startsWith("volume ")) {
            float vol = command.substring(7).toFloat();
            setSampleVolume(vol);
        }
        else if (command.startsWith("instrument ")) {
            int inst = command.substring(11).toInt();
            selectInstrument(inst);
        }
        else if (command == "load piano") {
            loadBasicPiano();
        }
        else if (command == "load drums") {
            loadBasicDrumKit();
        }
        else if (command == "sdtest") {
            testSDCardAccess();
        }
        else if (command == "sdlist") {
            DEBUG("SD Card Contents:");
            listSDContents("/", 3, 0);
        }
        else if (command == "status") {
            DEBUGF("Loaded instruments: %d\n", loadedInstruments);
            DEBUGF("Current instrument: %s\n", 
                   getCurrentInstrument() ? getCurrentInstrument()->name.c_str() : "None");
            DEBUGF("Total samples: %d\n", loadedSamples);
            
            int activeVoices = 0;
            for (int i = 0; i < MAX_POLYPHONY; i++) {
                if (voices[i].isActive) activeVoices++;
            }
            DEBUGF("Active voices: %d\n", activeVoices);
            DEBUGF("Sample volume: %.1f\n", sampleVolume);
            
            // Show current instrument details
            Instrument* current = getCurrentInstrument();
            if (current) {
                DEBUGF("Current instrument '%s' has %d key samples:\n", 
                       current->name.c_str(), current->numKeySamples);
                for (int i = 0; i < current->numKeySamples; i++) {
                    KeySample* ks = &current->keySamples[i];
                    if (ks->isLoaded) {
                        DEBUGF("  %s: root=%d, range=%d-%d\n", 
                               ks->sample->filename.c_str(), ks->rootNote, ks->minNote, ks->maxNote);
                    }
                }
            }
        }
        else if (command == "help") {
            DEBUG("Available commands:");
            DEBUG("  play <note>        - Play note (0-127)");
            DEBUG("  stop <note>        - Stop note");
            DEBUG("  volume <0-2>       - Set volume");
            DEBUG("  instrument <0-3>   - Select instrument");
            DEBUG("  load piano         - Load basic piano");
            DEBUG("  load drums         - Load basic drums");
            DEBUG("  status             - Show detailed status");
            DEBUG("  sdtest             - Test SD card access");
            DEBUG("  sdlist             - List SD card contents");
            DEBUG("  help               - Show this help");
        }
    }
}