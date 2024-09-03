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

void modeLSDJMasterSyncSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,INPUT);

  countSyncTime=0;
  blinkMaxCount=1000;
  modeLSDJMasterSync();
}

void modeLSDJMasterSync()
{
  while (1) {
    // continue to read from MIDI so that the programmer handler is called
    #ifdef HAS_USB_MIDI
    while (uMIDI.read()) ;
    #endif
    sMIDI.read();

    readGbClock();
    sendMidiClockSlaveFromLSDJ();                //send the clock & start offset data to midi
    setMode();
  }
}

void readGbClock() {
  readgbClockLine = digitalRead(pinGBClock);    //Read gameboy's clock line
  if(readgbClockLine) {                         //If Gb's Clock is On
    while(readgbClockLine) {                    //Loop untill its off
      readgbClockLine = digitalRead(pinGBClock);//Read the clock again
      bit = digitalRead(pinGBSerialIn);         //Read the serial input for song position
      checkActions();
    }

    countClockPause= 0;                          //Reset our wait timer for detecting a sequencer stop

    readGbSerialIn = readGbSerialIn << 1;        //left shift the serial byte by one to append new bit from last loop
    readGbSerialIn = readGbSerialIn + bit;       //and then add the bit that was read
  }
}

void checkActions()
{
  checkLSDJStopped();                        //Check if LSDJ hit Stop
  setMode();
  updateStatusLight();
}

 /*
  checkLSDJStopped counts how long the clock was on, if its been on too long we assume
  LSDJ has stopped- Send a MIDI transport stop message and return true.
 */
boolean checkLSDJStopped()
{
  countClockPause++;                            //Increment the counter
  if(countClockPause > 16000) {                 //if we've reached our waiting period
    if(sequencerStarted) {
      readgbClockLine=false;
      countClockPause = 0;                      //reset our clock
      sMIDI.sendRealTime(midi::Stop);
      #ifdef HAS_USB_MIDI
        uMIDI.sendRealTime(midi::Stop);
      #endif

      sequencerStop();                          //call the global sequencer stop function
    }
    return true;
  }
  return false;
}

#define VELOCITY_MAX 127

 /*
  sendMidiClockSlaveFromLSDJ waits for 8 clock bits from LSDJ,
  sends the transport start command if sequencer hasnt started yet,
  sends the midi clock tick, and sends a note value that corrisponds to
  LSDJ's row number on start (LSDJ only sends this once when it starts)
 */
void sendMidiClockSlaveFromLSDJ()
{
  if(!countGbClockTicks) {      //If we hit 8 bits
    if(!sequencerStarted) {         //If the sequencer hasnt started
      //Send the row value as a note
      sMIDI.sendNoteOn(readGbSerialIn, VELOCITY_MAX, memory[MEM_LSDJMASTER_MIDI_CH]+1);
      sMIDI.sendRealTime(midi::Start);

      #ifdef HAS_USB_MIDI
        uMIDI.sendNoteOn(readGbSerialIn, VELOCITY_MAX, memory[MEM_LSDJMASTER_MIDI_CH]+1);
        uMIDI.sendRealTime(midi::Start);
      #endif

      sequencerStart();             //call the global sequencer start function
    }
    sMIDI.sendRealTime(midi::Clock);
    #ifdef HAS_USB_MIDI
      uMIDI.sendRealTime(midi::Clock);
    #endif

    countGbClockTicks=0;            //Reset the bit counter
    readGbSerialIn = 0x00;                //Reset our serial read value

    updateVisualSync();
  }
  countGbClockTicks++;              //Increment the bit counter
 if(countGbClockTicks==8) countGbClockTicks=0;
}
