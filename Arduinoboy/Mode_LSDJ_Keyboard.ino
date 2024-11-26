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

void modeLSDJKeyboardSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,OUTPUT);
  digitalWrite(pinGBClock,HIGH);
  blinkMaxCount=1000;

  /* The stuff below makes sure the code is in the same state as LSDJ on reset / restart, mode switched, etc. */

  for(int rst=0;rst<5;rst++) sendKeyboardByteToGameboy(keyboardOctDn);   //Return lsdj to the first octave
  for(int rst=0;rst<41;rst++) sendKeyboardByteToGameboy(keyboardInsDn);  //Return lsdj to the first instrument

  keyboardCurrentOct = 0;  //Set our current octave to 0.
  keyboardLastOct    = 0;  //Set our last octave to 0.
  keyboardCurrentIns = 0;  //Set out current instrument to 0.
  keyboardLastIns    = 0;  //Set out last used instrument to 0.


  modeLSDJKeyboard();      //.... And start the fun
}


void modeLSDJKeyboard()
{
  while (1) {
    modeLSDJKeyboardUsbMidiReceive();
    modeLSDJKeyboardSerialMidiReceive();

    updateStatusLed();        // Update our status blinker
    setMode();                // Check if mode button was depressed
  }
}

void modeLSDJKeyboardUsbMidiReceive()
{
#ifdef HAS_USB_MIDI
  while(uMIDI.read(memory[MEM_KEYBD_CH]+1)) {

    byte type = uMIDI.getType();
    byte data1 = uMIDI.getData1();
    byte data2 = uMIDI.getData2();

    handleLSDJKeyboardMessage(type, data1, data2);
  }
#endif
}

void modeLSDJKeyboardSerialMidiReceive() {
  if (!sMIDI.read(memory[MEM_KEYBD_CH]+1)) return;

  byte type = sMIDI.getType();
  byte data1 = sMIDI.getData1();
  byte data2 = sMIDI.getData2();

  handleLSDJKeyboardMessage(type, data1, data2);
}

void handleLSDJKeyboardMessage(byte type, byte data1, byte data2) {
  switch(type) {
    case midi::NoteOff:
      playLSDJNote(data1, 0);
    break;
    case midi::NoteOn:
      playLSDJNote(data1, data2);
    break;
    case midi::ProgramChange:
      changeLSDJInstrument(midi::ProgramChange + memory[MEM_KEYBD_CH], data1);
    break;
  }
}


 /*
  changeLSDJInstrument does what it does via magic (rtfm, realize the fucking magic)
 */
void changeLSDJInstrument(byte channel,byte message)
{
  keyboardCurrentIns = message; //set the current instrument number

  if(channel == (midi::ProgramChange+memory[MEM_KEYBD_CH]) && keyboardCurrentIns != keyboardLastIns) {
  //if its on our midi channel and the instrument isnt the same as our currrent
    if(!memory[MEM_KEYBD_COMPAT_MODE]) {
      sendKeyboardByteToGameboy(midi::NoteOff | message); // <- this is suppose to work but doesn't :/
    } else {
      //We will find out which is greater, the current instrument or the last instrument. then
      //cycle up or down to that instrument
      if(keyboardCurrentIns > keyboardLastIns) {
        keyboardDiff = keyboardCurrentIns - keyboardLastIns;
        for(keyboardCount=0;keyboardCount<keyboardDiff;keyboardCount++) {
          sendKeyboardByteToGameboy(keyboardInsUp);  //send the instrument up command to go up to the requested instrument
        }
      } else {
        keyboardDiff = keyboardLastIns - keyboardCurrentIns;
        for(keyboardCount=0;keyboardCount<keyboardDiff;keyboardCount++) {
          sendKeyboardByteToGameboy(keyboardInsDn);  //send the instrument down command to down to the requested instrument
        }
      }
    }
    keyboardLastIns = keyboardCurrentIns; //assign our instrument for future reference to this function
  }
}


 /*
  w00t...
 */
void playLSDJNote(byte note, byte velocity)
{
  if(velocity > 0x00) { //If midi channel = ours and the velocity is greater then 0
    if(note >= keyboardNoteStart) {
      keyboardNoteOffset = 0;
      note = note - keyboardNoteStart;  //subtract the octave offset to get a value ranging from 0 to 48 for comparison

      keyboardCurrentOct = note / 0x0C;  //get a octave value from 0 to 4 by deviding the current note by 12
      changeLSDJOctave();

      if(note >= 0x3C) keyboardNoteOffset = 0x0C; //if the note really high we need to use the second row of keyboard keys
      note = (note % 12) + keyboardNoteOffset;    //get a 0 to 11 range of notes and add the offset
      sendKeyboardByteToGameboy(keyboardNotes[note]);        // and finally send the note

    } else if (note >= keyboardStartOctave) { //If we are at the octave below notes
      keyboardDiff = note - keyboardStartOctave; //Get a value between 0 and 11
      if(keyboardDiff < 8 && keyboardDiff > 3) sendKeyboardByteToGameboy(0xE0); //if we are sending cursor values we have to send a 0xE0 byte for "extended" pc keyboard mode
      sendKeyboardByteToGameboy(keyboardCommands[note - keyboardStartOctave]); //send the byte corrisponding to the note number in the keyboard command array
    }
  }
}


 /*
  changeLSDJOctave compares the last octave with the current one and then sends a byte
  to shift the octave to match if need be. its pretty much the same as the changeLSDJInstrument function.
 */
void changeLSDJOctave()
{
  if(keyboardCurrentOct != keyboardLastOct) {
    if(!memory[MEM_KEYBD_COMPAT_MODE]) { // This new mode doesnt work yet. :/
      keyboardCurrentOct = 0xB3 + keyboardCurrentOct;
      sendKeyboardByteToGameboy(keyboardCurrentOct);
    } else {
      ///We will find out which is greater, the current octave or the last. then
      //cycle up or down to that octave
      if(keyboardCurrentOct > keyboardLastOct) {
        keyboardDiff = keyboardCurrentOct - keyboardLastOct;
        for(keyboardCount=0;keyboardCount<keyboardDiff;keyboardCount++)
        {
           sendKeyboardByteToGameboy(keyboardOctUp);
        }
      } else {
        keyboardDiff = keyboardLastOct - keyboardCurrentOct;
        for(keyboardCount=0;keyboardCount<keyboardDiff;keyboardCount++)
        {
          sendKeyboardByteToGameboy(keyboardOctDn);
        }
      }
    }
    keyboardLastOct = keyboardCurrentOct;
  }
}


/*
  sendKeyboardByteToGameboy
*/
void sendKeyboardByteToGameboy(byte send_byte)
{
  GB_SET(0,0,0);
  delayMicroseconds(50);  //Delays are added to simulate PC keyboard rate
  GB_SET(1,0,0);
  delayMicroseconds(50);
  for(countLSDJTicks=0;countLSDJTicks<8;countLSDJTicks++) {  //we are going to send 8 bits, so do a loop 8 times
    if(send_byte & 0x01) {          //if the first bit is equal to 1
      GB_SET(0,1,0);
      delayMicroseconds(50);
      GB_SET(1,1,0);
      delayMicroseconds(50);
    } else {
      GB_SET(0,0,0);
      delayMicroseconds(50);
      GB_SET(1,0,0);
      delayMicroseconds(50);
    }
    send_byte >>= 1;                //bitshift right once for the next bit we are going to send
  }

  GB_SET(0,0,0);
  delayMicroseconds(50);
  GB_SET(1,0,0);
  delayMicroseconds(50);
  GB_SET(0,1,0);
  delayMicroseconds(50);
  GB_SET(1,1,0);
  delayMicroseconds(4000);
}