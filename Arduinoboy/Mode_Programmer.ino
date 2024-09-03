#define MSG_CONNECT 72
#define MSG_HEARTBEAT 0x7F
#define MSG_CONNECT_REQ 64
#define MSG_CONNECT_REQ_ACK 65
#define MSG_CONNECTED 66

#define MSG_SETTINGS 0x40
#define MSG_MODE 0x41

#define MSG_SETTNGS_SET 70
#define MSG_SETTINGS_RESET 71
#define MSG_SETTNGS_GET 76
#define MSG_MODE_GET 73
#define MSG_MODE_SET 74
#define MSG_MIDI_OUT_DELAY_SET 75

void modeProgrammer()
{ 
  while(sysexProgrammingConnected || sysexProgrammingMode) {
    checkProgrammerConnected();

    modeProgrammerSerialMidiReceive();
    modeProgrammerUsbMidiReceive();
    
    updateProgrammerLeds();
    setMode();
  }
  showSelectedMode();
  switchMode();
}

void modeProgrammerSerialMidiReceive() {
  // while in programmer mode, we still need to read the MIDI, but the
  // programmer handles all of the messages in systemExclusiveHandler
  if (!sMIDI.read()) { return; }
}

void modeProgrammerUsbMidiReceive() {
  #ifdef HAS_USB_MIDI
    // same here, read the midi data, messages handled in systemExclusiveHandler
    while (uMIDI.read()) ;
  #endif
}

void setProgrammerConnected()
{
  sysexProgrammerLastResponse = millis();
  if(!sysexProgrammingConnected) {
    programmerSendSettings();
  }
  sysexProgrammingConnected = 1;
}

void checkProgrammerConnected()
{
  programmerSendHeartbeat();
  programmerCheckTimeout();
}

void programmerSendSettings()
{
  sysexData[0] = midi::SystemExclusive;
  sysexData[1] = SYSEX_MFG_ID;
  sysexData[2] = MSG_SETTINGS;
  memcpy(&sysexData[3], memory, MEM_MAX);
  sysexData[MEM_MAX + 3] = midi::SystemExclusiveEnd;
  
  sMIDI.sendSysEx(MEM_MAX + 4, sysexData, true /* includes headers */);
  #ifdef HAS_USB_MIDI
    uMIDI.sendSysEx(MEM_MAX + 4, sysexData, true /* includes headers */);
  #endif
}

void setProgrammerRequestConnect()
{
  // Serial.println("<< MSG_CONNECT_REQ_ACK");
  uint8_t data[] = {midi::SystemExclusive,SYSEX_MFG_ID,MSG_CONNECT_REQ_ACK,midi::SystemExclusiveEnd};
  sMIDI.sendSysEx(sizeof(data), data, true /* includes headers */);
  #ifdef HAS_USB_MIDI
    uMIDI.sendSysEx(sizeof(data), data, true /* includes headers */);
  #endif
}

void saveSettings()
{
  byte sysexOffset = 2;
  for(byte m = MEM_FORCE_MODE; m < MEM_MAX; m++) {
    memory[m] = sysexData[sysexOffset];
    sysexOffset++;
  }
  saveMemory();
  loadMemory();
  programmerSendSettings();
}

void resetSettings()
{
  initMemory(true /* reinit */);
  programmerSendSettings();
}

void programmerCheckTimeout()
{
  if((sysexProgrammingMode || sysexProgrammingConnected) && millis() > (sysexProgrammerLastResponse + PROGRAMMER_MAX_WAIT_TIME)) {
    //programmer timeout!
    sysexProgrammingConnected = 0;
    sysexProgrammingMode      = 0;
  }
}

void programmerSendHeartbeat()
{
  if(millis() > (sysexProgrammerLastSent + PROGRAMMER_MAX_CALL_TIME)) {
    // Serial.println("<< MSG_HEARTBEAT");
    uint8_t data[] = {midi::SystemExclusive, SYSEX_MFG_ID, MSG_HEARTBEAT, defaultMemoryMap[MEM_VERSION_FIRST], defaultMemoryMap[MEM_VERSION_SECOND], midi::SystemExclusiveEnd};
    sMIDI.sendSysEx(sizeof(data), data, true /* includes headers */);
    #ifdef HAS_USB_MIDI
      uMIDI.sendSysEx(sizeof(data), data, true /* includes headers */);
    #endif
    sysexProgrammerLastSent = millis();
  }
}

boolean checkSysexChecksum()
{
  byte checksum = sysexData[(sysexPosition - 1)];
  byte checkdata= 0;
  if(checksum) {
    for(int x=2;x!=(sysexPosition - 2);x++) {
      checkdata += sysexData[x];
    }
    if(checkdata & 0x80) checkdata -= 0x7F;
    if(checkdata == checksum) {
      return true;
    }
  }
  return true;
}


void clearSysexBuffer()
{
  for(int x=0;x!=sysexPosition;x++) {
    sysexData[x]= 0;
  }
  sysexPosition = 0;
}

void setMode(byte mode)
{
  memory[MEM_MODE] = mode;
  #ifndef USE_DUE
  EEPROM.write(MEM_MODE, memory[MEM_MODE]);
  #endif
  showSelectedMode();
  switchMode();
}

void sendMode()
{
  uint8_t data[] = {midi::SystemExclusive, SYSEX_MFG_ID, MSG_MODE, memory[MEM_MODE], midi::SystemExclusiveEnd};
  sMIDI.sendSysEx(sizeof(data), data, true /* includes headers */);
  #ifdef HAS_USB_MIDI
    uMIDI.sendSysEx(sizeof(data), data, true /* includes headers */);
  #endif
}

void setMidioutDelay(byte a,byte b,byte c,byte d)
{
  memory[MEM_MIDIOUT_BIT_DELAY] = a;
  memory[MEM_MIDIOUT_BIT_DELAY+1] = b;
  memory[MEM_MIDIOUT_BYTE_DELAY] = c;
  memory[MEM_MIDIOUT_BYTE_DELAY+1] = d;
  saveMemory();
  changeTasks();
}

/** Handle programmer messages in the sysexData payload */
void handleProgrammerMessage()
{
  if(sysexData[0] == SYSEX_MFG_ID && checkSysexChecksum()) {
    const byte message = sysexData[1];

    //sysex good, do stuff
    sysexPosition = 0;
    if(sysexProgrammingMode) {
      if(message == MSG_CONNECT_REQ && versionsMatch()) {
        // Serial.println(">> MSG_CONNECT_REQ");
        setProgrammerRequestConnect();
      }
      if(message == MSG_CONNECTED && versionsMatch()) {
        // Serial.println(">> MSG_CONNECTED");
        setProgrammerConnected();
      }
      if(message == MSG_SETTNGS_SET) {
        // Serial.println(">> MSG_SETTNGS_SET");
        saveSettings();
      }
      if(message == MSG_SETTNGS_GET) {
        // Serial.println(">> MSG_SETTNGS_GET");
        programmerSendSettings();
      }
      if(message == MSG_SETTINGS_RESET) {
        // Serial.println(">> MSG_SETTINGS_RESET");
        resetSettings();
      }
    }
    if(message == MSG_CONNECT) {
      // Serial.println(">> MSG_CONNECT");
      sysexProgrammingMode = true;
      sysexProgrammerLastResponse = millis();
      modeProgrammer();
    }
    if(message == MSG_MODE_GET) {
      // Serial.println(">> MSG_MODE_GET");
      sendMode();
    }
    if(message == MSG_MODE_SET) {
      // Serial.println(">> MSG_MODE_SET");
      setMode(sysexData[2]);
    }
    if(message == MSG_MIDI_OUT_DELAY_SET) {
      // Serial.println(">> MSG_MIDI_OUT_DELAY_SET");
      setMidioutDelay(sysexData[2],sysexData[3],sysexData[4],sysexData[5]);
    }
  }
  clearSysexBuffer();
}

boolean checkForProgrammerSysex(byte sin)
{
    if(sin == 0xF0) {
        sysexReceiveMode = true;
        sysexPosition= 0;
        return true;
    } else if (sin == 0xF7 && sysexReceiveMode) {
        sysexReceiveMode = false;
        handleProgrammerMessage();
        sysexPosition= 0;
        return true;
    } else if (sysexReceiveMode == true) {
        sysexData[sysexPosition] = sin;
        sysexPosition++;
        if(sysexPosition > longestSysexMessage) {
            clearSysexBuffer();
            sysexReceiveMode = false;
        }
        return true;
    }
    return false;
}

void blinkSelectedLight(int led)
{
      if(!blinkSwitch[led]) digitalWrite(pinLeds[led],HIGH);
      blinkSwitch[led]=1;
      blinkSwitchTime[led]=0;
}

void initProgrammerSysexHandlers() {
#ifdef HAS_USB_MIDI
  uMIDI.setHandleSystemExclusive(systemExclusiveHandler);
#endif
  sMIDI.setHandleSystemExclusive(systemExclusiveHandler);
}

/** 
 * Prepares a SysEx array for the programmer. It expects sysexData to be populated with
 * the sysex array, without the header and EOF bytes.
 */
void systemExclusiveHandler(unsigned char *sysexArray, unsigned int sysexLength) {
  // handle programmer. Copy payload without SysEx header and EOF
  memcpy(&sysexData[0], &sysexArray[1], sysexLength - 2);
  handleProgrammerMessage();
}

bool versionsMatch() {
  if (sysexData[2] == defaultMemoryMap[MEM_VERSION_FIRST] && sysexData[3] == defaultMemoryMap[MEM_VERSION_SECOND]) {
    return true;
  }

  // Serial.printf(
  //   "Version mismatch: received %d . %d expected %d . %d\n", sysexData[2], sysexData[3], defaultMemoryMap[MEM_VERSION_FIRST], defaultMemoryMap[MEM_VERSION_SECOND]);
  return false;
}