#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace nova;

namespace
{
    constexpr const char* kParamIds[31] {
        "wave", "oct", "ratio", "fm", "wave2", "tune", "det", "lvl2", "mode2",
        "pAmt", "pDec", "fmDec", "click", "ftype", "cutoff", "reso", "fEnv", "fDec",
        "aDec", "drive", "bits", "lRate", "lDepth", "lDest", "rand",
        "dTime", "dFb", "dMix", "damp", "verb", "level"
    };
}

NovaBlipwerkProcessor::NovaBlipwerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (size_t i = 0; i < raw.size(); ++i)
    {
        raw[i] = apvts.getRawParameterValue (kParamIds[i]);
        jassert (raw[i] != nullptr);
        apvts.addParameterListener (kParamIds[i], this);
    }

    slots = defaultSlots();
    seq.load (slots[0]);
    seqName = slots[0].name;
    loadPreset (0);
}

NovaBlipwerkProcessor::~NovaBlipwerkProcessor()
{
    for (auto* id : kParamIds)
        apvts.removeParameterListener (id, this);
}

//==============================================================================
SynthParams NovaBlipwerkProcessor::readParams() const
{
    auto f = [this] (int i) { return raw[(size_t) i]->load(); };
    auto n = [this] (int i) { return juce::roundToInt (raw[(size_t) i]->load()); };
    SynthParams p;
    p.wave = n (0);   p.oct = n (1);    p.ratio = n (2);   p.fm = f (3);
    p.wave2 = n (4);  p.tune = n (5);   p.det = f (6);     p.lvl2 = f (7);   p.mode2 = n (8);
    p.pAmt = n (9);   p.pDec = f (10);  p.fmDec = f (11);  p.click = f (12);
    p.ftype = n (13); p.cutoff = f (14); p.reso = f (15);  p.fEnv = f (16);  p.fDec = f (17);
    p.aDec = f (18);  p.drive = f (19); p.bits = f (20);
    p.lRate = n (21); p.lDepth = f (22); p.lDest = n (23); p.rand = f (24);
    p.dTime = n (25); p.dFb = f (26);   p.dMix = f (27);   p.damp = f (28);  p.verb = f (29);
    p.level = f (30);
    return p;
}

void NovaBlipwerkProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (! loadingPreset.load() && parameterID != "level")
        edited.store (true);
}

//==============================================================================
void NovaBlipwerkProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, juce::jmax (samplesPerBlock, 4096));
    mono.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
    scratchR.assign (mono.size(), 0.0f);
    haveLastStep = false; expectedPpq = -1.0;
}

bool NovaBlipwerkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void NovaBlipwerkProcessor::pressPlay()
{
    if (running.load()) { seqOn.store (false); jam.store (false); return; }
    seqOn.store (true);
    if (! hostPlaying.load()) jam.store (true);
}

void NovaBlipwerkProcessor::pressStop()
{
    seqOn.store (false);
    jam.store (false);
    stopRequest.store (true);
    curStep.store (-1);
}

//==============================================================================
void NovaBlipwerkProcessor::triggerStep (long long k, int sample, double stepLen, double bpm, double ppq)
{
    const int L = juce::jlimit (1, 16, seq.len.load());
    int idx = 0;
    switch (seq.dir.load())
    {
        case 0:  idx = (int) (((k % L) + L) % L); break;
        case 1:  idx = L - 1 - (int) (((k % L) + L) % L); break;
        case 2:
            if (L == 1) { idx = 0; break; }
            {
                const long long P = 2 * L - 2;
                const int m = (int) (((k % P) + P) % P);
                idx = m < L ? m : (int) P - m;
            }
            break;
        default: idx = rng.nextInt (L); break;
    }
    curStep.store (idx);

    if (seq.on[(size_t) idx].load())
    {
        const int note = juce::jlimit (12, 120, quantize (seq.notes[(size_t) idx].load(), seq.scale.load()) + seq.transpose.load());
        const bool legato = seq.legato.load();
        const double stepSec = stepLen * 60.0 / bpm;
        Event e;
        e.sample = sample; e.type = evNoteOn; e.note = note;
        e.gate = legato ? stepSec * 1.02 : stepSec * seq.gate.load();
        e.held = false; e.glide = legato && prevOn; e.beat = ppq;
        addEvent (e);
        prevOn = true;
    }
    else
    {
        prevOn = false;
    }
}

void NovaBlipwerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0) return;
    if ((int) mono.size() < n) mono.resize ((size_t) n);
    std::fill (mono.begin(), mono.begin() + n, 0.0f);

    const SynthParams P = readParams();

    // ---- transport ---------------------------------------------------------------
    bool hp = false; double hBpm = 0.0, hPpq = 0.0; bool havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hp = pos->getIsPlaying();
            if (auto b = pos->getBpm()) hBpm = *b;
            if (auto q = pos->getPpqPosition()) { hPpq = *q; havePpq = true; }
        }
    hostBpm.store (hBpm);
    hostPlaying.store (hp);
    const double bpm = hBpm > 1.0 ? hBpm : (double) internalBpm.load();
    engine.fx.setParams (P, bpm);

    if (stopRequest.exchange (false))
    {
        internalPpq = 0.0; haveLastStep = false; prevOn = false; expectedPpq = -1.0;
        curStep.store (-1);
    }
    if (hp && ! wasHostPlaying) { jam.store (false); haveLastStep = false; }
    wasHostPlaying = hp;

    const bool run = seqOn.load() && (hp || jam.load());
    running.store (run);

    const double ppqPerSample = bpm / 60.0 / sr;
    const bool followHost = hp && havePpq;
    const double ppqStart = followHost ? hPpq : internalPpq;
    auto beatAt = [&] (int s) { return (followHost ? hPpq : freeBeat) + s * ppqPerSample; };

    numEvents = 0;

    // ---- MIDI input plays the synth directly --------------------------------------
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int s = juce::jlimit (0, n - 1, meta.samplePosition);
        Event e; e.sample = s;
        if (m.isNoteOn())                                { e.type = evNoteOn; e.note = m.getNoteNumber(); e.held = true; e.beat = beatAt (s); addEvent (e); }
        else if (m.isNoteOff())                          { e.type = evNoteOff; e.note = m.getNoteNumber(); addEvent (e); }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { e.type = evAllOff; addEvent (e); }
    }
    midi.clear();

    // ---- audition from the editor (encoder turns, knob releases) -------------------
    const int a = auditionNote.exchange (-1);
    if (a >= 0 && ! run)
    {
        Event e; e.sample = 0; e.type = evNoteOn; e.note = a; e.gate = 0.16; e.beat = beatAt (0);
        addEvent (e);
    }

    // ---- step sequencer -------------------------------------------------------------
    if (run)
    {
        const double ppqEnd = ppqStart + n * ppqPerSample;
        if (expectedPpq >= 0.0 && std::abs (ppqStart - expectedPpq) > 0.05)
            haveLastStep = false; // loop or jump in the host

        const int div = kDivs[juce::jlimit (0, 3, seq.div.load())];
        const double stepLen = 4.0 / div;
        const double swingOff = (seq.swing.load() - 0.5) * 2.0 * stepLen;
        const long long kFirst = (long long) std::floor ((ppqStart - swingOff) / stepLen) - 1;
        const long long kLast  = (long long) std::floor (ppqEnd / stepLen);

        for (long long k = kFirst; k <= kLast; ++k)
        {
            const double t = (double) k * stepLen + ((k & 1) ? swingOff : 0.0);
            if (t < ppqStart || t >= ppqEnd) continue;
            if (haveLastStep && k <= lastStepK) continue;
            lastStepK = k; haveLastStep = true;
            triggerStep (k, juce::jlimit (0, n - 1, (int) ((t - ppqStart) / ppqPerSample)), stepLen, bpm, t);
        }
        expectedPpq = ppqEnd;
        if (! followHost) internalPpq = ppqEnd;
    }
    else
    {
        expectedPpq = -1.0;
    }
    freeBeat += n * ppqPerSample;

    // ---- render between events (sample accurate) -----------------------------------
    std::stable_sort (events.begin(), events.begin() + numEvents, [] (const Event& x, const Event& y) { return x.sample < y.sample; });

    int cursor = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        const auto& e = events[(size_t) i];
        if (e.sample > cursor) { engine.renderVoices (mono.data() + cursor, e.sample - cursor); cursor = e.sample; }
        if (e.type == evNoteOn)
        {
            const float lfo = (float) std::sin (juce::MathConstants<double>::twoPi * e.beat / kLfoBeats[juce::jlimit (0, 5, P.lRate)]);
            engine.noteOn (P, e.note, e.gate, e.held, e.glide, lfo);
        }
        else if (e.type == evNoteOff) engine.noteOff (e.note);
        else                          engine.allNotesOff();
    }
    if (cursor < n) engine.renderVoices (mono.data() + cursor, n - cursor);

    // ---- effects + output ---------------------------------------------------------
    const int chans = buffer.getNumChannels();
    float* L = buffer.getWritePointer (0);
    float* R = chans > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R != nullptr)
    {
        engine.fx.process (mono.data(), L, R, n);
    }
    else
    {
        // mono output: render stereo, then fold down
        if ((int) scratchR.size() < n) scratchR.resize ((size_t) n);
        engine.fx.process (mono.data(), L, scratchR.data(), n);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + scratchR[(size_t) i]);
    }

    int w = scopeWrite.load();
    for (int i = 0; i < n; ++i) { scope[(size_t) w] = L[i]; w = (w + 1) & (scopeSize - 1); }
    scopeWrite.store (w);
}

//==============================================================================
int NovaBlipwerkProcessor::getNumPrograms() { return (int) presets().size(); }
int NovaBlipwerkProcessor::getCurrentProgram() { return presetIndex.load(); }
void NovaBlipwerkProcessor::setCurrentProgram (int index) { if (juce::isPositiveAndBelow (index, getNumPrograms())) loadPreset (index); }
const juce::String NovaBlipwerkProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String::fromUTF8 (presets()[(size_t) index].name) : juce::String();
}

void NovaBlipwerkProcessor::loadPreset (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    const auto& pr = presets()[(size_t) index];
    loadingPreset.store (true);
    for (auto* param : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (rp == nullptr || rp->getParameterID() == "level") continue;
        float v = rp->convertFrom0to1 (rp->getDefaultValue());
        for (const auto& kv : pr.values)
            if (rp->getParameterID() == kv.first) v = kv.second;
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    }
    loadingPreset.store (false);
    presetIndex.store (index);
    edited.store (false);
}

//==============================================================================
void NovaBlipwerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("NovaBlipwerk");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("preset", presetIndex.load(), nullptr);
    root.setProperty ("edited", edited.load(), nullptr);
    root.setProperty ("bpm", internalBpm.load(), nullptr);
    root.setProperty ("seqOn", seqOn.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    {
        const juce::ScopedLock sl (uiLock);
        root.setProperty ("slot", slot, nullptr);
        root.appendChild (seq.snapshot (seqName).toTree(), nullptr);
        juce::ValueTree st ("Slots");
        for (const auto& s : slots) st.appendChild (s.toTree(), nullptr);
        root.appendChild (st, nullptr);
    }
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaBlipwerkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("NovaBlipwerk")) return;

    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
    {
        loadingPreset.store (true);
        apvts.replaceState (params);
        loadingPreset.store (false);
    }
    presetIndex.store (juce::jlimit (0, getNumPrograms() - 1, (int) root.getProperty ("preset", 0)));
    edited.store ((bool) root.getProperty ("edited", false));
    internalBpm.store (juce::jlimit (60.0f, 200.0f, (float) root.getProperty ("bpm", 140.0f)));
    seqOn.store ((bool) root.getProperty ("seqOn", true));

    const juce::ScopedLock sl (uiLock);
    slot = juce::jlimit (0, 15, (int) root.getProperty ("slot", 0));
    auto st = root.getChildWithName ("Slots");
    if (st.isValid() && st.getNumChildren() == 16)
        for (int i = 0; i < 16; ++i) slots[(size_t) i] = SeqData::fromTree (st.getChild (i));
    auto cur = root.getChildWithName ("Seq");
    if (cur.isValid())
    {
        auto d = SeqData::fromTree (cur);
        seq.load (d);
        seqName = d.name;
    }
}

//==============================================================================
juce::AudioProcessorEditor* NovaBlipwerkProcessor::createEditor() { return new NovaBlipwerkEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaBlipwerkProcessor(); }
