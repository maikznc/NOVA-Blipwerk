#include "SynthEngine.h"
#include "Params.h"
#include <climits>

namespace nova
{

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    inline float polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt;              return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt;   return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline void wrap (double& ph) { ph -= std::floor (ph); }

    inline float expCoef (double tauSeconds, double sr) { return (float) std::exp (-1.0 / (std::max (1.0e-5, tauSeconds) * sr)); }
}

//==============================================================================
void Voice::start (const SynthParams& p, int midiNote, double sampleRate, double gateSeconds, bool heldByMidi,
                   float glideFromHz, float lfoValue, juce::Random& rng, uint32_t ageStamp)
{
    sr = sampleRate; note = midiNote; held = heldByMidi; age = ageStamp;
    active = true; released = false; n = 0;

    // MOTION: tempo LFO sampled at the note, plus per-note randomness
    float cutoff = p.cutoff, aDec = p.aDec, fm = p.fm;
    const float m = p.lDepth * lfoValue;
    if (p.lDest == 0)      cutoff *= std::pow (2.0f, m * 3.0f);
    else if (p.lDest == 1) fm = juce::jlimit (0.0f, 1.0f, fm + m * 0.5f);
    else                   aDec *= std::pow (2.0f, m * 2.0f);

    if (p.rand > 0.0f)
    {
        auto R = [&] { return (rng.nextFloat() * 2.0f - 1.0f) * p.rand; };
        cutoff *= std::pow (2.0f, R() * 2.5f);
        aDec   *= std::pow (2.0f, R());
        fm      = juce::jlimit (0.0f, 1.0f, fm + R() * 0.3f);
    }
    aDec = juce::jlimit (5.0f, 4000.0f, aDec);

    // pitch + BLIP envelope
    const bool glide = glideFromHz > 0.0f;
    base1 = (float) (440.0 * std::pow (2.0, (midiNote - 69) / 12.0 + p.oct));
    const float startHz = juce::jlimit (8.0f, 20000.0f, glide ? glideFromHz : base1 * std::pow (2.0f, (float) p.pAmt / 12.0f));
    envR  = startHz / base1;
    pe    = 1.0f;
    pCoef = expCoef (glide ? 0.018 : std::max (0.0004, p.pDec / 3000.0), sr);

    w1 = juce::jlimit (0, 4, p.wave); ph1 = 0.0;
    g1 = kWaveLevel[w1]; g2 = 0.0f; mixGain = 1.0f;
    hasOsc2 = p.lvl2 > 0.001f; ring = false;
    if (hasOsc2)
    {
        w2 = juce::jlimit (0, 4, p.wave2); ph2 = 0.0;
        base2 = base1 * std::pow (2.0f, ((float) p.tune + p.det / 100.0f) / 12.0f);
        if (p.mode2 == 0) { g2 = p.lvl2 * kWaveLevel[w2]; mixGain = 1.0f / (1.0f + p.lvl2 * 0.6f); }
        else              { ring = true; g2 = p.lvl2 * 0.95f; g1 = kWaveLevel[w1] * (1.0f - p.lvl2); }
    }

    hasFm = fm > 0.001f;
    if (hasFm)
    {
        const float r = kRatios[juce::jlimit (0, 11, p.ratio)];
        baseM  = base1 * r; phM = 0.0;
        dev0   = std::min (fm * fm * 9.0f * base1 * r, 14000.0f);
        devEnd = dev0 * 0.08f; fe = 1.0f;
        fmCoef = expCoef (std::max (0.001, p.fmDec / 3000.0), sr);
    }

    clickAmp  = p.click > 0.001f ? p.click * 1.3f : 0.0f;
    clickCoef = expCoef (0.0045, sr);
    seed = (uint32_t) rng.nextInt() | 1u;

    // filter
    ftype = juce::jlimit (0, 2, p.ftype);
    const float track = std::pow (2.0f, (midiNote - 60) / 24.0f);
    cut     = juce::jlimit (30.0f, 18000.0f, cutoff * track);
    peakCut = juce::jlimit (30.0f, 18000.0f, cut * std::pow (2.0f, p.fEnv * 6.0f));
    fe2 = 1.0f; fCoef = expCoef (std::max (0.001, p.fDec / 3000.0), sr);
    const float q = ftype == 1 ? 0.7f + p.reso * 14.0f : std::pow (10.0f, (p.reso * p.reso * 24.0f) / 20.0f);
    k = 1.0f / q; ic1 = ic2 = 0.0f; fCounter = 0;

    // amp: linear attack, exponential decay, faster release at gate end
    const double att  = glide ? 0.006 : 0.0015;
    const double dTau = aDec / 3000.0;
    rTauSec = (float) juce::jlimit (0.004, 0.07, dTau / 2.0);
    attackSamples = std::max (1, (int) (att * sr));
    dCoef = expCoef (dTau, sr); rCoef = expCoef (rTauSec, sr);
    amp = 0.0f; peak = 0.45f;

    const double off = std::max (gateSeconds, att + 0.002);
    gateSamples = held ? INT_MAX : (int) (off * sr);
    const double endSec = held ? att + dTau * 7.0 + 0.03 : std::min (off + rTauSec * 7.0, att + dTau * 7.0) + 0.03;
    maxSamples = (int) (endSec * sr);
}

void Voice::release()
{
    if (released) return;
    released = true;
    maxSamples = std::min (maxSamples, std::max (n, attackSamples) + (int) ((rTauSec * 7.0 + 0.03) * sr));
}

float Voice::noise()
{
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return (float) seed * 4.6566129e-10f - 1.0f;
}

float Voice::osc (int wave, double ph, double dt) const
{
    const double adt = std::min (std::abs (dt), 0.5);
    switch (wave)
    {
        case 0: return (float) std::sin (twoPi * ph);
        case 1: return (float) (ph < 0.25 ? 4.0 * ph : (ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0));
        case 2: return (float) (2.0 * ph - 1.0) - polyBlep (ph, adt);
        case 3:
        {
            float s = ph < 0.5 ? 1.0f : -1.0f;
            s += polyBlep (ph, adt);
            s -= polyBlep (std::fmod (ph + 0.5, 1.0), adt);
            return s;
        }
        default:
        {
            constexpr double d = 0.18;
            float s = ph < d ? 1.0f : -1.0f;
            s += polyBlep (ph, adt);
            s -= polyBlep (std::fmod (ph + 1.0 - d, 1.0), adt);
            return s - (float) (2.0 * d - 1.0);
        }
    }
}

void Voice::render (float* out, int numSamples)
{
    if (! active) return;
    const float nyq = (float) (0.45 * sr);
    const double invSr = 1.0 / sr;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! released && n >= gateSamples)
            release();

        const float pr = 1.0f + (envR - 1.0f) * pe;
        pe *= pCoef;

        float f1 = base1 * pr;
        if (hasFm)
        {
            const float dev = devEnd + (dev0 - devEnd) * fe;
            fe *= fmCoef;
            f1 += (float) std::sin (twoPi * phM) * dev;
            phM += baseM * pr * invSr; wrap (phM);
        }

        const double dt1 = f1 * invSr;
        const float x1 = osc (w1, ph1, dt1);
        ph1 += dt1; wrap (ph1);

        float s = g1 * x1;
        if (hasOsc2)
        {
            const double dt2 = base2 * pr * invSr;
            const float x2 = osc (w2, ph2, dt2);
            ph2 += dt2; wrap (ph2);
            s += g2 * (ring ? x1 * x2 : x2);
        }
        if (clickAmp > 1.0e-5f)
        {
            s += clickAmp * noise();
            clickAmp *= clickCoef;
        }
        s *= mixGain;

        // filter
        if ((fCounter++ & 7) == 0)
        {
            const float fc = std::min (cut + (peakCut - cut) * fe2, nyq);
            const float g = (float) std::tan (juce::MathConstants<double>::pi * fc * invSr);
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        fe2 *= fCoef;
        const float v3 = s - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        const float y = ftype == 0 ? v2 : (ftype == 1 ? k * v1 : s - k * v1 - v2);

        // amp
        if (n < attackSamples) amp = peak * (float) (n + 1) / (float) attackSamples;
        else                   amp *= released ? rCoef : dCoef;

        out[i] += y * amp;
        ++n;

        if (n >= maxSamples || (n > attackSamples && amp < 1.0e-5f))
        {
            active = false;
            break;
        }
    }
}

//==============================================================================
void Biquad::set (double fc, double q, double sr, bool highpass)
{
    fc = juce::jlimit (10.0, sr * 0.45, fc);
    const double w0 = twoPi * fc / sr, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    const double nb0 = highpass ? (1.0 + cw) / 2.0 : (1.0 - cw) / 2.0;
    const double nb1 = highpass ? -(1.0 + cw) : (1.0 - cw);
    b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = b0;
    a1 = (float) (-2.0 * cw / a0); a2 = (float) ((1.0 - alpha) / a0);
}

//==============================================================================
void FxBus::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    const int size = (int) (sr * 3.0) + 8;
    dl.assign ((size_t) size, 0.0f);
    dr.assign ((size_t) size, 0.0f);
    revL.assign ((size_t) std::max (1, maxBlock), 0.0f);
    revR.assign ((size_t) std::max (1, maxBlock), 0.0f);

    for (auto* s : { &driveK, &driveComp, &feedback, &sendEcho, &sendSpace, &master })
        s->reset (sr, 0.05);
    delaySamples.reset (sr, 0.08);
    driveK.setCurrentAndTargetValue (1.0f); driveComp.setCurrentAndTargetValue (1.0f);
    delaySamples.setCurrentAndTargetValue ((float) (0.32 * sr));

    hpL.setHighpass (170.0, 0.707, sr); hpR.setHighpass (170.0, 0.707, sr);
    spaceHpL.setHighpass (320.0, 0.707, sr); spaceHpR.setHighpass (320.0, 0.707, sr);
    dampCurrent = -1.0f;

    reverb.setSampleRate (sr);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.82f; rp.damping = 0.45f; rp.wetLevel = 0.33f; rp.dryLevel = 0.0f; rp.width = 1.0f; rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    compAtk = expCoef (0.002, sr);
    compRel = expCoef (0.12, sr);
    reset();
}

void FxBus::reset()
{
    std::fill (dl.begin(), dl.end(), 0.0f);
    std::fill (dr.begin(), dr.end(), 0.0f);
    writePos = 0;
    for (auto* b : { &lpL, &lpR, &hpL, &hpR, &spaceHpL, &spaceHpR }) b->reset();
    reverb.reset();
    compEnv = 0.0f;
}

void FxBus::setParams (const SynthParams& p, double bpm)
{
    driveK.setTargetValue (1.0f + p.drive * 14.0f);
    driveComp.setTargetValue (1.0f / (1.0f + p.drive * 1.6f));
    bits = p.bits;
    const double secs = kDelayBeats[juce::jlimit (0, 5, p.dTime)] * 60.0 / juce::jmax (20.0, bpm);
    delaySamples.setTargetValue ((float) juce::jlimit (1.0, (double) dl.size() - 4.0, secs * sr));
    feedback.setTargetValue (p.dFb);
    sendEcho.setTargetValue (p.dMix * 0.9f);
    sendSpace.setTargetValue (p.verb * 0.9f);
    master.setTargetValue (p.level * p.level * 1.15f * 1.4f); // 1.4 = makeup similar to the browser's compressor
    dampTarget = 16000.0f * std::pow (700.0f / 16000.0f, p.damp);
}

float FxBus::readDelay (const std::vector<float>& buf, float d) const
{
    const int size = (int) buf.size();
    double rp = (double) writePos - (double) d;
    while (rp < 0.0) rp += size;
    const int i0 = (int) rp;
    const float frac = (float) (rp - i0);
    const int i1 = (i0 + 1) % size;
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void FxBus::process (const float* mono, float* outL, float* outR, int numSamples)
{
    if (dampCurrent < 0.0f || std::abs (dampTarget - dampCurrent) > dampCurrent * 0.005f)
    {
        dampCurrent = dampCurrent < 0.0f ? dampTarget : dampCurrent + (dampTarget - dampCurrent) * 0.35f;
        lpL.setLowpass (dampCurrent, 0.707, sr);
        lpR.setLowpass (dampCurrent, 0.707, sr);
    }

    const int size = (int) dl.size();
    const bool crush = bits >= 0.01f;
    const float levels = std::pow (2.0f, 10.0f - bits * 8.5f);
    if ((int) revL.size() < numSamples) { revL.resize ((size_t) numSamples); revR.resize ((size_t) numSamples); }

    for (int i = 0; i < numSamples; ++i)
    {
        // drive + crush (input clamped like a waveshaper)
        const float kd = driveK.getNextValue();
        float y = std::tanh (kd * juce::jlimit (-1.0f, 1.0f, mono[i])) / std::tanh (kd);
        y *= driveComp.getNextValue();
        if (crush) y = std::round (juce::jlimit (-1.0f, 1.0f, y) * levels) / levels;
        const float post = y;

        // ping-pong echo with damping in the loop
        const float d  = delaySamples.getNextValue();
        const float yL = hpL.process (lpL.process (readDelay (dl, d)));
        const float yR = hpR.process (lpR.process (readDelay (dr, d)));
        const float fb = feedback.getNextValue();
        dl[(size_t) writePos] = post * sendEcho.getNextValue() + yR * fb;
        dr[(size_t) writePos] = yL * fb;
        if (++writePos >= size) writePos = 0;

        // space send (dry + echoes)
        const float vs = sendSpace.getNextValue();
        revL[(size_t) i] = spaceHpL.process ((post + yL) * vs);
        revR[(size_t) i] = spaceHpR.process ((post + yR) * vs);

        outL[i] = post + yL;
        outR[i] = post + yR;
    }

    reverb.processStereo (revL.data(), revR.data(), numSamples);

    constexpr float thrDb = -8.0f, ratio = 8.0f, knee = 6.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = master.getNextValue();
        float l = (outL[i] + revL[(size_t) i]) * g;
        float r = (outR[i] + revR[(size_t) i]) * g;

        const float lvl = std::max (std::abs (l), std::abs (r));
        compEnv = lvl > compEnv ? compAtk * compEnv + (1.0f - compAtk) * lvl
                                : compRel * compEnv + (1.0f - compRel) * lvl;
        const float over = juce::Decibels::gainToDecibels (compEnv, -120.0f) - thrDb;
        float gainDb = 0.0f;
        if (over > knee * 0.5f)       gainDb = -over * (1.0f - 1.0f / ratio);
        else if (over > -knee * 0.5f) gainDb = -(1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        const float cg = juce::Decibels::decibelsToGain (gainDb);

        outL[i] = juce::jlimit (-0.99f, 0.99f, l * cg);
        outR[i] = juce::jlimit (-0.99f, 0.99f, r * cg);
    }
}

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    fx.prepare (sampleRate, maxBlock);
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v.kill();
    lastHz = 0.0f;
    fx.reset();
}

void Engine::noteOn (const SynthParams& p, int midiNote, double gateSeconds, bool heldByMidi, bool glide, float lfoValue)
{
    Voice* v = nullptr;
    for (auto& candidate : voices)
        if (! candidate.active) { v = &candidate; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& candidate : voices)
            if (candidate.age < v->age) v = &candidate;
    }
    const float glideFrom = (glide && lastHz > 0.0f) ? lastHz : 0.0f;
    v->start (p, midiNote, sr, gateSeconds, heldByMidi, glideFrom, lfoValue, rng, ++counter);
    lastHz = (float) (440.0 * std::pow (2.0, (midiNote - 69) / 12.0 + p.oct));
}

void Engine::noteOff (int midiNote)
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == midiNote)
            v.release();
}

void Engine::allNotesOff()
{
    for (auto& v : voices)
        if (v.active) v.release();
}

void Engine::renderVoices (float* mono, int numSamples)
{
    for (auto& v : voices)
        if (v.active)
            v.render (mono, numSamples);
}

} // namespace nova
