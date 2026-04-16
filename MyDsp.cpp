#include "MyDsp.h"

#define AUDIO_OUTPUTS 1
#define MULT_16 32767

MyDsp::MyDsp() :
AudioStream(AUDIO_OUTPUTS, new audio_block_t*[AUDIO_OUTPUTS]),
//sawtooth(AUDIO_SAMPLE_RATE_EXACT),
sine(AUDIO_SAMPLE_RATE_EXACT)
{
  gain = 0.0;
}

MyDsp::~MyDsp(){}

void MyDsp::setFreq(float freq){
  sine.setFrequency(freq);
}

void MyDsp::setGain(float g){
  gain = g;
}

void MyDsp::noteOn(float velocity){
  startTime = millis();
  StartGain=velocity / 127.0;
  NoteActive = true;
  NoteReleased = false;
}

void MyDsp::noteOff(){
  stopTime = millis();
  NoteActive = false;
  NoteReleased = true;
}


// create envelope
void MyDsp::Envelope(float attack_val,float release_val){
  if (NoteActive) {
    //ATTACK
    float duration_begin = millis() - startTime;
    setGain((StartGain)*min(duration_begin/attack_val,1));
  }
  
    else {
    float duration_end = millis() - stopTime;
    setGain((StartGain)*(1-duration_end/release_val));
  }
}

void MyDsp::update(void) {
  Envelope(attack_val, release_val);
  audio_block_t* outBlock[AUDIO_OUTPUTS];
  for (int channel = 0; channel < AUDIO_OUTPUTS; channel++) {
    outBlock[channel] = allocate();
    if (outBlock[channel]) {
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float currentSample = (sine.tick() * 2 - 1) * gain;
        currentSample = max(-1.0f, min(1.0f, currentSample));
        outBlock[channel]->data[i] = (int16_t)(currentSample * MULT_16);
      }
      transmit(outBlock[channel], channel);
      release(outBlock[channel]);
    }
  }
}
