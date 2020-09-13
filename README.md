# ZFM - Arduino based FM synth using Mozzi library

### What's this?

Arduino based FM Synth using the Mozzi library

Note that I used [https://platformio.org](Platformio) for the development of this sketch but you can upload it using the standard Arduino development environment as well. To do so:

1. create a new sketch
2. copy and paste the contents of https://github.com/peterzimon/ZFM/blob/master/src/main.cpp to the sketch file
3. remove the `#include <Arduino.h>` line (line #22). It's only needed for Platformio

### How does it sound?

[VIDEO]

### Features

- single carrier, single modulator signal with waveform selection
- modulation speed and intensity knobs
- noise source
- notes reading either by MIDI input or knob + button (continuous frequency)

### More info

[https://www.peterzimon/](Here)