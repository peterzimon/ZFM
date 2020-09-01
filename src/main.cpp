/*
FM synth
-----------------------------------------------------------------------

FM Synthesis sample code: 
https://github.com/sensorium/Mozzi/blob/master/examples/03.Sensors/Knob_LightLevel_x2_FMsynth/Knob_LightLevel_x2_FMsynth.ino

Midi sample code: 
https://github.com/sensorium/Mozzi/blob/master/examples/05.Control_Filters/MIDI_portamento/MIDI_portamento.ino
*/

#include <Arduino.h>
#include <MIDI.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <Line.h>
#include <mozzi_midi.h>
#include <ADSR.h>
#include <mozzi_fixmath.h>

MIDI_CREATE_DEFAULT_INSTANCE();

// Oscillator tables
// --------------------------------------------------------------------
#include <tables/sin1024_int8.h>
#include <tables/triangle1024_int8.h>
#include <tables/saw1024_int8.h>
#include <tables/square_analogue512_int8.h>
#include <tables/whitenoise8192_int8.h>

#include <Smooth.h>
#include <AutoMap.h> // maps unpredictable inputs to a range

// Constants
// --------------------------------------------------------------------
#define CONTROL_RATE 512
#define MIDI_LED 13
#define COARSE_BUTTON 12

// Pins
const int MOD_WAVEFORM_PIN = A0;    // Modulator waveform: sin, triangle, saw, square
const int MOD_FREQ_PIN = A1;        // Modulator frequency (speed)
const int MOD_INTENSITY_PIN = A2;   // Modulator intensity (level)
const int CAR_WAVEFORM_PIN = A3;    // Carrier waveform: sin, triangle, saw, square
const int NOISE_LEVEL_PIN = A4;     // Noise level
const int CAR_FREQ_PIN = A5;        // Coarse frequency

// Desired carrier frequency max and min, for AutoMap -- TEMPORARY
const int MIN_CARRIER_FREQ = 22;
const int MAX_CARRIER_FREQ = 880;

// Desired intensity max and min, for AutoMap
const int MIN_INTENSITY = 10;
const int MAX_INTENSITY = 700;

// Desired mod speed max and min, for AutoMap
const int MIN_MOD_SPEED = 1;
const int MAX_MOD_SPEED = 5000;

// Whitenoise
const int MAX_NOISE_INTENSITY = 64;


// Knobs
// --------------------------------------------------------------------
AutoMap kMapCarrierFreq(0,1023,MIN_CARRIER_FREQ,MAX_CARRIER_FREQ); // TEMPORARY
AutoMap kMapIntensity(0,1023,MIN_INTENSITY,MAX_INTENSITY);
AutoMap kMapModSpeed(0,1023,MIN_MOD_SPEED,MAX_MOD_SPEED);


// Oscillators
// --------------------------------------------------------------------
Oscil<SIN1024_NUM_CELLS, AUDIO_RATE> aCarrierVCO(SIN1024_DATA);
Oscil<SIN1024_NUM_CELLS, AUDIO_RATE> aModVCO(SIN1024_DATA);
Oscil<SIN1024_NUM_CELLS, CONTROL_RATE> kIntensityMod(SIN1024_DATA);
Oscil<WHITENOISE8192_NUM_CELLS, AUDIO_RATE> aWhiteNoise(WHITENOISE8192_DATA);


// Envelopes
// --------------------------------------------------------------------
ADSR <CONTROL_RATE, AUDIO_RATE> envelopeVCO, envelopeWN;


// Variables
// --------------------------------------------------------------------
int modRatio = 5; // Brightness (harmonics)
long fmIntensity; // Carries control info from updateControl to updateAudio

// smoothing for intensity to remove clicks on transitions
float smoothness = 0.95f;
Smooth <long> aSmoothIntensity(smoothness);

byte carrierWavePosition, modWavePosition, whiteNoiseVolume;
int carrierFreq;
unsigned int notesPlaying, coarseButtonValue;
bool coarseButtonPushed;


// Helpers
// -------------------------------------------------------------
void updateCarrierFreq(byte note) {
  carrierFreq = (int)mtof(note);
  aCarrierVCO.setFreq(carrierFreq);
}


void HandleNoteOn(byte channel, byte note, byte velocity) {
  if (coarseButtonValue == LOW) { // Play midi notes only if the coarse button is not pressed
    return;
  }
  updateCarrierFreq(note);
  envelopeVCO.noteOn();
  envelopeWN.noteOn();
  digitalWrite(MIDI_LED,HIGH);
  notesPlaying++;
}

void HandleNoteOff(byte channel, byte note, byte velocity) {
  notesPlaying--;
  if (notesPlaying <= 0) {
    envelopeVCO.noteOff();
    envelopeWN.noteOff();
    digitalWrite(MIDI_LED,LOW);
    notesPlaying = 0;
  }
}

void chooseCarrierTable(int waveNumber) {
  switch (waveNumber)
  {
  case 0:
    aCarrierVCO.setTable(SIN1024_DATA);
    break;
  
  case 1:
    aCarrierVCO.setTable(TRIANGLE1024_DATA);
    break;
  
  case 2:
    aCarrierVCO.setTable(SAW1024_DATA);
    break;

  case 3:
    aCarrierVCO.setTable(SQUARE_ANALOGUE512_DATA);
    break;
  }
}

void chooseModTable(int waveNumber) {
  switch (waveNumber)
  {
  case 0:
    aModVCO.setTable(SIN1024_DATA);
    break;
  
  case 1:
    aModVCO.setTable(TRIANGLE1024_DATA);
    break;
  
  case 2:
    aModVCO.setTable(SAW1024_DATA);
    break;

  case 3:
    aModVCO.setTable(SQUARE_ANALOGUE512_DATA);
    break;
  }
}


// Mozzi functions
// --------------------------------------------------------------------
void updateControl() {
  
  // MIDI
  MIDI.read();
  envelopeVCO.update();
  envelopeWN.update();
  
  // CARRIER
  // When coarse button is pushed the frequency of the carrier is set by the frequency knob
  coarseButtonValue = digitalRead(COARSE_BUTTON);
  if (coarseButtonValue == LOW) {  // Button pushed
    coarseButtonPushed = true;

    // Read frequency (temporary, for testing)
    int freqKnobValue = mozziAnalogRead(CAR_FREQ_PIN); // value is 0-1023

    // Map the knob to carrier frequency
    int carrierFreq = kMapCarrierFreq(freqKnobValue);
    aCarrierVCO.setFreq(carrierFreq);

    envelopeVCO.noteOn();
    envelopeWN.noteOn();
  } else {
    if (coarseButtonPushed) {
      envelopeVCO.noteOff();
      envelopeWN.noteOff();
      coarseButtonPushed = false;
    }
  }

  // MODULATOR
  // Calculate the modulation frequency to stay in ratio
  int modFreq = carrierFreq * modRatio;

  // set the FM oscillator frequencies
  aModVCO.setFreq(modFreq);

  // Read waveform pot and set carrier waveform accordingly
  int carrierWaveformValue = mozziAnalogRead(CAR_WAVEFORM_PIN);
  carrierWavePosition = map(carrierWaveformValue, 0, 1023, 0, 4);
  chooseCarrierTable(carrierWavePosition);

  // Read waveform pot and set modulation waveform accordingly
  int modWaveformValue = mozziAnalogRead(MOD_WAVEFORM_PIN);
  modWavePosition = map(modWaveformValue, 0, 1023, 0, 4);
  chooseModTable(modWavePosition);

  // Read modulation intensity knob
  int modIntensityKnobValue = mozziAnalogRead(MOD_INTENSITY_PIN); // value is 0-1023
  int modIntensityKnobCalibrated = kMapIntensity(modIntensityKnobValue);

  // Calculate the fm_intensity
  fmIntensity = ((long)modIntensityKnobCalibrated * (kIntensityMod.next()+128))>>8; // shift back to range after 8 bit multiply

  // Read speed input
  int modSpeedKnobValue = mozziAnalogRead(MOD_FREQ_PIN); // value is 0-1023

  // use a float here for low frequencies
  float modSpeed = (float)kMapModSpeed(modSpeedKnobValue)/1000;
  kIntensityMod.setFreq(modSpeed);

  // WHITENOISE
  // Read noise level pot
  int whiteNoiseLevel = mozziAnalogRead(NOISE_LEVEL_PIN);
  whiteNoiseVolume = map(whiteNoiseLevel, 0, 1023, 0, MAX_NOISE_INTENSITY);
}

int updateAudio(){
  long modulation = aSmoothIntensity.next(fmIntensity) * aModVCO.next();

  // All of the signals have to be multiplied by their volume, then shift by 8 bit (256)
  return ((envelopeVCO.next() * aCarrierVCO.phMod(modulation)) + (envelopeWN.next() * aWhiteNoise.next() * whiteNoiseVolume))>>8;
}


// Arduino funcitons
// --------------------------------------------------------------------
void setup() {
  
  // Set up serial output with standard MIDI baud rate
  // Serial.begin(31250);

  // Pins
  pinMode(MIDI_LED, OUTPUT);
  pinMode(COARSE_BUTTON, INPUT_PULLUP);

  // Initiate MIDI communications, listen to all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(HandleNoteOn);  // Put only the name of the function
  MIDI.setHandleNoteOff(HandleNoteOff);  // Put only the name of the function

  // Set up envelopes
  envelopeVCO.setADLevels(255,255);
  envelopeVCO.setTimes(10,2000,10000,10); // 10000 is so the note will sustain 10 seconds unless a noteOff comes

  envelopeWN.setADLevels(4,4);
  envelopeWN.setTimes(10,2000,10000,10); // 10000 is so the note will sustain 10 seconds unless a noteOff comes

  // Set default carrier frequence (MIDI note input)
  updateCarrierFreq(75);

  // Start Mozzi
  startMozzi(CONTROL_RATE);
  aWhiteNoise.setFreq(440); // White noise can be fixed frequency
}

void loop(){
  audioHook();
}