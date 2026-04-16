#include "MyDsp.h"

#define AUDIO_OUTPUTS 1
#define MULT_16 32767

MyDsp::MyDsp() :
AudioStream(AUDIO_OUTPUTS, new audio_block_t*[AUDIO_OUTPUTS]),
phasor(AUDIO_SAMPLE_RATE_EXACT)
{
  gain = 0.0;
  distortionThreshold = 1.0;
  waveformType = 0; // Default: Sine
  sampleCount = 0;  // Initialize the master counter
}

MyDsp::~MyDsp(){}

void MyDsp::setFreq(float freq){
  phasor.setFrequency(freq);
}

void MyDsp::setGain(float g){
  gain = g;
}

void MyDsp::setDist(float d){
  // Map 0.0-1.0 to threshold 1.0-0.1
  distortionThreshold = 1.0f - (d * 0.9f);
}

void MyDsp::setWaveform(int type) {
  if (type >= 0 && type <= 3) {
    waveformType = type;
  }
}

void MyDsp::noteOn(float velocity){
  startSample = sampleCount;   // Mark the sample where note started
  StartGain = velocity / 127.0;
  NoteActive = true;
  NoteReleased = false;
}

void MyDsp::noteOff(){
  stopSample = sampleCount;    // Mark the sample where note ended
  NoteActive = false;
  NoteReleased = true;
}

void MyDsp::Envelope(float attack_val, float release_val){
  float SR = AUDIO_SAMPLE_RATE_EXACT;

  if (NoteActive) {
    // ATTACK logic based on sample difference
    float attack_samples = (attack_val / 1000.0f) * SR;
    float samples_elapsed = (float)(sampleCount - startSample);
    setGain(StartGain * fminf(samples_elapsed / attack_samples, 1.0f));
  }
  else if (NoteReleased) {
    // RELEASE logic based on sample difference
    float release_samples = (release_val / 1000.0f) * SR;
    float samples_elapsed = (float)(sampleCount - stopSample);
    float releaseGain = StartGain * (1.0f - (samples_elapsed / release_samples));
    setGain(fmaxf(releaseGain, 0.0f));
  }
}

void MyDsp::setAttack(float a) { 
  attack_val = a; 
  }
void MyDsp::setRelease(float r) { 
  release_val = r; 
}

void MyDsp::update(void) {
  audio_block_t* outBlock[AUDIO_OUTPUTS];
  
  for (int channel = 0; channel < AUDIO_OUTPUTS; channel++) {
    outBlock[channel] = allocate();
    if (outBlock[channel]) {
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        
        // 1. Update the envelope per-sample for maximum smoothness
        Envelope(attack_val, release_val);

        // 2. Generate the selected waveform from Phasor
        float p = phasor.tick();
        float rawSample = 0.0;

        switch (waveformType) {
          case 0: rawSample = sinf(p * 2.0f * PI); break;            // Sine
          case 1: rawSample = 2.0f * fabsf(2.0f * p - 1.0f) - 1.0f; break; // Tri
          case 2: rawSample = (p * 2.0f) - 1.0f; break;             // Saw
          case 3: rawSample = (p < 0.5f) ? 1.0f : -1.0f; break;     // Square
        }

        // 3. Apply Distortion (Clipping)
        float currentSample = rawSample;
        if (currentSample > distortionThreshold) currentSample = distortionThreshold;
        if (currentSample < -distortionThreshold) currentSample = -distortionThreshold;
        
        // 4. Normalize and Apply Gain (Envelope)
        currentSample = currentSample * (1.0f / distortionThreshold);
        currentSample *= gain;

        // 5. Output and increment the master sample counter
        outBlock[channel]->data[i] = (int16_t)(currentSample * MULT_16);
        sampleCount++; 
      }
      transmit(outBlock[channel], channel);
      release(outBlock[channel]);
    }
  }
}
