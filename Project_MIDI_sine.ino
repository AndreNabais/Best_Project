#include <Audio.h>
#include "MyDsp.h"

MyDsp myDsp;
AudioOutputI2S out;
AudioControlSGTL5000 audioShield;
AudioConnection patchCord0(myDsp, 0, out, 0);
AudioConnection patchCord1(myDsp, 0, out, 1);

// --- Hardware Pins ---
const int POWER_BUTTON_PIN = 2;

// --- State Variables ---
bool isPoweredOn = true;
int currentNote = -1;
float currentFreq = 0.0;
float currentGain = 0.0;
float potVolume = -1.0;
float distortionAmount = 0.0;

unsigned long lastSerialMs = 0;
const unsigned long SERIAL_INTERVAL_MS = 50; 

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5); // Important for fast GUI response
  
  pinMode(13, OUTPUT);
  pinMode(A0, INPUT);
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);

  AudioMemory(40);
  audioShield.enable();
  audioShield.volume(0.5);
}

void loop() {
  // 1. Power Button Logic (Toggle)
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(POWER_BUTTON_PIN);
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    isPoweredOn = !isPoweredOn; 
    if (!isPoweredOn) {
      myDsp.noteOff(); 
      currentNote = -2; // Send "Power Off" code to GUI
    } else {
      currentNote = -1; 
    }
    delay(50); // Simple debounce
  }
  lastButtonState = currentButtonState;

  // 2. Heartbeat LED
  static unsigned long lastBlink = 0;
  uint32_t blinkRate = isPoweredOn ? 500 : 100;
  if (millis() - lastBlink > blinkRate) {
    lastBlink = millis();
    digitalWrite(13, !digitalRead(13)); 
  }

  // 3. Audio & MIDI (Process only if On)
  if (isPoweredOn) {
    // Volume Potentiometer
    float newVol = analogRead(A0) / 1023.0;
    if (abs(newVol - potVolume) > 0.01) {
      potVolume = newVol;
      audioShield.volume(potVolume);
    }

    // MIDI Input
    if (usbMIDI.read()) {
      byte type = usbMIDI.getType();
      byte note = usbMIDI.getData1();
      byte velocity = usbMIDI.getData2();

      if (type == usbMIDI.NoteOn && velocity > 0) {
        currentNote = note;
        currentFreq = 440.0 * pow(2.0, (note - 69) / 12.0);
        currentGain = velocity / 127.0;
        myDsp.setFreq(currentFreq);
        myDsp.noteOn(velocity);
      } 
      else if (type == usbMIDI.NoteOff || (type == usbMIDI.NoteOn && velocity == 0)) {
        if (note == currentNote) {
          currentNote = -1;
          currentGain = 0.0;
          myDsp.noteOff();
        }
      }
    }
  }

  // 4. GUI Commands (Incoming from Python)
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    
    // Distortion Slider
    if (input.startsWith("D:")) {
      distortionAmount = input.substring(2).toFloat();
      myDsp.setDist(distortionAmount);
    }
    
    // Waveform Buttons
    if (input.startsWith("W:")) {
      int type = input.substring(2).toInt();
      myDsp.setWaveform(type);
    }

    // --- Add these two blocks ---
    if (input.startsWith("A:")) {
      float a = input.substring(2).toFloat();
      myDsp.setAttack(a);
    }
    
    if (input.startsWith("R:")) {
      float r = input.substring(2).toFloat();
      myDsp.setRelease(r);
    }
  }

  // 5. GUI Reporting (Outgoing to Python)
  unsigned long now = millis();
  if (now - lastSerialMs >= SERIAL_INTERVAL_MS) {
    lastSerialMs = now;
    // V=Vol, N=Note, F=Freq, G=Gain, D=Dist
    Serial.printf("V:%.3f,N:%d,F:%.2f,G:%.3f,D:%.3f\n", 
                  potVolume, currentNote, currentFreq, currentGain, distortionAmount);
  }
}
