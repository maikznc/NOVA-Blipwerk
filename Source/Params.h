#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace nova
{

//==============================================================================
// Lookup tables shared by engine, sequencer and UI. Values match the browser prototype.
inline const juce::StringArray& waveNames()   { static const juce::StringArray a { "SIN", "TRI", "SAW", "SQR", "PLS" }; return a; }
inline const juce::StringArray& ratioNames()  { static const juce::StringArray a { "0.5:1", "1:1", "1.41:1", "2:1", "2.5:1", "3:1", "3.5:1", "4:1", "5:1", "6:1", "7:1", "8:1" }; return a; }
inline const juce::StringArray& filterNames() { static const juce::StringArray a { "LP", "BP", "HP" }; return a; }
inline const juce::StringArray& modeNames()   { static const juce::StringArray a { "MIX", "RING" }; return a; }
inline const juce::StringArray& lfoRateNames(){ static const juce::StringArray a { "1/4", "1/2", "1 BAR", "2 BAR", "4 BAR", "8 BAR" }; return a; }
inline const juce::StringArray& lfoDestNames(){ static const juce::StringArray a { "CUT", "FM", "DEC" }; return a; }
inline const juce::StringArray& delayNames()  { static const juce::StringArray a { "1/16", "1/8T", "1/8", "3/16", "1/4", "3/8" }; return a; }

inline constexpr float  kRatios[12]     { 0.5f, 1.0f, 1.41f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
inline constexpr float  kWaveLevel[5]   { 0.9f, 0.85f, 0.5f, 0.45f, 0.45f };
inline constexpr double kDelayBeats[6]  { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5 };
inline constexpr double kLfoBeats[6]    { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
inline constexpr int    kDivs[4]        { 4, 8, 16, 32 };
inline constexpr const char* kDivNames[4] { "1/4", "1/8", "1/16", "1/32" };
inline constexpr const char* kDirSilk[4]  { "FWD", "REV", "ALT", "RND" };
inline constexpr const char* kDirNames[4] { "vorwärts", "rückwärts", "Pendel", "Zufall" };

struct ScaleDef { const char* silk; const char* name; int count; int degrees[12]; };
inline constexpr ScaleDef kScales[8]
{
    { "CHR", "Chromatisch", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    { "MAJ", "Dur",          7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "MIN", "Moll",         7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "DOR", "Dorisch",      7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "MIX", "Mixolydisch",  7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "HMN", "Harm. Moll",   7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "BLU", "Blues",        6, { 0, 3, 5, 6, 7, 10 } },
    { "PHR", "Phrygisch",    7, { 0, 1, 3, 5, 7, 8, 10 } },
};

inline int quantize (int note, int scale)
{
    const auto& s = kScales[juce::jlimit (0, 7, scale)];
    const int pc = ((note % 12) + 12) % 12;
    const int octave = (note - pc) / 12;
    int best = s.degrees[0];
    for (int i = 0; i < s.count; ++i)
        if (s.degrees[i] <= pc)
            best = s.degrees[i];
    return octave * 12 + best;
}

inline juce::String noteName (int midi)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int pc = ((midi % 12) + 12) % 12;
    return juce::String (names[pc]) + juce::String ((midi - pc) / 12 - 1);
}

//==============================================================================
// Value formatting (also used by the host to display parameter values)
inline juce::String fmtPct (float v)  { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
inline juce::String fmtMs (float v)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms"; }
inline juce::String fmtHz (float v)
{
    if (v >= 10000.0f) return juce::String (juce::roundToInt (v / 1000.0f)) + " kHz";
    if (v >= 1000.0f)  return juce::String (v / 1000.0f, 1) + " kHz";
    return juce::String (juce::roundToInt (v)) + " Hz";
}
inline juce::String fmtSigned (int v, const char* unit = "") { return (v > 0 ? "+" : "") + juce::String (v) + unit; }

inline juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float n) { return s * std::pow (e / s, n); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto integer = [&] (const char* id, const char* name, int lo, int hi, int def, std::function<String (int, int)> f)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def,
                                                         AudioParameterIntAttributes().withStringFromValueFunction (std::move (f))));
    };
    auto linear = [&] (const char* id, const char* name, float lo, float hi, float def, std::function<String (float, int)> f)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f))));
    };
    auto logarithmic = [&] (const char* id, const char* name, float lo, float hi, float def, std::function<String (float, int)> f)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, logRange (lo, hi), def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f))));
    };
    auto pct = [] (float v, int) { return fmtPct (v); };
    auto ms  = [] (float v, int) { return fmtMs (v); };

    // OSC 1
    choice  ("wave",  "Osc1 Wave",  waveNames(), 0);
    integer ("oct",   "Osc1 Octave", -2, 2, 0, [] (int v, int) { return fmtSigned (v); });
    choice  ("ratio", "FM Ratio",   ratioNames(), 3);
    linear  ("fm",    "FM Amount",  0.0f, 1.0f, 0.0f, pct);
    // OSC 2
    choice  ("wave2", "Osc2 Wave",  waveNames(), 0);
    integer ("tune",  "Osc2 Tune",  -24, 24, 12, [] (int v, int) { return fmtSigned (v, " st"); });
    linear  ("det",   "Osc2 Detune", 0.0f, 50.0f, 0.0f, [] (float v, int) { return String (roundToInt (v)) + " ct"; });
    linear  ("lvl2",  "Osc2 Level", 0.0f, 1.0f, 0.0f, pct);
    choice  ("mode2", "Osc2 Mode",  modeNames(), 0);
    // BLIP
    integer     ("pAmt",  "Blip Amount",   -24, 48, 12, [] (int v, int) { return fmtSigned (v, " st"); });
    logarithmic ("pDec",  "Pitch Decay",   1.0f, 400.0f, 10.0f, ms);
    logarithmic ("fmDec", "FM Decay",      5.0f, 2000.0f, 50.0f, ms);
    linear      ("click", "Click",         0.0f, 1.0f, 0.0f, pct);
    // FILTER
    choice      ("ftype",  "Filter Type",   filterNames(), 0);
    logarithmic ("cutoff", "Cutoff",        40.0f, 16000.0f, 4000.0f, [] (float v, int) { return fmtHz (v); });
    linear      ("reso",   "Resonance",     0.0f, 1.0f, 0.2f, pct);
    linear      ("fEnv",   "Filter Env",    0.0f, 1.0f, 0.3f, pct);
    logarithmic ("fDec",   "Filter Decay",  5.0f, 2000.0f, 60.0f, ms);
    // AMP
    logarithmic ("aDec",  "Decay",  10.0f, 2500.0f, 100.0f, ms);
    linear      ("drive", "Drive",  0.0f, 1.0f, 0.0f, pct);
    linear      ("bits",  "Crush",  0.0f, 1.0f, 0.0f, pct);
    // MOTION
    choice ("lRate",  "LFO Rate",  lfoRateNames(), 3);
    linear ("lDepth", "LFO Depth", 0.0f, 1.0f, 0.0f, pct);
    choice ("lDest",  "LFO Dest",  lfoDestNames(), 0);
    linear ("rand",   "Random",    0.0f, 1.0f, 0.0f, pct);
    // ECHO / SPACE
    choice ("dTime", "Echo Time",     delayNames(), 3);
    linear ("dFb",   "Echo Feedback", 0.0f, 0.92f, 0.45f, pct);
    linear ("dMix",  "Echo Mix",      0.0f, 1.0f, 0.25f, pct);
    linear ("damp",  "Echo Damp",     0.0f, 1.0f, 0.5f, pct);
    linear ("verb",  "Space",         0.0f, 1.0f, 0.12f, pct);
    // MASTER
    linear ("level", "Volume",        0.0f, 1.0f, 0.75f, pct);

    return layout;
}

//==============================================================================
struct Preset
{
    const char* name;
    const char* description;
    std::vector<std::pair<const char*, float>> values; // params not listed fall back to their defaults
};

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list
    {
        { "Bubble", "Die Tonhöhe steigt im Anschlag, statt zu fallen. Blubbert.",
          { {"wave",0},{"oct",0},{"ratio",1},{"fm",.08f},{"lvl2",0},{"pAmt",-12},{"pDec",55},{"fmDec",40},{"click",0},{"ftype",0},{"cutoff",2200},{"reso",.2f},{"fEnv",.2f},{"fDec",60},{"aDec",140},{"drive",.1f},{"bits",0},{"lDepth",0},{"rand",0},{"dTime",2},{"dFb",.4f},{"dMix",.25f},{"damp",.5f},{"verb",.2f} } },
        { "Bubble Fifth", "Bubble mit Quinte darüber und leichter Zufallsbewegung pro Note.",
          { {"wave",0},{"oct",0},{"ratio",1},{"fm",.08f},{"wave2",0},{"tune",7},{"lvl2",.25f},{"pAmt",-12},{"pDec",55},{"fmDec",40},{"cutoff",2200},{"reso",.2f},{"fEnv",.2f},{"fDec",60},{"aDec",140},{"drive",.1f},{"rand",.2f},{"dTime",3},{"dFb",.5f},{"dMix",.28f},{"damp",.5f},{"verb",.2f} } },
        { "Drip", "Höher und knapper, zwei Oktaven von unten herauf. Tropfen.",
          { {"wave",0},{"oct",1},{"ratio",3},{"fm",.12f},{"fmDec",30},{"lvl2",0},{"pAmt",-24},{"pDec",30},{"cutoff",4000},{"reso",.25f},{"fEnv",.25f},{"fDec",40},{"aDec",70},{"drive",.05f},{"rand",.15f},{"dTime",3},{"dFb",.5f},{"dMix",.3f},{"damp",.45f},{"verb",.22f} } },
        { "Bloop", "Dreieck, steigt eine Quinte, das resonante Filter wandert über zwei Takte.",
          { {"wave",1},{"ratio",1},{"fm",.05f},{"lvl2",0},{"pAmt",-7},{"pDec",70},{"fmDec",40},{"cutoff",1500},{"reso",.45f},{"fEnv",.3f},{"fDec",80},{"aDec",160},{"drive",.15f},{"lRate",3},{"lDepth",.3f},{"lDest",0},{"rand",.25f},{"dTime",2},{"dFb",.4f},{"dMix",.25f},{"damp",.55f},{"verb",.15f} } },
        { "Nova Bleep", "Zwei Sinus im Oktavabstand, kurzer Pitch-Sprung, 3/16-Echo. Der Grundbleep.",
          { {"wave",0},{"ratio",3},{"fm",.15f},{"wave2",0},{"tune",12},{"lvl2",.35f},{"pAmt",12},{"pDec",8},{"fmDec",30},{"click",.1f},{"cutoff",6000},{"reso",.15f},{"fEnv",.2f},{"fDec",40},{"aDec",70},{"drive",.1f},{"lRate",3},{"rand",.15f},{"dTime",3},{"dFb",.45f},{"dMix",.28f},{"damp",.45f},{"verb",.12f} } },
        { "Hypno Blip", "Sinus plus Quinte, Oktavsprung im Anschlag, das Filter atmet über zwei Takte.",
          { {"wave",0},{"ratio",3},{"fm",.2f},{"wave2",1},{"tune",7},{"lvl2",.25f},{"pAmt",24},{"pDec",14},{"fmDec",50},{"cutoff",3500},{"reso",.3f},{"fEnv",.35f},{"fDec",70},{"aDec",95},{"drive",.15f},{"lRate",3},{"lDepth",.35f},{"lDest",0},{"rand",.2f},{"dTime",3},{"dFb",.55f},{"dMix",.32f},{"damp",.5f},{"verb",.15f} } },
        { "Resonant Ping", "Fast selbstschwingendes Filter, vom Click angestoßen. Jeder Ton klingt etwas anders.",
          { {"wave",1},{"lvl2",0},{"pAmt",0},{"click",.35f},{"cutoff",1400},{"reso",.86f},{"fEnv",.55f},{"fDec",35},{"aDec",90},{"drive",.2f},{"rand",.4f},{"dTime",2},{"dFb",.4f},{"dMix",.25f},{"damp",.5f},{"verb",.1f} } },
        { "Ring Bleep", "Ringmodulation zweier Sinus. Der klassische Computer-Bleep.",
          { {"wave",0},{"wave2",0},{"tune",19},{"lvl2",.7f},{"mode2",1},{"pAmt",12},{"pDec",6},{"aDec",60},{"cutoff",8000},{"reso",.1f},{"fEnv",.1f},{"rand",.1f},{"dTime",0},{"dFb",.5f},{"dMix",.3f},{"damp",.35f},{"verb",.12f} } },
        { "Glass Ping", "FM 3.5:1 durch den Bandpass, glasig und hell.",
          { {"wave",0},{"oct",1},{"ratio",6},{"fm",.42f},{"fmDec",160},{"wave2",0},{"tune",24},{"lvl2",.15f},{"pAmt",12},{"pDec",6},{"ftype",1},{"cutoff",2600},{"reso",.3f},{"fEnv",.2f},{"fDec",220},{"aDec",220},{"drive",.05f},{"dTime",2},{"dFb",.5f},{"dMix",.35f},{"damp",.3f},{"verb",.3f} } },
        { "Telemetrie", "Hoher reiner Sinus, extrem kurz, viel Echo.",
          { {"wave",0},{"oct",1},{"fm",.05f},{"wave2",0},{"tune",12},{"lvl2",.2f},{"pAmt",0},{"click",.05f},{"cutoff",12000},{"reso",.1f},{"fEnv",0},{"aDec",30},{"dTime",0},{"dFb",.6f},{"dMix",.4f},{"damp",.4f},{"verb",.25f} } },
        { "Square Bleep", "Rechteck mit Sub-Oktave. Der Bleep aus den frühen Neunzigern.",
          { {"wave",3},{"wave2",3},{"tune",-12},{"lvl2",.5f},{"pAmt",0},{"cutoff",2200},{"reso",.2f},{"fEnv",.3f},{"fDec",50},{"aDec",120},{"drive",.2f},{"dTime",3},{"dFb",.4f},{"dMix",.2f},{"damp",.5f},{"verb",.1f} } },
        { "Pulse Chirp", "Schmaler Puls, leicht verstimmt, die Länge pumpt mit dem LFO.",
          { {"wave",4},{"oct",1},{"wave2",4},{"tune",12},{"det",12},{"lvl2",.3f},{"pAmt",7},{"pDec",20},{"cutoff",5000},{"reso",.35f},{"fEnv",.4f},{"fDec",30},{"aDec",50},{"drive",.15f},{"lRate",2},{"lDepth",.3f},{"lDest",2},{"rand",.25f},{"dTime",1},{"dFb",.45f},{"dMix",.3f},{"damp",.4f},{"verb",.15f} } },
        { "Dub Stab", "Sägezahn mit Quinte, dunkel, langes Ping-Pong. Das Filter wandert über vier Takte.",
          { {"wave",2},{"wave2",2},{"tune",7},{"det",8},{"lvl2",.8f},{"pAmt",0},{"cutoff",600},{"reso",.35f},{"fEnv",.55f},{"fDec",130},{"aDec",200},{"drive",.2f},{"lRate",4},{"lDepth",.5f},{"lDest",0},{"dTime",3},{"dFb",.7f},{"dMix",.45f},{"damp",.65f},{"verb",.35f} } },
        { "Rubber Drop", "Dreieck mit großem, langsamem Pitch-Fall. Gummiartig.",
          { {"wave",1},{"oct",-1},{"ratio",1},{"fm",.15f},{"wave2",0},{"tune",-12},{"lvl2",.4f},{"pAmt",36},{"pDec",40},{"fmDec",40},{"cutoff",1000},{"reso",.5f},{"fEnv",.45f},{"fDec",80},{"aDec",150},{"drive",.3f},{"dTime",2},{"dFb",.3f},{"dMix",.15f},{"damp",.6f},{"verb",.05f} } },
        { "Metal Tick", "Inharmonische FM, Ringmod und Click. Kurz und perkussiv.",
          { {"wave",3},{"oct",1},{"ratio",2},{"fm",.7f},{"fmDec",25},{"wave2",3},{"tune",11},{"lvl2",.5f},{"mode2",1},{"pAmt",12},{"pDec",4},{"click",.35f},{"ftype",2},{"cutoff",2000},{"reso",.4f},{"fEnv",.15f},{"fDec",30},{"aDec",40},{"drive",.25f},{"bits",.15f},{"rand",.3f},{"dTime",0},{"dFb",.35f},{"dMix",.2f},{"damp",.2f},{"verb",.1f} } },
        { "Acid Blip", "Resonanter Tiefpass, angezerrt, der Cutoff wandert mit.",
          { {"wave",2},{"oct",-1},{"wave2",3},{"tune",-12},{"lvl2",.3f},{"pAmt",0},{"cutoff",380},{"reso",.8f},{"fEnv",.7f},{"fDec",100},{"aDec",160},{"drive",.5f},{"lRate",3},{"lDepth",.4f},{"lDest",0},{"rand",.25f},{"dTime",0},{"dFb",.25f},{"dMix",.12f},{"damp",.5f},{"verb",.05f} } },
        { "Breathing Bleep", "Die Notenlänge atmet über zwei Takte von knapp bis lang.",
          { {"wave",1},{"ratio",3},{"fm",.2f},{"fmDec",40},{"wave2",0},{"tune",12},{"lvl2",.3f},{"pAmt",12},{"pDec",10},{"cutoff",4000},{"reso",.25f},{"fEnv",.3f},{"fDec",50},{"aDec",90},{"drive",.1f},{"lRate",3},{"lDepth",.8f},{"lDest",2},{"dTime",3},{"dFb",.5f},{"dMix",.3f},{"damp",.5f},{"verb",.18f} } },
        { "Crushed Blip", "Rechteck mit Bitcrush und Hochpass. Lo-Fi.",
          { {"wave",3},{"fm",.1f},{"lvl2",0},{"pAmt",12},{"pDec",10},{"fmDec",30},{"ftype",2},{"cutoff",300},{"reso",.25f},{"fEnv",.1f},{"fDec",40},{"aDec",70},{"drive",.4f},{"bits",.65f},{"dTime",3},{"dFb",.4f},{"dMix",.22f},{"damp",.3f},{"verb",.08f} } },
        { "Deep Sonar", "Tiefer Sinus mit langer Fahne und viel Raum.",
          { {"wave",0},{"oct",-1},{"ratio",3},{"fm",.1f},{"fmDec",200},{"wave2",0},{"tune",12},{"lvl2",.2f},{"pAmt",7},{"pDec",25},{"cutoff",1200},{"reso",.2f},{"fEnv",.2f},{"fDec",200},{"aDec",500},{"drive",.1f},{"dTime",5},{"dFb",.6f},{"dMix",.35f},{"damp",.7f},{"verb",.45f} } },
    };
    return list;
}

} // namespace nova
