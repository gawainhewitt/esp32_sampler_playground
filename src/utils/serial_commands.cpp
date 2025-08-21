#include "serial_commands.h"
#include "../config.h"
#include "../debug.h"
#include "../audio/audio_engine.h"

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
        else if (command == "status") {
            DEBUGF("Loaded samples: %d\n", loadedSamples);
            int activeVoices = 0;
            for (int i = 0; i < MAX_POLYPHONY; i++) {
                if (voices[i].isActive) activeVoices++;
            }
            DEBUGF("Active voices: %d\n", activeVoices);
            DEBUGF("Sample volume: %.1f\n", sampleVolume);
        }
    }
}