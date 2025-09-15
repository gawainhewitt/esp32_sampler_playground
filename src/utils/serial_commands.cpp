#include "serial_commands.h"
#include "../config.h"
#include "../debug.h"
#include "../audio/audio_engine.h"
#include "../storage/instrument_manager.h"
#include "../audio/mp3_test.h"
#include "../audio/mp3_streamer.h"
#include "FS.h"
#include "SD.h"

void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.startsWith("play mp3 ")) {
            String filename = command.substring(9);
            filename.trim();
            startMP3Stream(filename.c_str());
        }
        else if (command.startsWith("play ")) {
            int note = command.substring(5).toInt();
            if (note >= 0 && note <= 127) {
                noteOn(note, 127);
                DEBUGF("Playing MIDI note %d\n", note);
            }
        }
        else if (command.startsWith("stop mp3")) {
            stopMP3Stream();
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
        else if (command.startsWith("test mp3 ")) {
            String filename = command.substring(9);
            filename.trim();
            testMP3Decode(filename.c_str());
        }
        else if (command.startsWith("stream mp3 ")) {
            String filename = command.substring(11);
            filename.trim();
            testMP3Stream(filename.c_str());
        }
        else if (command == "list files") {
            listSDFiles();
        }
        else if (command.startsWith("file info ")) {
            String filename = command.substring(10);
            filename.trim();
            showFileInfo(filename.c_str());
        }
        else if (command == "memory") {
            showMemoryInfo();
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
            DEBUG("  test mp3 <file>    - Test MP3 decode (e.g., 'test mp3 song.mp3')");
            DEBUG("  stream mp3 <file>  - Test MP3 streaming decode");
            DEBUG("  play mp3 <file>    - Start MP3 backing track");
            DEBUG("  stop mp3           - Stop MP3 backing track");
            DEBUG("  mp3 volume <0-1>   - Set MP3 backing track volume");
            DEBUG("  list files         - Show all files on SD card");
            DEBUG("  file info <file>   - Show detailed file information");
            DEBUG("  memory             - Show memory usage");
            DEBUG("  status             - Show detailed status");
            DEBUG("  sdtest             - Test SD card access");
            DEBUG("  sdlist             - List SD card contents");
            DEBUG("  help               - Show this help");
        }
    }
}

void listSDFiles() {
    DEBUG("=== SD Card Files ===");
    
    File root = SD.open("/");
    if (!root) {
        DEBUG("Failed to open root directory");
        return;
    }
    
    if (!root.isDirectory()) {
        DEBUG("Root is not a directory");
        root.close();
        return;
    }
    
    File file = root.openNextFile();
    int fileCount = 0;
    
    while (file) {
        if (file.isDirectory()) {
            DEBUGF("[DIR]  %s\n", file.name());
        } else {
            String filename = file.name();
            String extension = "";
            int lastDot = filename.lastIndexOf('.');
            if (lastDot > 0) {
                extension = filename.substring(lastDot);
                extension.toUpperCase();
            }
            
            DEBUGF("[FILE] %s (%d bytes) %s\n", 
                   file.name(), 
                   file.size(),
                   extension.c_str());
            fileCount++;
        }
        file = root.openNextFile();
    }
    
    root.close();
    DEBUGF("Total files: %d\n", fileCount);
    DEBUG("=== End File List ===");
}

void showFileInfo(const char* filename) {
    String filepath = String(filename);
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }
    
    DEBUG("=== File Information ===");
    DEBUGF("Filename: %s\n", filepath.c_str());
    
    File file = SD.open(filepath.c_str());
    if (!file) {
        DEBUG("File not found or cannot be opened");
        return;
    }
    
    DEBUGF("Size: %d bytes (%.2f KB)\n", file.size(), file.size() / 1024.0);
    
    // Check file extension
    String ext = filepath;
    int lastDot = ext.lastIndexOf('.');
    if (lastDot > 0) {
        ext = ext.substring(lastDot);
        ext.toUpperCase();
        DEBUGF("Extension: %s\n", ext.c_str());
        
        if (ext == ".MP3") {
            DEBUG("File type: MP3 audio");
            
            // Read first few bytes to check for ID3 tag or MP3 sync
            uint8_t header[10];
            file.read(header, 10);
            
            if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
                DEBUG("Has ID3 tag");
                DEBUGF("ID3 version: %d.%d\n", header[3], header[4]);
            } else if ((header[0] == 0xFF && (header[1] & 0xE0) == 0xE0)) {
                DEBUG("Found MP3 sync pattern");
            } else {
                DEBUG("Unknown MP3 format - might still work");
            }
        } else if (ext == ".WAV") {
            DEBUG("File type: WAV audio");
        }
    }
    
    file.close();
    DEBUG("=== End File Info ===");
}

void showMemoryInfo() {
    DEBUG("=== Memory Information ===");
    
    // Heap memory
    DEBUGF("Free heap: %d bytes (%.1f KB)\n", 
           ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
    DEBUGF("Largest free block: %d bytes (%.1f KB)\n", 
           ESP.getMaxAllocHeap(), ESP.getMaxAllocHeap() / 1024.0);
    DEBUGF("Total heap: %d bytes (%.1f KB)\n", 
           ESP.getHeapSize(), ESP.getHeapSize() / 1024.0);
    
    // PSRAM memory  
    DEBUGF("Free PSRAM: %d bytes (%.1f KB)\n", 
           ESP.getFreePsram(), ESP.getFreePsram() / 1024.0);
    DEBUGF("Total PSRAM: %d bytes (%.1f KB)\n", 
           ESP.getPsramSize(), ESP.getPsramSize() / 1024.0);
    
    // Flash memory
    DEBUGF("Flash size: %d bytes (%.1f MB)\n", 
           ESP.getFlashChipSize(), ESP.getFlashChipSize() / (1024.0 * 1024.0));
    
    // Audio-specific memory usage
    DEBUGF("Loaded samples: %d/%d\n", loadedSamples, MAX_SAMPLES);
    DEBUGF("Loaded instruments: %d/%d\n", loadedInstruments, MAX_INSTRUMENTS);
    
    DEBUG("=== End Memory Info ===");
}