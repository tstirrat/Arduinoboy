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

void modeLSDJMidioutSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,OUTPUT);
  digitalWrite(pinGBClock,HIGH);

  countGbClockTicks=0;
  lastMidiData[0] = -1;
  lastMidiData[1] = -1;
  midiValueMode = false;
  blinkMaxCount=60;
  modeLSDJMidiout();
}

void modeLSDJMidiout()
{
  while(1) {
     if(getIncommingSlaveByte()) {
        if(incomingMidiByte > 0x6f) {
          switch(incomingMidiByte)
          {
            case 0x7F: //clock tick
              sMIDI.sendRealTime(midi::Clock);
              #ifdef HAS_USB_MIDI
                uMIDI.sendRealTime(midi::Clock);
              #endif
              break;
            case 0x7E: //seq stop
              sMIDI.sendRealTime(midi::Stop);
              #ifdef HAS_USB_MIDI
                uMIDI.sendRealTime(midi::Stop);
              #endif
              stopAllNotes();
              break;
            case 0x7D: //seq start
              sMIDI.sendRealTime(midi::Start);
              #ifdef HAS_USB_MIDI
                uMIDI.sendRealTime(midi::Start);
              #endif
              break;
            default:
              midiData[0] = (incomingMidiByte - 0x70);
              midiValueMode = true;
              break;
          }
        } else if (midiValueMode == true) {
          midiValueMode = false;
          midioutDoAction(midiData[0],incomingMidiByte);
        }

      } else {
        setMode();                // Check if mode button was depressed
        updateBlinkLights();
        // read from MIDI to detect programmer messages
        sMIDI.read();
        #ifdef HAS_USB_MIDI
          while(uMIDI.read()) ;
        #endif
      }
   }
}

void midioutDoAction(byte m, byte v)
{
  if(m < 4) {
    //note message
    if(v) {
      checkStopNote(m);
      playNote(m,v);
    } else if (midiOutLastNote[m]>=0) {
      stopNote(m);
    }
  } else if (m < 8) {
    m-=4;
    //cc message
    playCC(m,v);
  } else if(m < 0x0C) {
    m-=8;
    playPC(m,v);
  }
  blinkLight(0x90+m,v);
}

void checkStopNote(byte m)
{
  if((midioutNoteTimer[m]+midioutNoteTimerThreshold) < millis()) {
    stopNote(m);
  }
}

void stopNote(byte m)
{
  for(int x=0;x<midioutNoteHoldCounter[m];x++) {
    sMIDI.sendNoteOff(midioutNoteHold[m][x], 0, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #ifdef HAS_USB_MIDI
      uMIDI.sendNoteOff(midioutNoteHold[m][x], 0, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #endif
  }
  midiOutLastNote[m] = -1;
  midioutNoteHoldCounter[m] = 0;
}

void playNote(byte m, byte n)
{
  sMIDI.sendNoteOn(n, 127, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
  #ifdef HAS_USB_MIDI
    uMIDI.sendNoteOn(n, 127, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
  #endif

  midioutNoteHold[m][midioutNoteHoldCounter[m]] =n;
  midioutNoteHoldCounter[m]++;
  midioutNoteTimer[m] = millis();
  midiOutLastNote[m] =n;
}

void playCC(byte m, byte n)
{
  byte v = n;

  if(memory[MEM_MIDIOUT_CC_MODE+m]) {
    if(memory[MEM_MIDIOUT_CC_SCALING+m]) {
      v = (v & 0x0F)*8;
      //if(v) v --;
    }
    n=(m*7)+((n>>4) & 0x07);
    sMIDI.sendControlChange((memory[MEM_MIDIOUT_CC_NUMBERS+n]), v, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #ifdef HAS_USB_MIDI
      uMIDI.sendControlChange((memory[MEM_MIDIOUT_CC_NUMBERS+n]), v, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #endif
  } else {
    if(memory[MEM_MIDIOUT_CC_SCALING+m]) {
      float s;
      s = n;
      v = ((s / 0x6f) * 0x7f);
    }
    n=(m*7);
    
    sMIDI.sendControlChange((memory[MEM_MIDIOUT_CC_NUMBERS+n]), v, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #ifdef HAS_USB_MIDI
      uMIDI.sendControlChange((memory[MEM_MIDIOUT_CC_NUMBERS+n]), v, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #endif
  }
}

void playPC(byte m, byte n)
{
  sMIDI.sendProgramChange(n, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
  #ifdef HAS_USB_MIDI
    uMIDI.sendProgramChange(n, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
  #endif
}

void stopAllNotes()
{
  for(int m=0;m<4;m++) {
    if(midiOutLastNote[m]>=0) {
      stopNote(m);
    }
    sMIDI.sendControlChange(123, 127, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #ifdef HAS_USB_MIDI
      uMIDI.sendControlChange(123, 127, memory[MEM_MIDIOUT_NOTE_CH+m]+1);
    #endif
  }
}

boolean getIncommingSlaveByte()
{
  delayMicroseconds(midioutBitDelay);
  GB_SET(0,0,0);
  delayMicroseconds(midioutBitDelay);
  GB_SET(1,0,0);
  delayMicroseconds(2);
  if(digitalRead(pinGBSerialIn)) {
    incomingMidiByte = 0;
    for(countClockPause=0;countClockPause!=7;countClockPause++) {
      GB_SET(0,0,0);
      delayMicroseconds(2);
      GB_SET(1,0,0);
      incomingMidiByte = (incomingMidiByte << 1) + digitalRead(pinGBSerialIn);
    }
    return true;
  }
  return false;
}






















