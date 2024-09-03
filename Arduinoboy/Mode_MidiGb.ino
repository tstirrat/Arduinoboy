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

void modeMidiGbSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,OUTPUT);
  digitalWrite(pinGBClock,HIGH);

  blinkMaxCount=1000;
  modeMidiGb();
}

void modeMidiGb()
{
  while(1) {
    modeMidiGbUsbMidiReceive();

    if (!modeMidiGbSerialReceive()) {
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

/** Send multiple raw bytes to GB (e.g. a SysEx payload) */
void sendBytesToGameboy(const byte *bytes, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    byte b = bytes[i];
    sendByteToGameboy(b);
    delayMicroseconds(GB_MIDI_DELAY);
  }
}

void modeMidiGbUsbMidiReceive()
{
#ifdef HAS_USB_MIDI
  while (uMIDI.read()) {
    byte type = uMIDI.getType();
    byte channel = uMIDI.getChannel() == 0 ? 0 : uMIDI.getChannel() - 1;
    byte data1 = uMIDI.getData1();
    byte data2 = uMIDI.getData2();

    if (type == midi::SystemExclusive) {
      sendBytesToGameboy(uMIDI.getSysExArray(), uMIDI.getSysExArrayLength());
    } else {
      sendMidiMessageToGameboy(type, channel, data1, data2);
    }
    
    statusLedOn();
  }
#endif
}

bool modeMidiGbSerialReceive() {
  if (!sMIDI.read()) { return false; }

  byte type = sMIDI.getType();
  byte channel = sMIDI.getChannel() == 0 ? 0 : sMIDI.getChannel() - 1;
  byte data1 = sMIDI.getData1();
  byte data2 = sMIDI.getData2();


  if (type == midi::SystemExclusive) {
    sendBytesToGameboy(sMIDI.getSysExArray(), sMIDI.getSysExArrayLength());
  } else {
    sendMidiMessageToGameboy(type, channel, data1, data2);
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
  if (shouldDropRedirectedSynthMessage(type, channel)) return;

  byte data0 = type | getMappedChannel(channel);

  switch (type)
  {
    // 1 byte messages
    // case midi::Tick: // filtered
    // case midi::ActiveSensing: // filtered
    case midi::TuneRequest:
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
    case midi::SongSelect:
    // case midi::AfterTouchChannel: // filtered
    // case midi::TimeCodeQuarterFrame: // filtered
        sendByteToGameboy(data0);
        delayMicroseconds(GB_MIDI_DELAY);
        sendByteToGameboy(data1);
        delayMicroseconds(GB_MIDI_DELAY);
        break;

    // 3 byte messages
    case midi::SongPosition:
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
uint8_t getMappedChannel(uint8_t inputChannel) {
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
 * We drop a MIDI message if:
 * - It's a channel based message
 * - The MIDI channel is not in the MIDI -> mGB channel map
 * 
 * Note: The MIDI message will still be passed via MIDI Thru to the serial out
 */
bool shouldDropRedirectedSynthMessage(byte type, uint8_t channel) {
  if (channel > MGB_POLY) return false;

  // message types that have channels
  if (type >= midi::NoteOff && type <= midi::PitchBend) {
    return !isMappedChannel(channel);
  }
  return false;
}

/** Is this MIDI channel mapped to a mGB synth in the config? */
bool isMappedChannel(uint8_t ch) {
  uint8_t customPU1 = memory[MEM_MGB_CH + MGB_PU1];
  uint8_t customPU2 = memory[MEM_MGB_CH + MGB_PU2];
  uint8_t customWAV = memory[MEM_MGB_CH + MGB_WAV];
  uint8_t customNOI = memory[MEM_MGB_CH + MGB_NOI];
  uint8_t customPOLY = memory[MEM_MGB_CH + MGB_POLY];

  return customPU1 == ch || customPU2 == ch || customWAV == ch || customNOI == ch || customPOLY == ch;
}