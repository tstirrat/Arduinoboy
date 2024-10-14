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

void modeNanoloopSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,OUTPUT);
  digitalWrite(pinGBClock,HIGH);

  blinkMaxCount=1000;
  modeNanoloopSync();
}

void modeNanoloopSync()
{
  while (1) {
    modeNanoloopUsbMidiReceive();
    modeNanoloopSerialMidiReceive();

    setMode();         //Check if the mode button was depressed
    updateStatusLight();
  }
}

boolean sendTickToNanoloop(boolean state, boolean last_state)
{
  if(!state) {
    if(last_state) {
       GB_SET(0,1,0);
       GB_SET(1,1,0);
    } else {
       GB_SET(0,0,0);
       GB_SET(1,0,0);
    }
    return true;
  } else {
    GB_SET(0,1,0);
    GB_SET(1,1,0);
    return false;
  }
}


void modeNanoloopUsbMidiReceive()
{
#ifdef HAS_USB_MIDI
  while (uMIDI.read()) {
    // checkForUsbProgrammerSysex(&usbMIDI);

    byte type = uMIDI.getType();
    byte channel = uMIDI.getChannel();
    byte data1 = uMIDI.getData1();

    handleNanoloopMessage(type, channel, data1);
  }
#endif
}

void modeNanoloopSerialMidiReceive()
{
  while (sMIDI.read()) {
    // checkForSerialProgrammerSysex(&sMIDI);

    byte type = sMIDI.getType();
    byte channel = sMIDI.getChannel();
    byte data1 = sMIDI.getData1();

    handleNanoloopMessage(type, channel, data1);
  }
}

void handleNanoloopMessage(byte type, byte channel, byte data1) {
  switch (type) {
    // Note handling was in Teensy code, but not in serial midi code.
    // assuming it was accidental copy paste from LSDJ Slave code.
    // leaving it out

    // case midi::NoteOn:
    //   if (channel == memory[MEM_LSDJSLAVE_MIDI_CH]+1) {
    //     getSlaveSyncEffect(data1);
    //   }
    //   break;
    case midi::Clock:
      if(sequencerStarted) {
        nanoSkipSync = !nanoSkipSync;
        if(countSyncTime) {
          nanoState = sendTickToNanoloop(nanoState, false);
        } else {
          nanoState = sendTickToNanoloop(true, true);
        }
        nanoState = sendTickToNanoloop(nanoState, nanoSkipSync);
        updateVisualSync();
      }
      break;
    case midi::Start:
    case midi::Continue:
      sequencerStart();
      break;
    case midi::Stop:
      sequencerStop();
      break;
  }
}