// IMPORTANT: define 'true' if this mcu lives inside of Box A, which is the wooden unit.
// define 'false' if this mcu lives inside of Box B, which is the black bakelite dial unit.
#define IS_SIDE_A false

#include <MD_YX5300.h>
#include <SoftwareSerial.h>
#include <ButtonDebounce.h>

// IO Constants
const short mp3_rx = 8; // D8 - connect to TX of MP3 Player module
const short mp3_tx = 7; // D7 - connect to RX of MP3 Player module
const short internal_on_hook_pin = 6; // D6 - internal on-hold input
const short external_on_hook_pin = 9; // D9 - external on-hold input
const short relay_audio_out_pin = 4; // D4 - relay coil for audio output
const short relay_audio_in_pin = 5; // D5 - relay coil for audio input

const short debounce_ms = 100;

// IO Objects
ButtonDebounce internalOH(internal_on_hook_pin, debounce_ms);
ButtonDebounce externalOH(external_on_hook_pin, debounce_ms);


// MP3 Objects
SoftwareSerial  MP3Stream(mp3_rx, mp3_tx);
MD_YX5300 mp3(MP3Stream);

#define PlayDialing() mp3.playSpecific(1,2);
#define PlayRinging() mp3.playSpecific(1,1);

// State logic
enum HookState
{
  Open, // Open circuit - no audio
  Calling, // Partial circuit - ringing the other end
  BeingCalled, // Partial circuit - being rung
  Closed, // Closed circuit - talking
};

const String HookStateNames[5] = {
  "Open",
  "Calling",
  "BeingCalled",
  "Closed"
};

HookState lastHookState = HookState::Open;

void setup()
{
  Serial.begin(9600);
  Serial.println("PaniQ Telephone Control Board A");

  // Pin setup
  pinMode(relay_audio_in_pin, OUTPUT);
  pinMode(relay_audio_out_pin, OUTPUT);
  // OH pins set to INTERNAL_PULLUP by ButtonDebounce constructor

  internalOH.setCallback([](const int state) {Serial.println("Internal = " + String(state));});
  externalOH.setCallback([](const int state) {Serial.println("External = " + String(state));});

  // Initialise player
  MP3Stream.begin(MD_YX5300::SERIAL_BPS);
  mp3.begin();
  mp3.setSynchronous(true);
}

void loop()
{
  mp3.check();
  updateButtons();

  // Check if hook state has changed
  auto currentState = checkHookState();

  if (currentState != lastHookState){
    if (lastHookState == HookState::Closed && currentState != HookState::Open){
      handleNewHookState(HookState::Open);
    } else {
      handleNewHookState(currentState);
      lastHookState = currentState;
      Serial.println("New State: " + HookStateNames[currentState]);
    }
  }
}

void setAudioRelays(bool closed) {
  digitalWrite(relay_audio_in_pin, closed);
  digitalWrite(relay_audio_out_pin, closed);
}

void updateButtons(){
  internalOH.update();
  externalOH.update();
}

void handleNewHookState(HookState state) {
  switch(state){
      case HookState::Closed:
        setAudioRelays(false);
        mp3.playStop();
        break;
      case HookState::BeingCalled:
        setAudioRelays(true);
        PlayRinging();
        break;
      case HookState::Calling:
        setAudioRelays(true);
        PlayDialing();
        break;
      case HookState::Open:
      default: 
        setAudioRelays(true);
        mp3.playStop();
        break;
    }
}

HookState checkHookState(){
  bool internal;
  bool external;

  if (IS_SIDE_A){
    internal = !internalOH.state();
    external = externalOH.state();
  } else {
    internal = internalOH.state();
    external = !externalOH.state();
  }
  

  if ( internal && external ) return HookState::Open;
  if ( internal && !external ) return HookState::BeingCalled;
  if ( !internal && external ) return HookState::Calling;
  return HookState::Closed;
}