#include <Audio.h>
#include "MyDsp.h"

MyDsp voices[4];
AudioMixer4 mixer;
AudioOutputI2S out;
AudioControlSGTL5000 audioShield;

AudioConnection p0(voices[0], 0, mixer, 0);
AudioConnection p1(voices[1], 0, mixer, 1);
AudioConnection p2(voices[2], 0, mixer, 2);
AudioConnection p3(voices[3], 0, mixer, 3);
AudioConnection pMixL(mixer, 0, out, 0);
AudioConnection pMixR(mixer, 0, out, 1);

const int POWER_BUTTON_PIN = 2;
bool isPoweredOn = true;
float potVolume = 0.5;
float distortionAmount = 0.0;
float pitchBendMod = 1.0;
unsigned long lastSerialMs = 0;
const unsigned long SERIAL_INTERVAL_MS = 50;

void setup() {
  Serial.begin(115200);
  AudioMemory(120);
  audioShield.enable();
  audioShield.volume(0.5);
  for(int i=0; i<4; i++) mixer.gain(i, 0.25);
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(POWER_BUTTON_PIN);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    isPoweredOn = !isPoweredOn;
    if (!isPoweredOn) for(int i=0; i<4; i++) voices[i].noteOff();
    delay(50);
  }
  lastButtonState = currentButtonState;

  if (isPoweredOn) {
    float newVol = analogRead(A0) / 1023.0;
    if (abs(newVol - potVolume) > 0.01) {
      potVolume = newVol;
      audioShield.volume(potVolume);
    }

    if (usbMIDI.read()) {
      byte type = usbMIDI.getType();
      byte note = usbMIDI.getData1();
      byte vel  = usbMIDI.getData2();

      if (type == usbMIDI.NoteOn && vel > 0) {
        int vIdx = -1;
        for (int i=0; i<4; i++) if (!voices[i].NoteActive && !voices[i].NoteReleased) { vIdx=i; break; }
        if (vIdx == -1) vIdx = 0;
        voices[vIdx].setFreq(440.0 * pow(2.0, (note - 69) / 12.0));
        voices[vIdx].setMidiNote(note);
        voices[vIdx].noteOn(vel);
      } 
      else if (type == usbMIDI.NoteOff || (type == usbMIDI.NoteOn && vel == 0)) {
        for (int i=0; i<4; i++) if (voices[i].getMidiNote() == note) voices[i].noteOff();
      }
      else if (type == usbMIDI.PitchBend) {
        int bend = usbMIDI.getData1() + (usbMIDI.getData2() << 7);
        pitchBendMod = powf(2.0f, ((bend - 8192.0f) / 8192.0f * 2.0f) / 12.0f);
        for(int i=0; i<4; i++) voices[i].setPitchBend(pitchBendMod);
      }
    }
  }

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.startsWith("DRUM:")) {
      String drum = input.substring(5);
      if (drum == "kick") voices[0].triggerKick();
      else if (drum == "snare") voices[0].triggerSnare();
      else if (drum == "hihat") voices[0].triggerHihat();
      else if (drum == "tom") voices[0].triggerTom();
      else if (drum == "cowbell") voices[0].triggerCowbell();
    } else {
      float val = input.substring(2).toFloat();
      if      (input.startsWith("D:"))  { distortionAmount = val; for(int i=0; i<4; i++) voices[i].setDist(val); }
      else if (input.startsWith("W:"))  { for(int i=0; i<4; i++) voices[i].setWaveform((int)val); }
      else if (input.startsWith("A:"))  { for(int i=0; i<4; i++) voices[i].setAttack(val); }
      else if (input.startsWith("R:"))  { for(int i=0; i<4; i++) voices[i].setRelease(val); }
      else if (input.startsWith("VB:")) { for(int i=0; i<4; i++) voices[i].setVibrato(val); }
    }
  }

  unsigned long now = millis();
  if (now - lastSerialMs >= SERIAL_INTERVAL_MS) {
      lastSerialMs = now;
      Serial.printf("V:%.3f,N:%d,N2:%d,N3:%d,N4:%d,G:0.800,D:%.3f,B:%.3f\n", 
                    potVolume, voices[0].getMidiNote(), voices[1].getMidiNote(), 
                    voices[2].getMidiNote(), voices[3].getMidiNote(), distortionAmount, pitchBendMod);
  }
}
