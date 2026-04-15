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

  private:
    Sine sine;
    float gain;
};

#endif