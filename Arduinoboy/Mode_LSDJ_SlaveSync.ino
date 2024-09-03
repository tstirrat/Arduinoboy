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

void modeLSDJSlaveSyncSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,OUTPUT);
  digitalWrite(pinGBClock,HIGH);

  blinkMaxCount=1000;
  modeLSDJSlaveSync();
}

void modeLSDJSlaveSync()
{
  while (1) {
    modeLSDJSlaveSyncUsbMidiReceive();
    modeLSDJSlaveSyncSerialMidiReceive();
    
    setMode();         //Check if the mode button was depressed
    updateStatusLight();
  }
}

/*
  sendClockTickToLSDJ is a lovely loving simple function I wish they where all this short
  Technicallyly we are sending nothing but a 8bit clock pulse
*/
void sendClockTickToLSDJ()
{
  for(countLSDJTicks=0;countLSDJTicks<8;countLSDJTicks++) {
    GB_SET(0,0,0);
    GB_SET(1,0,0);
  }
}


/*
  getSlaveSyncEffect receives a note, and assigns the propper effect of that note
*/
void getSlaveSyncEffect(byte note)
{
    switch(note) {
    case 48:                        //C-3ish, Transport Start
      sequencerStart();
      break;
    case 49:                        //C#3 Transport Stop
      sequencerStop();
      break;
    case 50:                        //D-3 Turn off sync effects
      midiSyncEffectsTime = false;
      break;
    case 51:                        //D#3 Sync effect, 1/2 time
      midiSyncEffectsTime = true;
      countSyncTime = 0;
      countSyncSteps = 2;
      break;
    case 52:                        //E-3 Sync Effect, 1/4 time
      midiSyncEffectsTime = true;
      countSyncTime = 0;
      countSyncSteps = 4;
      break;
    case 53:                        //F-3 Sync Effect, 1/8 time
      midiSyncEffectsTime = true;
      countSyncTime = 0;
      countSyncSteps = 8;
      break;
    default:                        //All other notes will make LSDJ Start at the row number thats the same as the note number.
      midiDefaultStartOffset = midiData[0];
      break;
    }
}

void modeLSDJSlaveSyncUsbMidiReceive()
{
#ifdef HAS_USB_MIDI
  while (uMIDI.read()) {
    byte type = uMIDI.getType();
    byte channel = uMIDI.getChannel();
    byte data1 = uMIDI.getData1();

    handleLsdjSlaveSyncMessage(type, channel, data1);
  }
#endif
}

void modeLSDJSlaveSyncSerialMidiReceive()
{
  if (!sMIDI.read()) return;

  byte type = sMIDI.getType();
  byte channel = sMIDI.getChannel();
  byte data1 = sMIDI.getData1();

  handleLsdjSlaveSyncMessage(type, channel, data1);
}

void handleLsdjSlaveSyncMessage(byte type, byte channel, byte data1) {
  switch (type) {
    case midi::NoteOn:
      if (channel == memory[MEM_LSDJSLAVE_MIDI_CH]+1) {
        getSlaveSyncEffect(data1);
      }
    break;
    case midi::Clock:
      if ((sequencerStarted && midiSyncEffectsTime && !countSyncTime) //If the seq has started and our sync effect is on and at zero
          || (sequencerStarted && !midiSyncEffectsTime))
      { //or seq is started and there is no sync effects
        if (!countSyncPulse && midiDefaultStartOffset)
        { //if we received a note for start offset
          //sendByteToGameboy(midiDefaultStartOffset);              //send the offset
        }
        sendClockTickToLSDJ(); //send the clock tick
        updateVisualSync();
      }
      if (midiSyncEffectsTime)
      {                                                 //If sync effects are turned on
        countSyncTime++;                                //increment our tick counter
        countSyncTime = countSyncTime % countSyncSteps; //and mod it by the number of steps we want for the effect
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
