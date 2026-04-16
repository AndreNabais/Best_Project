#ifndef faust_teensy_h_
#define faust_teensy_h_

#include "Arduino.h"
#include "AudioStream.h"
#include "Audio.h"
#include "Phasor.h"
#include "Sine.h"

class MyDsp : public AudioStream
{
  public:
    MyDsp();
    ~MyDsp();
    virtual void update(void);
    void setFreq(float freq);
    void setGain(float g);
    void Envelope(float attack_val,float release_val);
    void noteOn(float velocity);
    void noteOff();

  private:
    Sine sine;
    float gain;
    float StartGain;
    bool NoteActive;
    bool NoteReleased;
    float stopTime;
    float startTime;
    float attack_val = 10.0f;  // Default 10ms
    float release_val = 50.0f; // Default 50ms
    float velocity;
    float sampleCount=0.0f;
    float startSample=0.0f;
    float stopSample=0.0f;
    float SR = 48000.0f;

};

#endif
#endif
