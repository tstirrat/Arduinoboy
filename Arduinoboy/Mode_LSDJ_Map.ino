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

void modeLSDJMapSetup()
{
  digitalWrite(pinStatusLed,LOW);
  pinMode(pinGBClock,OUTPUT);
  digitalWrite(pinGBClock, HIGH);
  blinkMaxCount=1000;
  modeLSDJMap();
}

void modeLSDJMap()
{
  while (1) {
    modeLSDJMapUsbMidiReceive();
    checkMapQueue();

    if (!modeLSDJMapSerialMidiReceive()) {
      setMode();
      updateStatusLight();
      checkMapQueue();
      updateBlinkLights();
    }
  }
}

void setMapByte(uint8_t b, boolean usb)
{
    uint8_t wait = mapQueueWaitSerial;
    if(usb) {
        wait = mapQueueWaitUsb;
    }

    switch(b) {
      case midi::SystemReset:
        setMapQueueMessage(0xFF, wait);
        break;
      case midi::ActiveSensing:
        if(!sequencerStarted) {
            sendByteToGameboy(0xFE);
        } else if (mapCurrentRow >= 0) {
            setMapQueueMessage(mapCurrentRow, wait);
        }
        break;
      default:
        mapCurrentRow = b;
        sendByteToGameboy(b);
        resetMapCue();
    }
}

void setMapQueueMessage(uint8_t m, uint8_t wait)
{
    if(mapQueueMessage == -1 || mapQueueMessage == 0xFF) {
        mapQueueTime=millis()+wait;
        mapQueueMessage=m;
    }
}

void resetMapCue()
{
    mapQueueMessage=-1;
}

void checkMapQueue()
{
  if(mapQueueMessage >= 0 && millis()>mapQueueTime) {
      if(mapQueueMessage == 0xFF) {
          sendByteToGameboy(mapQueueMessage);
      } else {
          if(mapQueueMessage == 0xFE || mapCurrentRow == mapQueueMessage) {
              // Only kill playback if the row is the last one that's been played.
              mapCurrentRow = -1;
              sendByteToGameboy(0xFE);
          }
      }
      mapQueueMessage=-1;
      updateVisualSync();
  }
}

void modeLSDJMapUsbMidiReceive() {
#ifdef HAS_USB_MIDI
  while (uMIDI.read()) {

    byte type = uMIDI.getType();
    byte channel = uMIDI.getChannel() - 1;
    byte data1 = uMIDI.getData1();

    if (channel != memory[MEM_LIVEMAP_CH] && channel != (memory[MEM_LIVEMAP_CH] + 1)){
      continue;
    }

    handleLsdjMapMessage(type, channel, data1);
  }
#endif
}

bool modeLSDJMapSerialMidiReceive() {
  if (!sMIDI.read()) return false;

  byte type = sMIDI.getType();
  byte channel = sMIDI.getChannel() - 1;
  byte data1 = sMIDI.getData1();

  if (channel != memory[MEM_LIVEMAP_CH] && channel != (memory[MEM_LIVEMAP_CH] + 1)){
    return true;
  }

  handleLsdjMapMessage(type, channel, data1);

  checkMapQueue();
  return true;
}

void handleLsdjMapMessage(byte type, byte channel, byte data1) {
  switch(type) {
    case midi::NoteOff:
      setMapByte(0xFE, true);
    break;

    case midi::NoteOn:
      if (channel == (memory[MEM_LIVEMAP_CH] + 1)) {
        setMapByte(128 + data1, true);
      } else {
        setMapByte(data1, true);
      }
    break;
    
    // Realtime / clock transport stuff
    case midi::Clock:
      setMapByte(0xFF, true);
    break;
    case midi::Start:
    case midi::Continue:
      resetMapCue();
      sequencerStart();
    break;
    case midi::Stop:
      sequencerStop();
      setMapByte(0xFE, true);
    break;
  }
}