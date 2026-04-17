#include "MyDsp.h"

#define MULT_16 32767

MyDsp::MyDsp() : AudioStream(1, new audio_block_t*[1]),
                 phasor(44117.65f), 
                 kickOsc(44117.65f), 
                 tomOsc(44117.65f),
                 bellOsc1(44117.65f), 
                 bellOsc2(44117.65f),
                 snareNoise(),
                 hihatNoise(),
                 sineTable(4096)
{
    gain = 0.0f;
    distortionThreshold = 1.0f;
    waveformType = 0;
    sampleCount = 0;
    NoteActive = false;
    NoteReleased = false;
    currentMidiNote = -1;
    attack_val = 10.0f;
    release_val = 500.0f;
    vibratoAmount = 0.0f;
    baseFrequency = 0.0f;
    pitchBendMod = 1.0f;
    lfoPhase = 0.0f;
    startSample = 0;
    stopSample = 0;

    kickActive = snareActive = hihatActive = tomActive = cowbellActive = false;
    kickSampleCount = snareSampleCount = hihatSampleCount = tomSampleCount = cowbellSampleCount = 0;
}

MyDsp::~MyDsp() {}

void MyDsp::noteOn(float velocity) {
    startSample = sampleCount;
    StartGain = velocity / 127.0f;
    NoteActive = true;
    NoteReleased = false;
}

void MyDsp::noteOff() {
    stopSample = sampleCount;
    NoteActive = false;
    NoteReleased = true;
}

void MyDsp::Envelope(float attack_val, float release_val) {
    float SR = 44117.65f;
    if (NoteActive) {
        float attack_samples = (attack_val / 1000.0f) * SR;
        float samples_elapsed = (float)(sampleCount - startSample);
        gain = StartGain * fminf(samples_elapsed / attack_samples, 1.0f);
    }
    else if (NoteReleased) {
        float release_samples = (release_val / 1000.0f) * SR;
        float samples_elapsed = (float)(sampleCount - stopSample);
        float releaseGain = StartGain * (1.0f - (samples_elapsed / release_samples));
        gain = fmaxf(releaseGain, 0.0f);
        if (gain <= 0.0f) NoteReleased = false;
    }
}

float MyDsp::compute_kick() {
    if (!kickActive) return 0.0f;
    float pitchEnv = expf(-(float)kickSampleCount / 1000.0f); 
    kickOsc.setFrequency((110.0f * pitchEnv) + 40.0f);
    float ampEnv = expf(-(float)kickSampleCount / 5000.0f);
    float out = kickOsc.tick() * ampEnv;
    kickSampleCount++;
    if (ampEnv < 0.001f) kickActive = false; 
    return out;
}

float MyDsp::compute_tom() {
    if (!tomActive) return 0.0f;
    float pitchEnv = expf(-(float)tomSampleCount / 2000.0f); 
    tomOsc.setFrequency((220.0f * pitchEnv) + 80.0f);
    float ampEnv = expf(-(float)tomSampleCount / 8000.0f);
    float out = tomOsc.tick() * ampEnv;
    tomSampleCount++;
    if (ampEnv < 0.001f) tomActive = false;
    return out;
}

float MyDsp::compute_cowbell() {
    if (!cowbellActive) return 0.0f;
    bellOsc1.setFrequency(540.0f);
    bellOsc2.setFrequency(800.0f);
    float ampEnv = expf(-(float)cowbellSampleCount / 3000.0f);
    float out = (bellOsc1.tick() + bellOsc2.tick()) * 0.5f * ampEnv;
    cowbellSampleCount++;
    if (ampEnv < 0.001f) cowbellActive = false;
    return out;
}

float MyDsp::compute_snare() {
    if (!snareActive) return 0.0f;
    float pitchEnv = expf(-(float)snareSampleCount / 500.0f);
    kickOsc.setFrequency((180.0f * pitchEnv) + 100.0f); 
    float thump = kickOsc.tick() * expf(-(float)snareSampleCount / 2000.0f);
    float sizzle = snareNoise.tick() * expf(-(float)snareSampleCount / 4000.0f);
    float out = (thump * 0.3f) + (sizzle * 0.7f);
    snareSampleCount++;
    if (snareSampleCount > 10000) snareActive = false; 
    return out;
}

float MyDsp::compute_hihat() {
    if (!hihatActive) return 0.0f;
    float ampEnv = expf(-(float)hihatSampleCount / 1000.0f);
    float out = hihatNoise.tick() * ampEnv;
    hihatSampleCount++;
    if (ampEnv < 0.001f) hihatActive = false;
    return out;
}

void MyDsp::update(void) {
    audio_block_t* outBlock = allocate();
    if (!outBlock) return; 

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        sampleCount++;
        Envelope(attack_val, release_val);

        lfoPhase += 5.5f / 44117.65f; 
        if (lfoPhase > 1.0f) lfoPhase -= 1.0f;
        float vibratoMod = (vibratoAmount > 0.0f) ? 1.0f + (sineTable.tick((int)(lfoPhase * 2047.0f)) * 0.015f) : 1.0f;

        phasor.setFrequency(baseFrequency * pitchBendMod * vibratoMod);
        float p = phasor.tick();
        float sig = 0.0f;
        switch (waveformType) {
            case 0: sig = sineTable.tick((int)(p * 2047.0f)); break;
            case 1: sig = 2.0f * fabsf(2.0f * p - 1.0f) - 1.0f; break;
            case 2: sig = (p < 0.5f) ? 1.0f : -1.0f; break;
            case 3: sig = 2.0f * p - 1.0f; break;
        }
        sig *= gain;

        sig += compute_kick() * 0.5f;
        sig += compute_snare() * 0.4f;
        sig += compute_hihat() * 0.3f;
        sig += compute_tom() * 0.4f;
        sig += compute_cowbell() * 0.2f;

        if (sig > distortionThreshold) sig = distortionThreshold;
        if (sig < -distortionThreshold) sig = -distortionThreshold;
        float makeup = 1.0f / distortionThreshold;

        outBlock->data[i] = (int16_t)(constrain(sig * makeup, -1.0f, 1.0f) * MULT_16);
    }
    transmit(outBlock);
    release(outBlock);
}
