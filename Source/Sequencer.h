#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include "Params.h"

namespace nova
{

/** One stored sequence (a RECALL/STORE slot). Lives on the message thread. */
struct SeqData
{
    std::array<int, 16>  notes;
    std::array<bool, 16> on;
    int   len = 16, dir = 0, div = 2, scale = 2, transpose = 0;
    float gate = 0.55f, swing = 0.5f;
    bool  legato = false;
    juce::String name { "Init" };

    SeqData() { notes.fill (60); on.fill (true); }

    juce::ValueTree toTree() const
    {
        juce::ValueTree t ("Seq");
        juce::StringArray ns; juce::String os;
        for (int i = 0; i < 16; ++i) { ns.add (juce::String (notes[(size_t) i])); os << (on[(size_t) i] ? "1" : "0"); }
        t.setProperty ("name", name, nullptr);
        t.setProperty ("notes", ns.joinIntoString (","), nullptr);
        t.setProperty ("on", os, nullptr);
        t.setProperty ("len", len, nullptr);         t.setProperty ("dir", dir, nullptr);
        t.setProperty ("div", div, nullptr);         t.setProperty ("scale", scale, nullptr);
        t.setProperty ("transpose", transpose, nullptr);
        t.setProperty ("gate", gate, nullptr);       t.setProperty ("swing", swing, nullptr);
        t.setProperty ("legato", legato, nullptr);
        return t;
    }

    static SeqData fromTree (const juce::ValueTree& t)
    {
        SeqData s;
        if (! t.isValid()) return s;
        s.name = t.getProperty ("name", "Init").toString().substring (0, 16);
        auto ns = juce::StringArray::fromTokens (t.getProperty ("notes").toString(), ",", "");
        if (ns.size() == 16)
            for (int i = 0; i < 16; ++i) s.notes[(size_t) i] = juce::jlimit (36, 84, ns[i].getIntValue());
        auto os = t.getProperty ("on").toString();
        if (os.length() == 16)
            for (int i = 0; i < 16; ++i) s.on[(size_t) i] = os[i] == '1';
        s.len       = juce::jlimit (1, 16, (int) t.getProperty ("len", 16));
        s.dir       = juce::jlimit (0, 3, (int) t.getProperty ("dir", 0));
        s.div       = juce::jlimit (0, 3, (int) t.getProperty ("div", 2));
        s.scale     = juce::jlimit (0, 7, (int) t.getProperty ("scale", 2));
        s.transpose = juce::jlimit (-24, 24, (int) t.getProperty ("transpose", 0));
        s.gate      = juce::jlimit (0.05f, 1.0f, (float) t.getProperty ("gate", 0.55f));
        s.swing     = juce::jlimit (0.5f, 0.75f, (float) t.getProperty ("swing", 0.5f));
        s.legato    = (bool) t.getProperty ("legato", false);
        return s;
    }
};

inline SeqData makeSeq (const char* name, const char* on, std::initializer_list<int> notes,
                        float gate, float swing = 0.5f, int len = 16, int dir = 0, int div = 2, int scale = 2, bool legato = false)
{
    SeqData s;
    s.name = name;
    for (int i = 0; i < 16; ++i) s.on[(size_t) i] = on[i] == '1';
    int i = 0;
    for (auto n : notes) s.notes[(size_t) i++] = n;
    s.gate = gate; s.swing = swing; s.len = len; s.dir = dir; s.div = div; s.scale = scale; s.legato = legato;
    return s;
}

inline std::array<SeqData, 16> defaultSlots()
{
    std::array<SeqData, 16> a;
    a[0] = makeSeq ("Hypno",       "1011010110101101", { 60,60,67,72,60,63,60,70,60,60,75,60,67,60,72,63 }, 0.5f, 0.54f);
    a[1] = makeSeq ("Rolling",     "1001001001001010", { 60,60,60,67,60,60,63,60,60,72,60,60,58,60,70,60 }, 0.45f);
    a[2] = makeSeq ("Telemetrie",  "1111111111111111", { 72,79,75,84,72,70,79,77,72,82,75,79,84,72,77,70 }, 0.3f, 0.5f, 16, 3);
    a[3] = makeSeq ("Offbeat",     "0010001000100110", { 72,72,72,72,72,72,79,72,72,72,72,72,72,75,79,72 }, 0.4f, 0.58f);
    a[4] = makeSeq ("Polymeter 7", "1111111000000000", { 60,63,65,67,70,72,75,60,60,60,60,60,60,60,60,60 }, 0.5f, 0.5f, 7, 2);
    a[5] = makeSeq ("Phryg Slide", "1101110111011101", { 48,48,60,49,48,55,48,60,51,48,48,61,48,55,60,48 }, 0.7f, 0.5f, 16, 0, 2, 7, true);
    a[6] = makeSeq ("Sparse",      "1000000100100000", { 60,60,60,60,60,60,60,67,60,60,63,60,60,60,60,60 }, 0.6f);
    a[7] = makeSeq ("Achtel",      "1011010100000000", { 60,60,72,67,60,70,60,63,60,60,60,60,60,60,60,60 }, 0.4f, 0.5f, 8, 0, 1);
    return a;
}

/** The sequence the audio thread plays. Every field is atomic so the UI can edit it while running. */
struct SeqLive
{
    std::array<std::atomic<int>, 16>  notes;
    std::array<std::atomic<bool>, 16> on;
    std::atomic<int>   len { 16 }, dir { 0 }, div { 2 }, scale { 2 }, transpose { 0 };
    std::atomic<float> gate { 0.55f }, swing { 0.5f };
    std::atomic<bool>  legato { false };

    SeqLive() { load (SeqData()); }

    void load (const SeqData& d)
    {
        for (size_t i = 0; i < 16; ++i) { notes[i].store (d.notes[i]); on[i].store (d.on[i]); }
        len.store (d.len); dir.store (d.dir); div.store (d.div); scale.store (d.scale); transpose.store (d.transpose);
        gate.store (d.gate); swing.store (d.swing); legato.store (d.legato);
    }

    SeqData snapshot (const juce::String& name) const
    {
        SeqData d;
        for (size_t i = 0; i < 16; ++i) { d.notes[i] = notes[i].load(); d.on[i] = on[i].load(); }
        d.len = len.load(); d.dir = dir.load(); d.div = div.load(); d.scale = scale.load(); d.transpose = transpose.load();
        d.gate = gate.load(); d.swing = swing.load(); d.legato = legato.load(); d.name = name;
        return d;
    }
};

} // namespace nova
