#pragma once

#include <MIDI.h>
#include "../config.h"

// MIDI setup
struct Serial2MIDISettings : public midi::DefaultSettings {
    static const long BaudRate = 31250;
    static const int8_t RxPin  = MIDIRX_PIN;
    static const int8_t TxPin  = MIDITX_PIN;
};

extern MIDI_NAMESPACE::SerialMIDI<HardwareSerial> Serial2MIDI;
extern MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial, Serial2MIDISettings>> MIDI;