#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>
#include <vector>

namespace nova
{

/** Snapshot of all sound parameters. Voices capture it at note start, like the prototype. */
struct SynthParams
{
    int   wave = 0, oct = 0, ratio = 3;
    float fm = 0.0f;
    int   wave2 = 0, tune = 12;
    float det = 0.0f, lvl2 = 0.0f;
    int   mode2 = 0;
    int   pAmt = 12;
    float pDec = 10.0f, fmDec = 50.0f, click = 0.0f;
    int   ftype = 0;
    float cutoff = 4000.0f, reso = 0.2f, fEnv = 0.3f, fDec = 60.0f;
    float aDec = 100.0f, drive = 0.0f, bits = 0.0f;
    int   lRate = 3;
    float lDepth = 0.0f;
    int   lDest = 0;
    float rand = 0.0f;
    int   dTime = 3;
    float dFb = 0.45f, dMix = 0.25f, damp = 0.5f, verb = 0.12f, level = 0.75f;
};

//==============================================================================
class Voice
{
public:
    void start (const SynthParams& p, int midiNote, double sampleRate, double gateSeconds, bool heldByMidi,
                float glideFromHz, float lfoValue, juce::Random& rng, uint32_t ageStamp);
    void release();
    void kill() { active = false; }
    void render (float* out, int numSamples);

    bool     active = false;
    bool     held   = false;
    int      note   = -1;
    uint32_t age    = 0;

private:
    float osc (int wave, double phase, double dt) const;
    float noise();

    double sr = 48000.0;
    int    n = 0;
    bool   released = false;

    // oscillators
    int    w1 = 0, w2 = 0;
    bool   hasOsc2 = false, ring = false, hasFm = false;
    double ph1 = 0, ph2 = 0, phM = 0;
    float  base1 = 440, base2 = 880, baseM = 880, envR = 1.0f, pe = 1.0f, pCoef = 0.0f;
    float  dev0 = 0, devEnd = 0, fe = 1.0f, fmCoef = 0.0f;
    float  g1 = 1.0f, g2 = 0.0f, mixGain = 1.0f, clickAmp = 0.0f, clickCoef = 0.0f;
    uint32_t seed = 22222;

    // filter (TPT state variable)
    int   ftype = 0, fCounter = 0;
    float cut = 1000, peakCut = 1000, fe2 = 1.0f, fCoef = 0.0f, k = 1.0f;
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0;

    // amp
    float amp = 0.0f, peak = 0.45f, dCoef = 0.0f, rCoef = 0.0f, rTauSec = 0.01f;
    int   attackSamples = 1, gateSamples = 0, maxSamples = 0;
};

//==============================================================================
class Biquad
{
public:
    void setLowpass  (double fc, double q, double sr) { set (fc, q, sr, false); }
    void setHighpass (double fc, double q, double sr) { set (fc, q, sr, true); }
    void reset() { z1 = z2 = 0.0f; }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    void set (double fc, double q, double sr, bool highpass);
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

//==============================================================================
/** Drive, crush, ping-pong echo, space (reverb), master gain and a gentle limiter. */
class FxBus
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const SynthParams& p, double bpm);
    void process (const float* mono, float* outL, float* outR, int numSamples);

private:
    float readDelay (const std::vector<float>& buf, float delaySamples) const;

    double sr = 48000.0;
    juce::SmoothedValue<float> driveK, driveComp, delaySamples, feedback, sendEcho, sendSpace, master;
    float bits = 0.0f, dampTarget = 8000.0f, dampCurrent = -1.0f;

    std::vector<float> dl, dr;
    int writePos = 0;
    Biquad lpL, lpR, hpL, hpR, spaceHpL, spaceHpR;
    juce::Reverb reverb;
    std::vector<float> revL, revR;

    float compEnv = 0.0f, compAtk = 0.0f, compRel = 0.0f;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** glide = slide from the previous note's pitch (sequencer legato). */
    void noteOn (const SynthParams& p, int midiNote, double gateSeconds, bool heldByMidi, bool glide, float lfoValue);
    void noteOff (int midiNote);
    void allNotesOff();
    void renderVoices (float* mono, int numSamples);

    FxBus fx;

private:
    std::array<Voice, 16> voices;
    double sr = 48000.0;
    float lastHz = 0.0f;
    uint32_t counter = 0;
    juce::Random rng;
};

} // namespace nova
