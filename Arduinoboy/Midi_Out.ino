/// File contains some wrappers for the serial/usb midi out functions, to handle a few conditionals and bugs

void MIDI_sendNoteOn(byte note, byte velocity, byte channel) {
  sMIDI.sendNoteOn(note, velocity, channel);
  #ifdef HAS_USB_MIDI
    uMIDI.sendNoteOn(note, velocity, channel);
  #endif
}

void MIDI_sendNoteOff(byte note, byte velocity, byte channel) {
  sMIDI.sendNoteOff(note, velocity, channel);
  #ifdef HAS_USB_MIDI
    uMIDI.sendNoteOff(note, velocity, channel);
  #endif
}

void MIDI_sendRealTime(midi::MidiType t) {
  sMIDI.sendRealTime(t);
  #ifdef HAS_USB_MIDI
    // usb midi.sendRealTime is currently busted: https://github.com/lathoub/Arduino-USBMIDI/issues/16
    // uMIDI.sendRealTime(t);
    midiEventPacket_t packet = {0xF,t, 0x0, 0x0};
    MidiUSB.sendMIDI(packet);
    MidiUSB.flush();
  #endif
}

void MIDI_sendControlChange(byte number, byte value, byte channel) {
  sMIDI.sendControlChange(number, value, channel);
  #ifdef HAS_USB_MIDI
    uMIDI.sendControlChange(number, value, channel);
  #endif
}

void MIDI_sendProgramChange(byte number, byte channel) {
  sMIDI.sendProgramChange(number, channel);
  #ifdef HAS_USB_MIDI
    uMIDI.sendProgramChange(number, channel);
  #endif
}