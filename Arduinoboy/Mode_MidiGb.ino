/**************************************************************************
 * Name:    Timothy Lamb                                                  *
 * Email:   trash80@gmail.com                                             *
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <MIDI.h>

void modeMidiGbSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,OUTPUT);
  digitalWrite(pinGBClock,HIGH);

#ifdef USE_TEENSY
  usbMIDI.setHandleRealTimeSystem(NULL);
#endif

  blinkMaxCount=1000;
  modeMidiGb();
}

// Are we capturing SysEx bytes?
bool isSysexMessage = false;

void modeMidiGb()
{
  MIDI_CREATE_DEFAULT_INSTANCE();
  MIDI.begin(MIDI_CHANNEL_OMNI);

  while(1) {
    modeMidiGbUsbMidiReceive();

    if (!modeMidiGbSerialReceive(&MIDI)) {
      setMode();                // Check if mode button was depressed
      updateBlinkLights();
      updateStatusLed();
    }
  }
}

 /*
 sendByteToGameboy does what it says. yay magic
 */
void sendByteToGameboy(byte send_byte)
{
 for(countLSDJTicks=0;countLSDJTicks!=8;countLSDJTicks++) {  //we are going to send 8 bits, so do a loop 8 times
   if(send_byte & 0x80) {
       GB_SET(0,1,0);
       GB_SET(1,1,0);
   } else {
       GB_SET(0,0,0);
       GB_SET(1,0,0);
   }

#if defined (F_CPU) && (F_CPU > 24000000)
   // Delays for Teensy etc where CPU speed might be clocked too fast for cable & shift register on gameboy.
   delayMicroseconds(1);
#endif
   send_byte <<= 1;
 }
}

/** 
 * Prepares a SysEx array for the programmer. It expects sysexData to be populated with
 * the sysex array, without the header and EOF bytes.
 */
void handleProgrammer(const byte *sysexArray, uint8_t sysexLength) {
  // handle programmer. Copy payload without SysEx header and EOF
  memcpy(&sysexData[0], &sysexArray[1], sysexLength - 2);
  getSysexData();
}

/** Send multiple raw bytes to GB (e.g. a SysEx payload) */
void sendBytesToGameboy(const byte *bytes, uint8_t length) {
  // Serial.print("Sending bytes: ");
  for (uint8_t i = 0; i < length; i++) {
    byte b = bytes[i];
    // Serial.print(b, 0xF);
    // Serial.print(" ");
    sendByteToGameboy(b);
    delayMicroseconds(GB_MIDI_DELAY);
  }
  // Serial.println("");
}

void modeMidiGbUsbMidiReceive()
{
#ifdef USE_TEENSY

    while(usbMIDI.read()) {
      byte type = usbMIDI.getType();
      byte channel = usbMIDI.getChannel() == 0 ? 0 : usbMIDI.getChannel() - 1;
      byte data1 = usbMIDI.getData1();
      byte data2 = usbMIDI.getData2();
      
      byte sysexLength = usbMIDI.getSysExArrayLength();

      // handle SysEx
      if (type == midi::SystemExclusive) {
        sendBytesToGameboy(usbMIDI.getSysExArray(), sysexLength);

        handleProgrammer(usbMIDI.getSysExArray(), sysexLength);
      } else {
        sendMidiMessageToGameboy(type, channel, data1, data2);
      }
      
      statusLedOn();
    }
#endif

#ifdef USE_LEONARDO

    midiEventPacket_t rx;
    do {
      rx = MidiUSB.read();
      
      maybePassThroughSysex(rx.byte1);
      maybePassThroughSysex(rx.byte2);
      maybePassThroughSysex(rx.byte3);

      if (isSysexMessage) continue;

      byte type = rx.byte1 & 0xF0;
      byte channel = rx.byte1 & 0x0F;
      sendMidiMessageToGameboy(type, channel, rx.byte2, rx.byte3);
      statusLedOn();
    } while (rx.header != 0);
#endif
}

bool modeMidiGbSerialReceive(midi::MidiInterface<midi::SerialMIDI<HardwareSerial>> *MIDI) {
  if (!MIDI->read()) { return false; }

  byte type = MIDI->getType();
  byte channel = MIDI->getChannel() == 0 ? 0 : MIDI->getChannel() - 1;
  byte data1 = MIDI->getData1();
  byte data2 = MIDI->getData2();
  byte sysexLength = MIDI->getSysExArrayLength();

  // char strBuf[100];
  // sprintf(strBuf, "{type: %X, channel: %X, data1: %X, data2: %X, sysex: %d}", type, channel, data1, data2, sysexLength);
  // Serial.println(strBuf);

  // handle SysEx
  if (type == midi::SystemExclusive) {
    sendBytesToGameboy(MIDI->getSysExArray(), sysexLength);

    handleProgrammer(MIDI->getSysExArray(), sysexLength);
  } else {
    sendMidiMessageToGameboy(type, channel, data1, data2);

    if (!usbMode) {
      sendMidiMessageToSerialOut(type, channel, data1, data2);
    }
  }
  
  statusLedOn();
  return true;
}

/** 
 * Send a MIDI message to GB
 *  @param type The type (Status without channel e.g. 0x90), or the status message
 *  @param channel A 0 indexed channel
 *  @param data1 The second message byte
 *  @param data1 The third message byte
 */
void sendMidiMessageToGameboy(byte type, byte channel, byte data1, byte data2) {
  byte data0 = type + mappedChannel(channel);

  switch (type)
  {
    // 1 byte messages
    // case midi::Tick: // filtered
    // case midi::ActiveSensing: // filtered
    // case midi::TuneRequest: // filtered
    case midi::Start:
    case midi::Continue:
    case midi::Stop:
    case midi::Clock:
    case midi::SystemReset:
        sendByteToGameboy(type);
        delayMicroseconds(GB_MIDI_DELAY);
        break;

    // 2 byte messages
    case midi::ProgramChange:
    // case midi::SongSelect: // filtered
    // case midi::AfterTouchChannel: // filtered
    // case midi::TimeCodeQuarterFrame: // filtered
        sendByteToGameboy(data0);
        delayMicroseconds(GB_MIDI_DELAY);
        sendByteToGameboy(data1);
        delayMicroseconds(GB_MIDI_DELAY);
        break;

    // 3 byte messages
    // case midi::SongPosition: // filtered
    // case midi::AfterTouchPoly: // filtered
    case midi::NoteOn:
    case midi::NoteOff:
    case midi::ControlChange:
    case midi::PitchBend:
        sendByteToGameboy(data0);
        delayMicroseconds(GB_MIDI_DELAY);
        
        sendByteToGameboy(data1);
        delayMicroseconds(GB_MIDI_DELAY);
        
        sendByteToGameboy(data2);
        delayMicroseconds(GB_MIDI_DELAY);

        blinkLight(data0, data2);
        break;
    default:
        break;
  }
}

#define MGB_PU1 0
#define MGB_PU2 1
#define MGB_WAV 2
#define MGB_NOI 3
#define MGB_POLY 4

/** 
 * Redirects the configured input midi channel to the correct GB midi channel. e.g.
 * Config channel for PU1 (CH1) might be MIDI channel 5, it will redirect input CH5 to 
 * output CH1 for the GB (Since mGB has fixed channels)
 */
uint8_t mappedChannel(uint8_t inputChannel) {
  uint8_t customPU1 = memory[MEM_MGB_CH + MGB_PU1];
  uint8_t customPU2 = memory[MEM_MGB_CH + MGB_PU2];
  uint8_t customWAV = memory[MEM_MGB_CH + MGB_WAV];
  uint8_t customNOI = memory[MEM_MGB_CH + MGB_NOI];
  uint8_t customPOLY = memory[MEM_MGB_CH + MGB_POLY];

  if (inputChannel == customPU1)
    return MGB_PU1;
  if (inputChannel == customPU2)
    return MGB_PU2;
  if (inputChannel == customWAV)
    return MGB_WAV;
  if (inputChannel == customNOI)
    return MGB_NOI;
  if (inputChannel == customPOLY)
    return MGB_POLY;

  return inputChannel;
}

/** 
 * Send a MIDI message to serial out
 *  @param type The type (Status without channel e.g. 0x90), or the status message
 *  @param channel A 0 indexed channel (for messages that are not channel specific this will be 0)
 *  @param data1 The second message byte
 *  @param data1 The third message byte
 */
void sendMidiMessageToSerialOut(byte type, byte channel, byte data1, byte data2) {
  byte data0 = type + channel;

  switch (type)
  {
    // 1 byte messages
    case midi::Tick:
    case midi::ActiveSensing:
    case midi::TuneRequest:
    case midi::Start:
    case midi::Continue:
    case midi::Stop:
    case midi::Clock:
    case midi::SystemReset:
        serial->write(type);
        break;

    // 2 byte messages
    case midi::ProgramChange:
    case midi::SongSelect:
    case midi::AfterTouchChannel:
    case midi::TimeCodeQuarterFrame:
        serial->write(data0);
        serial->write(data1);
        break;

    // 3 byte messages
    case midi::SongPosition:
    case midi::AfterTouchPoly:
    case midi::NoteOn:
    case midi::NoteOff:
    case midi::ControlChange:
    case midi::PitchBend:
        serial->write(data0);
        serial->write(data1);
        serial->write(data2);
        break;
    default:
        break;
  }
}

/** Passes through SysEx messages to the GB and the programmer (if applicable) */
void maybePassThroughSysex(byte incomingMidiByte) {
  // fills the sysexData payload so that programmer messages will work
  checkForProgrammerSysex(incomingMidiByte);

  // byte 1 - Check for SysEx status byte
  if (!isSysexMessage && incomingMidiByte == midi::SystemExclusive) {
    isSysexMessage = true;
    // Serial.print("SysEx: ");
    // Serial.println(incomingMidiByte, 0xF);
    sendByteToGameboy(incomingMidiByte);
    delayMicroseconds(GB_MIDI_DELAY);
    blinkLight(0x90, 1);
    return;
  }

  if (!isSysexMessage) return;


  // send bytes (incl EOF)
  sendByteToGameboy(incomingMidiByte);
  delayMicroseconds(GB_MIDI_DELAY);
  blinkLight(0x9D, 1);
  // Serial.print("-->: ");
  // Serial.println(incomingMidiByte, 0xF);

  // stop passing through, EOF
  if (incomingMidiByte == midi::SystemExclusiveEnd) {
    isSysexMessage = false;
  }
}
