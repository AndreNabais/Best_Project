#include <Audio.h>
#include "MyDsp.h"

MyDsp myDsp;
AudioOutputI2S out;
AudioControlSGTL5000 audioShield;
AudioConnection patchCord0(myDsp, 0, out, 0);
AudioConnection patchCord1(myDsp, 0, out, 1);

void setup() {
  AudioMemory(20);
  audioShield.enable();
  audioShield.volume(0.5);
}

void loop() {
  if (usbMIDI.read()) {
    byte type     = usbMIDI.getType();
    byte note     = usbMIDI.getData1();
    byte velocity = usbMIDI.getData2();

    if (type == usbMIDI.NoteOn && velocity > 0) {
      float freq = 440.0 * pow(2.0, (note - 69) / 12.0);
      myDsp.setFreq(freq);
      myDsp.setGain(velocity / 127.0);
    }

    if (type == usbMIDI.NoteOff || (type == usbMIDI.NoteOn && velocity == 0)) {
      myDsp.setGain(0.0);
    }
  }
}
