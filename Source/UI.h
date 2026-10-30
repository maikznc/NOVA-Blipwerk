#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace nova::ui
{

//==============================================================================
namespace pal
{
    inline const juce::Colour bg        { 0xff0f1114 };
    inline const juce::Colour plateTop  { 0xff3a3f47 };
    inline const juce::Colour plate     { 0xff2b2f36 };
    inline const juce::Colour plateEdge { 0xff17191d };
    inline const juce::Colour well      { 0xff24282e };
    inline const juce::Colour wellLine  { 0x10ffffff };
    inline const juce::Colour ink       { 0xffe9edf2 };
    inline const juce::Colour ink2      { 0xff9aa4b0 };
    inline const juce::Colour ink3      { 0xff6c7682 };
    inline const juce::Colour knob      { 0xff14171b };
    inline const juce::Colour knobHi    { 0xff414955 };
    inline const juce::Colour tick      { 0xfff3f6f9 };
    inline const juce::Colour track     { 0x17ffffff };
    inline const juce::Colour led       { 0xff3d8bff };
    inline const juce::Colour ledHi     { 0xff7ab1ff };
    inline const juce::Colour cyan      { 0xff4fd8ff };
    inline const juce::Colour ledLine   { 0x6b6ea5ff };
    inline const juce::Colour ledWash   { 0x183d8bff };
    inline const juce::Colour slab      { 0xff141619 };
    inline const juce::Colour slabHi    { 0xff1d2025 };
    inline const juce::Colour slabLine  { 0xff07080a };
    inline const juce::Colour slabInk   { 0xffd0d7df };
    inline const juce::Colour slabInk2  { 0xff7f8a96 };
    inline const juce::Colour slabTrack { 0xff2f353e };
    inline const juce::Colour slabKnob  { 0xff0d0f12 };
    inline const juce::Colour slabKnobHi{ 0xff363d47 };
    inline const juce::Colour pad       { 0xff252a32 };
    inline const juce::Colour padHi     { 0xff313842 };
    inline const juce::Colour oled      { 0xff04070b };
    inline const juce::Colour oledInk   { 0xffe4f1ff };
    inline const juce::Colour oledAcc   { 0xff7ab1ff };
    inline const juce::Colour oledDim   { 0xff55687f };
}

struct Fonts
{
    static juce::Font display (float height, float kerning = 0.0f);
    static juce::Font label   (float height, float kerning = 0.0f);
    static juce::Font mono    (float height, float kerning = 0.0f);
};

void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, float norm, bool slabStyle);

//==============================================================================
class NovaLnF : public juce::LookAndFeel_V4
{
public:
    NovaLnF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider&) override;
    juce::Font getPopupMenuFont() override { return Fonts::mono (14.0f); }
};

//==============================================================================
/** A synth knob attached to a host parameter: label above, value below. */
class ParamKnob : public juce::Component
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& labelText, int dragPixels);
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (const juce::String& label, const juce::String& value, float norm)> onShow;
    std::function<void()> onRelease;

    juce::Slider slider;

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::String titleText, bool highlightPanel) : title (std::move (titleText)), highlight (highlightPanel) {}
    void add (std::unique_ptr<ParamKnob> k) { addAndMakeVisible (*k); knobs.push_back (std::move (k)); }
    void paint (juce::Graphics&) override;
    void resized() override;
    std::vector<std::unique_ptr<ParamKnob>> knobs;

private:
    juce::String title;
    bool highlight = false;
};

//==============================================================================
struct KnobSpec
{
    juce::String label;
    double min = 0, max = 1, step = 0, value = 0, def = 0, sens = 200;
    std::function<void (double)> set;
    std::function<juce::String (double)> fmt;
    bool hl = false, dim = false;
};

/** Sequencer encoder: its meaning is supplied live (it changes with SHIFT, like the hardware). */
class HwKnob : public juce::Component
{
public:
    explicit HwKnob (bool isBig = false) : big (isBig) {}
    std::function<KnobSpec()> spec;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void apply (const KnobSpec& s, double v);
    bool big = false;
    double dragN = 0;
    float lastY = 0;
};

class HwButton : public juce::Component
{
public:
    HwButton (juce::String text, bool isWide = false) : label (std::move (text)), wide (isWide) { setRepaintsOnMouseActivity (false); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setLit (bool shouldLight) { if (lit != shouldLight) { lit = shouldLight; repaint(); } }

    std::function<void()> onPress, onRelease, onClick;
    bool dice = false;

private:
    juce::String label;
    bool wide = false, lit = false, down = false;
};

class PadComp : public juce::Component
{
public:
    PadComp (int padIndex, juce::String silkText) : index (padIndex), silk (std::move (silkText)) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onPress) onPress (index); }
    void setState (bool on, bool opt, bool out, bool cur, bool silkActive);

    std::function<void (int)> onPress;

private:
    int index;
    juce::String silk;
    bool isOn = false, isOpt = false, isOut = false, isCur = false, silkAct = false;
};

/** Two-line display above the encoders. */
class SeqLcd : public juce::Component
{
public:
    void setText (juce::AttributedString line1, juce::AttributedString line2);
    void paint (juce::Graphics&) override;

private:
    juce::AttributedString l1, l2;
    juce::String lastKey;
};

} // namespace nova::ui
