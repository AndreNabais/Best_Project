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
  startSample = sampleCount;
  StartGain=velocity / 127.0;
  NoteActive = true;
  NoteReleased = false;
}

void MyDsp::noteOff(){
  stopSample = sampleCount;
  NoteActive = false;
  NoteReleased = true;
}


// create envelope
void MyDsp::Envelope(float attack_val,float release_val){
  if (NoteActive) {
    //ATTACK
    float attack_samples = (attack_val/1000.0f)*SR;
    float samples_elapsed = sampleCount - startSample;
    setGain((StartGain)*min(samples_elapsed/attack_samples,1));
  }
  
    //RELEASE
    else {
      float release_samples = (release_val/1000.0f)*SR;
      float samples_elapsed = sampleCount - stopSample;
      setGain((StartGain)*max((1-samples_elapsed/release_samples),0));
  }
}

void MyDsp::update(void) {
  audio_block_t* outBlock[AUDIO_OUTPUTS];
  for (int channel = 0; channel < AUDIO_OUTPUTS; channel++) {
    outBlock[channel] = allocate();
    if (outBlock[channel]) {
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        Envelope(attack_val, release_val);
        float currentSample = (sine.tick() * 2 - 1) * gain;
        currentSample = max(-1.0f, min(1.0f, currentSample));
        outBlock[channel]->data[i] = (int16_t)(currentSample * MULT_16);
        sampleCount++;
      }
      transmit(outBlock[channel], channel);
      release(outBlock[channel]);
    }
  }
}
