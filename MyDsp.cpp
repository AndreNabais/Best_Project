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
    float pitchEnv = expf(-(float)kickSampleCount / 2500.0f); 
    kickOsc.setFrequency((75.0f * pitchEnv) + 45.0f);
    float ampEnv = expf(-(float)kickSampleCount / 5000.0f);
    float out = kickOsc.tick() * ampEnv;
    kickSampleCount++;
    if (ampEnv < 0.001f) kickActive = false; 
    return out;
}

float MyDsp::compute_tom() {
    if (!tomActive) return 0.0f;

    // 1. Very slow pitch decay (12000.0f)
    // This keeps the pitch stable so it sounds like a musical note, not a slide.
    float pitchEnv = expf(-(float)tomSampleCount / 12000.0f); 
    
    // 2. Narrow Frequency Range
    // Start at 130Hz and drop only to 90Hz. 
    // This is the "sweet spot" for a mid-to-low tom.
    float currentFreq = (40.0f * pitchEnv) + 90.0f;
    tomOsc.setFrequency(currentFreq);

    // 3. Stick Impact (The "Tom" secret sauce)
    // We add a tiny burst of noise at the very start (first 10ms) 
    // to simulate the drumstick hitting the skin.
    float stickHit = hihatNoise.tick() * expf(-(float)tomSampleCount / 400.0f) * 0.15f;

    // 4. Amplitude Envelope
    float ampEnv = expf(-(float)tomSampleCount / 10000.0f);
    
    // Mix the oscillator with the stick hit
    float out = (tomOsc.tick() + stickHit) * ampEnv;
    
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
        
        // 1. Envelope & Vibrato calculations
        Envelope(attack_val, release_val);
        lfoPhase += 5.5f / 44117.65f; 
        if (lfoPhase > 1.0f) lfoPhase -= 1.0f;
        
        float vibratoMod = (vibratoAmount > 0.0f) ? 
                           1.0f + (sineTable.tick((int)(lfoPhase * 2047.0f)) * 0.015f) : 1.0f;

        // 2. Generate Oscillator (The Synth Part)
        phasor.setFrequency(baseFrequency * pitchBendMod * vibratoMod);
        float p = phasor.tick();
        float synthSig = 0.0f;
        
        switch (waveformType) {
            case 0: synthSig = sineTable.tick((int)(p * 2047.0f)); break; 
            case 1: synthSig = 2.0f * fabsf(2.0f * p - 1.0f) - 1.0f; break; 
            case 2: synthSig = (p < 0.5f) ? 1.0f : -1.0f; break;          
            case 3: synthSig = 2.0f * p - 1.0f; break;                    
        }

        // --- APPLY ENVELOPE GAIN ONLY TO SYNTH ---
        float finalMix = synthSig * gain;

        // 3. Add Drums (These now bypass the synth envelope)
        finalMix += compute_kick() * 0.5f;
        finalMix += compute_snare() * 0.4f;
        finalMix += compute_hihat() * 0.3f;
        finalMix += compute_tom() * 0.4f;
        finalMix += compute_cowbell() * 0.2f;

        // 4. Dedicated Metronome Beeps
        if (metroHighActive || metroLowActive) {
            float freq = metroHighActive ? 1000.0f : 500.0f;
            float beep = sinf(2.0f * PI * freq * (float)metroSampleCount / 44117.65f);
            float env = expf(-(float)metroSampleCount / 1000.0f);
            
            finalMix += beep * env * 0.4f;
            metroSampleCount++;
            
            if (env < 0.01f) { 
                metroHighActive = false; 
                metroLowActive = false; 
            }
        }

        // 5. Distortion Stage
        if (finalMix > distortionThreshold) finalMix = distortionThreshold;
        if (finalMix < -distortionThreshold) finalMix = -distortionThreshold;
        
        // 6. Makeup Gain
        finalMix *= (1.0f / distortionThreshold);

        // 7. Master Safety Limiter
        if (finalMix > 1.0f) finalMix = 1.0f;
        if (finalMix < -1.0f) finalMix = -1.0f;

        // 8. Output
        outBlock->data[i] = (int16_t)(finalMix * MULT_16);
    }
    
    transmit(outBlock);
    release(outBlock);
}
