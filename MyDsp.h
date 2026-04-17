#ifndef faust_teensy_h_
#define faust_teensy_h_

#include "Arduino.h"
#include "AudioStream.h"
#include "Audio.h"
#include "Phasor.h"
#include "SineTable.h"
#include "Noise.h"

class MyDsp : public AudioStream
{
  public:
    MyDsp();
    ~MyDsp();
    
    virtual void update(void);
    
    void setFreq(float freq) { baseFrequency = freq; }
    void setDist(float d) { distortionThreshold = 1.0f - (d * 0.9f); }
    void setWaveform(int type) { if (type >= 0 && type <= 3) waveformType = type; }
    void setAttack(float a) { attack_val = a; }
    void setRelease(float r) { release_val = r; }
    void setVibrato(float v) { vibratoAmount = v; }
    void setPitchBend(float b) { pitchBendMod = b; }
    void setMidiNote(int n) { currentMidiNote = n; }
    int getMidiNote() { return currentMidiNote; }
    
    void Envelope(float a, float r);
    void noteOn(float velocity);
    void noteOff();

    // Drum Triggers
    void triggerKick()    { kickActive = true;    kickSampleCount = 0; }
    void triggerSnare()   { snareActive = true;   snareSampleCount = 0; }
    void triggerHihat()   { hihatActive = true;   hihatSampleCount = 0; }
    void triggerTom()     { tomActive = true;     tomSampleCount = 0; }
    void triggerCowbell() { cowbellActive = true; cowbellSampleCount = 0; }

    bool NoteActive;
    bool NoteReleased;

  private:
    Phasor phasor;
    SineTable sineTable;
    
    Phasor kickOsc;
    Phasor tomOsc;
    Phasor bellOsc1;
    Phasor bellOsc2;
    Noise snareNoise;
    Noise hihatNoise;

    int waveformType;
    int currentMidiNote;
    float gain;
    float StartGain;
    float distortionThreshold;
    uint32_t sampleCount;
    uint32_t startSample;
    uint32_t stopSample;
    float attack_val;
    float release_val;
    float vibratoAmount;
    float baseFrequency;
    float pitchBendMod;
    float lfoPhase;

    bool kickActive, snareActive, hihatActive, tomActive, cowbellActive;
    uint32_t kickSampleCount, snareSampleCount, hihatSampleCount, tomSampleCount, cowbellSampleCount;

    float compute_kick();
    float compute_snare();
    float compute_hihat();
    float compute_tom();
    float compute_cowbell();
};

#endif
