#ifndef faust_teensy_h_
#define faust_teensy_h_

#include "Arduino.h"
#include "AudioStream.h"
#include "Audio.h"
#include "Phasor.h"

class MyDsp : public AudioStream
{
  public:
    MyDsp();
    ~MyDsp();
    
    virtual void update(void);
    
    // Control functions
    void setFreq(float freq);
    void setGain(float g);
    void setDist(float d);             // Update distortion threshold
    void setWaveform(int type);        // Switch between Sine, Tri, Saw, Square
    
    // MIDI / Envelope functions
    void Envelope(float attack_val, float release_val);
    void noteOn(float velocity);
    void noteOff();

    void setAttack(float a);
    void setRelease(float r);

    // Distortion threshold needs to be accessible for the math in .cpp
    float distortionThreshold;

  private:
    // Audio Generation
    Phasor phasor;
    int waveformType = 0;

    // Gain and State
    float gain;
    float StartGain;
    bool NoteActive;
    bool NoteReleased;
    
    // Sample-Accurate Timing (Replaces millis() variables)
    uint32_t sampleCount; 
    uint32_t startSample;
    uint32_t stopSample;
    
    // Envelope Settings
    float attack_val = 10.0f;   // Default 10ms
    float release_val = 500.0f; // Default 500ms
};

#endif
