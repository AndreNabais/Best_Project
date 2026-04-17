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

const int POWER_BUTTON_PIN = 0;  // Power on Pin 0
const int VOLUME_POT_PIN = A0;   // Volume on A0
const int PITCH_BEND_PIN = A2;   // Bending on A2

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
  audioShield.volume(1.0);
  for(int i=0; i<4; i++) mixer.gain(i, 0.22f); 
  
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP); // Using internal pullup for Pin 0
}

void loop() {
  // 1. Power Logic (Pin 0)
  bool currentButtonState = digitalRead(POWER_BUTTON_PIN);
  static bool lastButtonState = HIGH;
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    isPoweredOn = !isPoweredOn;
    if (!isPoweredOn) {
      for(int i=0; i<4; i++) voices[i].noteOff();
    }
    delay(150); // Debounce
  }
  lastButtonState = currentButtonState;

  if (isPoweredOn) {
    // 2. Pots
    float v = analogRead(VOLUME_POT_PIN) / 1023.0;
    if (abs(v - potVolume) > 0.01) { potVolume = v; audioShield.volume(v); }

    int b = analogRead(PITCH_BEND_PIN);
    float nb = powf(2.0f, ((b - 512.0f) / 512.0f * 2.0f) / 12.0f);
    if (abs(nb - pitchBendMod) > 0.01) {
        pitchBendMod = nb;
        for(int i=0; i<4; i++) voices[i].setPitchBend(nb);
    }

    // 3. MIDI
    if (usbMIDI.read()) {
      byte type = usbMIDI.getType();
      byte note = usbMIDI.getData1();
      byte vel  = usbMIDI.getData2();
      if (type == usbMIDI.NoteOn && vel > 0) {
        int vIdx = -1;
        for (int i=0; i<4; i++) if (!voices[i].NoteActive && !voices[i].NoteReleased) { vIdx = i; break; }
        if (vIdx == -1) vIdx = 0;
        voices[vIdx].setFreq(440.0 * pow(2.0, (note - 69) / 12.0));
        voices[vIdx].setMidiNote(note);
        voices[vIdx].noteOn(vel);
      } 
      else if (type == usbMIDI.NoteOff || (type == usbMIDI.NoteOn && vel == 0)) {
        for (int i=0; i<4; i++) if (voices[i].getMidiNote() == note) voices[i].noteOff();
      }
    }
  }

  // 4. Commands & Metronome Triggers
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.startsWith("DRUM:")) {
    String d = input.substring(5);
    if (d == "m_h") for(int i=0; i<4; i++) voices[i].triggerMetroHigh();
    if (d == "m_l") for(int i=0; i<4; i++) voices[i].triggerMetroLow();
    if (d == "kick")  voices[0].triggerKick();
    if (d == "snare") voices[0].triggerSnare();
    if (d == "hihat") voices[0].triggerHihat();
    if (d == "tom") voices[0].triggerTom();
    if (d == "cowbell") voices[0].triggerCowbell();
}else {
      int cIdx = input.indexOf(':');
      if (cIdx != -1) {
        float val = input.substring(cIdx + 1).toFloat();
        if (input.startsWith("D:")) { distortionAmount = val; for(int i=0; i<4; i++) voices[i].setDist(val); }
        else if (input.startsWith("W:")) { for(int i=0; i<4; i++) voices[i].setWaveform((int)val); }
      }
    }
  }

  // 5. Sync
  if (millis() - lastSerialMs >= SERIAL_INTERVAL_MS) {
      lastSerialMs = millis();
      Serial.printf("V:%.3f,N:%d,N2:%d,N3:%d,N4:%d,G:0.800,D:%.3f,B:%.3f\n", 
              potVolume, 
              voices[0].getMidiNote(), 
              voices[1].getMidiNote(), 
              voices[2].getMidiNote(), 
              voices[3].getMidiNote(), 
              distortionAmount, 
              pitchBendMod);
  }
}
