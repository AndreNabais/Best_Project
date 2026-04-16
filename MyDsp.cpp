#include "MyDsp.h"

#define AUDIO_OUTPUTS 1
#define MULT_16 32767

MyDsp::MyDsp() :
AudioStream(AUDIO_OUTPUTS, new audio_block_t*[AUDIO_OUTPUTS]),
phasor(AUDIO_SAMPLE_RATE_EXACT),
kickOsc(AUDIO_SAMPLE_RATE_EXACT),
tomOsc(AUDIO_SAMPLE_RATE_EXACT),
snareNoise(),
hihatNoise(),
bellOsc1(AUDIO_SAMPLE_RATE_EXACT),
bellOsc2(AUDIO_SAMPLE_RATE_EXACT)
{
  gain = 0.0;
  distortionThreshold = 1.0;
  waveformType = 0; // Default: Sine
  sampleCount = 0;  // Initialize the master counter
}

MyDsp::~MyDsp(){}

//DRUM MACHINE

float MyDsp::compute_kick() {
    if (!kickActive) return 0.0f;

    // Pitch Envelope: Starts at 150Hz, drops to 40Hz very fast
    float pitchEnv = expf(-(float)kickSampleCount / 1000.0f); 
    float currentFreq = (110.0f * pitchEnv) + 40.0f;
    kickOsc.setFrequency(currentFreq);

    // Amplitude Envelope: Fades out over ~200ms
    float ampEnv = expf(-(float)kickSampleCount / 5000.0f);
    
    float out = kickOsc.tick() * ampEnv;
    kickSampleCount++;

    if (ampEnv < 0.001f) kickActive = false; // Stop when silent
    return out;
}

float MyDsp::compute_tom() {
    if (!tomActive) return 0.0f;

    // Pitch: Drops from 300Hz to 80Hz
    float pitchEnv = expf(-(float)tomSampleCount / 2000.0f); 
    tomOsc.setFrequency((220.0f * pitchEnv) + 80.0f);

    // Amplitude: Slightly longer decay than the kick
    float ampEnv = expf(-(float)tomSampleCount / 8000.0f);
    
    float out = tomOsc.tick() * ampEnv;
    tomSampleCount++;

    if (ampEnv < 0.001f) tomActive = false;
    return out;

}

float MyDsp::compute_cowbell() {
    if (!cowbellActive) return 0.0f;

    // Frequencies for a classic 808 bell
    bellOsc1.setFrequency(540.0f);
    bellOsc2.setFrequency(800.0f);

    // Short, sharp decay
    float ampEnv = expf(-(float)cowbellSampleCount / 3000.0f);
    
    // Mix the two squares and multiply by the envelope
    float out = (bellOsc1.tick() + bellOsc2.tick()) * 0.5f * ampEnv;
    
    cowbellSampleCount++;
    if (ampEnv < 0.001f) cowbellActive = false;
    return out;
}

float MyDsp::compute_snare() {
    if (!snareActive) return 0.0f;

    // Part 1: The "Thump" (Sine sweep)
    float pitchEnv = expf(-(float)snareSampleCount / 500.0f);
    kickOsc.setFrequency((180.0f * pitchEnv) + 100.0f); // Use kickOsc temporarily
    float thump = kickOsc.tick() * expf(-(float)snareSampleCount / 2000.0f);

    // Part 2: The "Sizzle" (Noise)
    float noise = snareNoise.tick();
    float sizzle = noise * expf(-(float)snareSampleCount / 4000.0f);

    // Mix them (mostly sizzle)
    float out = (thump * 0.3f) + (sizzle * 0.7f);

    snareSampleCount++;
    if (snareSampleCount > 10000) snareActive = false; // Stop after ~200ms
    return out;
}

float MyDsp::compute_hihat() {
    if (!hihatActive) return 0.0f;

    // Generate White Noise
    float noise = hihatNoise.tick();

    // Very short, "ticking" envelope
    float ampEnv = expf(-(float)hihatSampleCount / 1000.0f);
    
    float out = noise * ampEnv;

    hihatSampleCount++;
    if (ampEnv < 0.001f) hihatActive = false;
    return out;
}






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
  audio_block_t* outBlock = allocate();
  if (!outBlock) return;

  for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
    // 1. Reset the mix for this specific sample
    float mix = 0.0f;

    // 2. Sum all active drum voices
    // Each 'compute' function handles its own oscillators, envelopes, and internal counters
    mix += compute_kick() * 0.5f;
    mix += compute_snare() * 0.4f;
    mix += compute_hihat() * 0.3f;
    mix += compute_tom() * 0.4f;
    mix += compute_cowbell() * 0.2f;

    // 3. Clipping Protection / Hard Limiting
    // This replaces your 'distortionThreshold' logic to keep the mix safe
    if (mix > 1.0f) mix = 1.0f;
    if (mix < -1.0f) mix = -1.0f;

    // 4. Convert to 16-bit integer for the Audio Shield
    outBlock->data[i] = (int16_t)(mix * 32767);

    // 5. Increment the MASTER sample counter
    // This allows all individual compute functions to stay in sync
    sampleCount++; 
  }

  // 6. Transmit to both Left (0) and Right (1) channels
  transmit(outBlock, 0);
  transmit(outBlock, 1); 
  release(outBlock);
}

