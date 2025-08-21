#include "midi_handler.h"
#include "midi_config.h"
#include "../audio/audio_engine.h"
#include "../config.h"

MIDI_NAMESPACE::SerialMIDI<HardwareSerial> Serial2MIDI(Serial2);
MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial, Serial2MIDISettings>> MIDI((MIDI_NAMESPACE::SerialMIDI<HardwareSerial, Serial2MIDISettings>&)Serial2MIDI);

void initMIDI() {
    Serial2.begin(31250, SERIAL_8N1, MIDIRX_PIN, MIDITX_PIN);
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.begin(MIDI_CHANNEL_OMNI);
}

void processMIDI() {
    MIDI.read();
}

void handleNoteOn(byte channel, byte note, byte velocity) {
    noteOn(note, velocity);
}

void handleNoteOff(byte channel, byte note, byte velocity) {
    noteOff(note);
}