#include "serial_commands.h"
#include "../config.h"
#include "../debug.h"
#include "../audio/audio_engine.h"
#include "../storage/instrument_manager.h"

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
            DEBUG("  help               - Show this help");
        }
    }
}