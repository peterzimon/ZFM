#include <Arduino.h>
#include <MozziGuts.h>
#include <Oscil.h>

// Oscillator tables
#include <tables/sin1024_int8.h>
#include <tables/triangle1024_int8.h>
#include <tables/saw1024_int8.h>
#include <tables/square_analogue512_int8.h>
#include <tables/whitenoise8192_int8.h>

#include <Smooth.h>
#include <AutoMap.h> // maps unpredictable inputs to a range

#define CONTROL_RATE 64

// Desired carrier frequency max and min, for AutoMap -- TEMPORARY
const int MIN_CARRIER_FREQ = 22;
const int MAX_CARRIER_FREQ = 880;

// desired intensity max and min, for AutoMap
const int MIN_INTENSITY = 10;
const int MAX_INTENSITY = 700;

// desired mod speed max and min, for AutoMap
const int MIN_MOD_SPEED = 1;
const int MAX_MOD_SPEED = 5000;

// whitenoise
const int MAX_NOISE_INTENSITY = 32;

AutoMap kMapCarrierFreq(0,1023,MIN_CARRIER_FREQ,MAX_CARRIER_FREQ); // TEMPORARY
AutoMap kMapIntensity(0,1023,MIN_INTENSITY,MAX_INTENSITY);
AutoMap kMapModSpeed(0,1023,MIN_MOD_SPEED,MAX_MOD_SPEED);

const int CAR_FREQ_PIN = A5; // TEMPORARY for testing without MIDI controller

const int MOD_WAVEFORM_PIN = A0;
const int MOD_FREQ_PIN = A1;
const int MOD_INTENSITY_PIN = A2;
const int CAR_WAVEFORM_PIN = A3;
const int NOISE_LEVEL_PIN = A4;

Oscil<SIN1024_NUM_CELLS, AUDIO_RATE> aCarrierVCO(SIN1024_DATA);
Oscil<SIN1024_NUM_CELLS, AUDIO_RATE> aModVCO(SIN1024_DATA);
Oscil<SIN1024_NUM_CELLS, CONTROL_RATE> kIntensityMod(SIN1024_DATA);
Oscil<WHITENOISE8192_NUM_CELLS, AUDIO_RATE> aWhiteNoise(WHITENOISE8192_DATA);

int modRatio = 5; // Brightness (harmonics)
long fmIntensity; // Carries control info from updateControl to updateAudio

// smoothing for intensity to remove clicks on transitions
float smoothness = 0.95f;
Smooth <long> aSmoothIntensity(smoothness);

byte carrierWavePosition, modWavePosition, whiteNoiseVolume;

void setup(){
  // Serial.begin(9600);
  startMozzi(CONTROL_RATE);
  aWhiteNoise.setFreq(440); // White noise can be fixed frequency
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

void updateControl() {
  
  /* CARRIER
  /* ---------------------------------------------------------------- */

  // Read frequency (temporary, for testing)
  int freqKnobValue = mozziAnalogRead(CAR_FREQ_PIN); // value is 0-1023

  // Map the knob to carrier frequency
  int carrierFreq = kMapCarrierFreq(freqKnobValue);
  aCarrierVCO.setFreq(carrierFreq);


  /* MODULATOR
  /* ---------------------------------------------------------------- */
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

  
  /* WHITENOISE
  /* ---------------------------------------------------------------- */
  // Read noise level pot
  int whiteNoiseLevel = mozziAnalogRead(NOISE_LEVEL_PIN);
  whiteNoiseVolume = map(whiteNoiseLevel, 0, 1023, 0, MAX_NOISE_INTENSITY);
}

int updateAudio(){
  long modulation = aSmoothIntensity.next(fmIntensity) * aModVCO.next();

  // All of the signals have to be multiplied by their volume, then shift by 8 bit (256)
  return ((aCarrierVCO.phMod(modulation) * 255) + (aWhiteNoise.next() * whiteNoiseVolume))>>8;
}

void loop(){
  audioHook();
}