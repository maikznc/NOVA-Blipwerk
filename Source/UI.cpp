#include "UI.h"
#include "BinaryData.h"

namespace nova::ui
{

//==============================================================================
namespace
{
    juce::Typeface::Ptr loadFace (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    juce::Font makeFont (juce::Typeface::Ptr face, float h, float kerning)
    {
        auto opts = juce::FontOptions().withHeight (h).withKerningFactor (kerning);
        if (face != nullptr) opts = opts.withTypeface (face);
        return juce::Font (opts);
    }
}

juce::Font Fonts::display (float h, float k)
{
    static auto face = loadFace (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    return makeFont (face, h, k);
}

juce::Font Fonts::label (float h, float k)
{
    static auto face = loadFace (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
    return makeFont (face, h, k);
}

juce::Font Fonts::mono (float h, float k)
{
    static auto face = loadFace (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);
    return makeFont (face, h, k);
}

//==============================================================================
void drawKnob (juce::Graphics& g, juce::Rectangle<float> r, float norm, bool slabStyle)
{
    using namespace juce;
    const float size   = jmin (r.getWidth(), r.getHeight());
    const auto  c      = r.getCentre();
    const float stroke = jmax (2.6f, size * 0.07f);
    const float radius = size * 0.5f - stroke * 0.5f - 1.0f;
    const float a0 = -MathConstants<float>::pi * 0.75f, a1 = MathConstants<float>::pi * 0.75f;
    const PathStrokeType st (stroke, PathStrokeType::curved, PathStrokeType::rounded);

    Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
    g.setColour (slabStyle ? pal::slabTrack : pal::track);
    g.strokePath (track, st);

    norm = jlimit (0.0f, 1.0f, norm);
    if (norm > 0.002f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a0 + (a1 - a0) * norm, true);
        g.setColour (pal::led.withAlpha (0.2f));
        g.strokePath (arc, PathStrokeType (stroke * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        if (slabStyle) g.setColour (pal::led);
        else g.setGradientFill (ColourGradient (pal::led, c.x - radius, c.y + radius, pal::cyan, c.x + radius, c.y - radius, false));
        g.strokePath (arc, st);
    }

    const float capR = radius - size * 0.12f;
    const auto capB = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (capB.translated (0.0f, size * 0.045f).expanded (0.5f));
    const Colour hi = slabStyle ? pal::slabKnobHi : pal::knobHi;
    const Colour lo = slabStyle ? pal::slabKnob : pal::knob;
    g.setGradientFill (ColourGradient (hi, c.x - capR * 0.35f, c.y - capR * 0.5f, lo, c.x + capR * 0.55f, c.y + capR * 0.75f, true));
    g.fillEllipse (capB);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawEllipse (capB.reduced (0.5f), 1.0f);

    const float ang = a0 + (a1 - a0) * norm;
    auto pt = [&] (float rr) { return Point<float> (c.x + std::sin (ang) * rr, c.y - std::cos (ang) * rr); };
    g.setColour (slabStyle ? pal::slabInk : pal::tick);
    g.drawLine (Line<float> (pt (capR * 0.86f), pt (capR * 0.36f)), jmax (1.8f, size * 0.045f));
}

//==============================================================================
NovaLnF::NovaLnF()
{
    setColour (juce::PopupMenu::backgroundColourId, pal::oled);
    setColour (juce::PopupMenu::textColourId, pal::oledInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::led.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, pal::oledInk);
    setColour (juce::PopupMenu::headerTextColourId, pal::oledDim);
}

void NovaLnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider&)
{
    drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, false);
}

//==============================================================================
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& labelText, int dragPixels)
    : label (labelText)
{
    param = state.getParameter (paramId);
    jassert (param != nullptr);
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (dragPixels);
    slider.setVelocityBasedMode (false);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this]
    {
        repaint();
        if (slider.isMouseButtonDown() && onShow)
            onShow (param->getName (40), param->getCurrentValueAsText(), param->getValue());
    };
    slider.onDragEnd = [this] { if (onRelease) onRelease(); };
    addAndMakeVisible (slider);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    r.removeFromBottom (16);
    const int s = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (pal::ink2);
    g.setFont (Fonts::label (11.0f, 0.1f));
    g.drawText (label.toUpperCase(), r.removeFromTop (14.0f), juce::Justification::centred, false);
    g.setColour (pal::ink);
    g.setFont (Fonts::mono (10.5f));
    g.drawText (param->getCurrentValueAsText(), r.removeFromBottom (14.0f), juce::Justification::centred, false);
}

//==============================================================================
void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (pal::well);
    g.fillRoundedRectangle (r, 12.0f);
    if (highlight)
    {
        g.setGradientFill (juce::ColourGradient (pal::ledWash, 0.0f, r.getY(), pal::ledWash.withAlpha (0.0f), 0.0f, r.getBottom() * 0.8f, false));
        g.fillRoundedRectangle (r, 12.0f);
    }
    // recessed top shadow
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f), 0.0f, r.getY(), juce::Colours::transparentBlack, 0.0f, r.getY() + 8.0f, false));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (highlight ? pal::ledLine : pal::wellLine);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);

    const auto titleFont = Fonts::display (9.5f, 0.24f);
    g.setFont (titleFont);
    g.setColour (highlight ? pal::ledHi : pal::ink2);
    const float tw = juce::GlyphArrangement::getStringWidth (titleFont, title);
    g.drawText (title, juce::Rectangle<float> (14.0f, 9.0f, tw + 4.0f, 14.0f), juce::Justification::centredLeft, false);
    g.setColour (highlight ? pal::ledLine : pal::wellLine);
    g.fillRect (juce::Rectangle<float> (14.0f + tw + 12.0f, 15.5f, r.getWidth() - tw - 40.0f, 1.0f));
}

void ModulePanel::resized()
{
    auto area = getLocalBounds().reduced (8, 0);
    area.removeFromTop (28);
    area.removeFromBottom (8);
    const int n = (int) knobs.size();
    if (n == 0) return;
    const int kw = 62;
    const float gap = (float) (area.getWidth() - n * kw) / (float) n;
    for (int i = 0; i < n; ++i)
        knobs[(size_t) i]->setBounds ((int) (area.getX() + gap * 0.5f + i * (kw + gap)), area.getY(), kw, area.getHeight());
}

//==============================================================================
void HwKnob::paint (juce::Graphics& g)
{
    if (! spec) return;
    const auto s = spec();
    auto r = getLocalBounds().toFloat();
    if (s.dim) g.beginTransparencyLayer (0.32f);

    const float labelH = big ? 15.0f : 13.0f, valueH = big ? 17.0f : 14.0f;
    g.setFont (Fonts::label (big ? 12.0f : 10.5f, 0.12f));
    g.setColour (s.hl ? pal::oledAcc : pal::slabInk2);
    g.drawText (s.label, r.removeFromTop (labelH), juce::Justification::centred, false);
    g.setFont (Fonts::mono (big ? 13.0f : 10.5f));
    g.setColour (pal::slabInk);
    g.drawText (s.fmt ? s.fmt (s.value) : juce::String (s.value), r.removeFromBottom (valueH), juce::Justification::centred, false);

    r.reduce (0.0f, 3.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    const float norm = (float) ((s.value - s.min) / juce::jmax (1.0e-9, s.max - s.min));
    drawKnob (g, r.withSizeKeepingCentre (d, d), norm, true);

    if (s.dim) g.endTransparencyLayer();
}

void HwKnob::mouseDown (const juce::MouseEvent& e)
{
    if (! spec) return;
    const auto s = spec();
    dragN = (s.value - s.min) / juce::jmax (1.0e-9, s.max - s.min);
    lastY = e.position.y;
}

void HwKnob::apply (const KnobSpec& s, double v)
{
    v = juce::jlimit (s.min, s.max, v);
    if (s.step > 0) v = s.min + std::round ((v - s.min) / s.step) * s.step;
    if (std::abs (v - s.value) > 1.0e-9 && s.set) s.set (v);
    repaint();
}

void HwKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! spec) return;
    const float dy = lastY - e.position.y;
    lastY = e.position.y;
    if (std::abs (dy) < 1.0e-4f) return;
    const auto s = spec();
    dragN = juce::jlimit (0.0, 1.0, dragN + dy / (s.sens * (e.mods.isAltDown() ? 4.0 : 1.0)));
    apply (s, s.min + dragN * (s.max - s.min));
}

void HwKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! spec) return;
    const auto s = spec();
    apply (s, s.def);
}

void HwKnob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (! spec) return;
    const auto s = spec();
    const double dir = (w.deltaY > 0) ? 1.0 : -1.0;
    apply (s, s.value + dir * (s.step > 0 ? s.step : (s.max - s.min) * 0.015));
}

//==============================================================================
void HwButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float capH = wide ? 22.0f : 18.0f;
    auto cap = wide ? juce::Rectangle<float> (r.getX(), r.getY() + 3.0f, r.getWidth(), capH)
                    : juce::Rectangle<float> (46.0f, capH).withCentre ({ r.getCentreX(), r.getY() + 3.0f + capH * 0.5f });
    if (down) cap.translate (0.0f, 1.0f);

    if (lit)
    {
        juce::Path p; p.addRoundedRectangle (cap, 5.0f);
        juce::DropShadow (pal::led.withAlpha (0.6f), 14, {}).drawForPath (g, p);
    }
    g.setColour (pal::slabLine);
    g.fillRoundedRectangle (cap.translated (0.0f, down ? 1.0f : 2.0f), 5.0f);
    g.setGradientFill (lit ? juce::ColourGradient (pal::ledHi, 0.0f, cap.getY(), pal::led, 0.0f, cap.getBottom(), false)
                           : juce::ColourGradient (pal::padHi, 0.0f, cap.getY(), pal::pad, 0.0f, cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 4.0f, cap.getRight() - 4.0f);

    if (dice)
    {
        const auto c = cap.getCentre();
        g.setColour (lit ? juce::Colours::white.withAlpha (0.85f) : pal::slabInk2);
        for (auto o : { juce::Point<float> (-3.5f, -3.5f), { 3.5f, -3.5f }, { 0.0f, 0.0f }, { -3.5f, 3.5f }, { 3.5f, 3.5f } })
            g.fillEllipse (juce::Rectangle<float> (3.2f, 3.2f).withCentre (c + o));
    }

    g.setFont (Fonts::label (11.0f, 0.14f));
    g.setColour (lit ? pal::slabInk : pal::slabInk2);
    g.drawText (label, juce::Rectangle<float> (r.getX(), cap.getBottom() + 6.0f, r.getWidth(), 13.0f), juce::Justification::centred, false);
}

void HwButton::mouseDown (const juce::MouseEvent&)
{
    down = true; repaint();
    if (onPress) onPress();
}

void HwButton::mouseUp (const juce::MouseEvent& e)
{
    down = false; repaint();
    if (onRelease) onRelease();
    if (onClick && getLocalBounds().contains (e.getPosition())) onClick();
}

//==============================================================================
void PadComp::setState (bool on, bool opt, bool out, bool cur, bool silkActive)
{
    if (on == isOn && opt == isOpt && out == isOut && cur == isCur && silkActive == silkAct) return;
    isOn = on; isOpt = opt; isOut = out; isCur = cur; silkAct = silkActive;
    repaint();
}

void PadComp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float size = juce::jmin (r.getWidth() - 12.0f, r.getHeight() - 20.0f);
    auto pad = juce::Rectangle<float> (size, size).withCentre ({ r.getCentreX(), r.getY() + 2.0f + size * 0.5f });

    if (isOut) g.beginTransparencyLayer (0.36f);

    juce::Path p; p.addRoundedRectangle (pad, 7.0f);
    if (isOn) juce::DropShadow (pal::led.withAlpha (0.55f), 20, {}).drawForPath (g, p);
    g.setColour (pal::slabLine);
    g.fillRoundedRectangle (pad.translated (0.0f, 3.0f), 7.0f);
    g.setGradientFill (isOn ? juce::ColourGradient (pal::ledHi, 0.0f, pad.getY(), pal::led, 0.0f, pad.getBottom(), false)
                            : juce::ColourGradient (pal::padHi, 0.0f, pad.getY(), pal::pad, 0.0f, pad.getBottom(), false));
    g.fillPath (p);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) pad.getY() + 1, pad.getX() + 6.0f, pad.getRight() - 6.0f);
    if (isOpt && ! isOn)
    {
        g.setColour (pal::ledLine);
        g.drawRoundedRectangle (pad.reduced (0.5f), 7.0f, 1.0f);
    }
    if (isCur)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (pad.reduced (1.0f), 7.0f, 2.0f);
    }
    g.setFont (Fonts::mono (10.0f));
    g.setColour (isOn ? juce::Colours::white.withAlpha (0.85f) : pal::slabInk2);
    g.drawText (juce::String (index + 1), pad.reduced (7.0f, 5.0f), juce::Justification::topLeft, false);

    if (isOut) g.endTransparencyLayer();

    g.setFont (Fonts::label (10.0f, 0.12f));
    g.setColour (silkAct ? pal::oledAcc : pal::slabInk2);
    g.drawText (silk, juce::Rectangle<float> (r.getX(), pad.getBottom() + 7.0f, r.getWidth(), 12.0f), juce::Justification::centred, false);
}

//==============================================================================
void SeqLcd::setText (juce::AttributedString line1, juce::AttributedString line2)
{
    const auto key = line1.getText() + "|" + line2.getText();
    if (key == lastKey) return;
    lastKey = key;
    l1 = std::move (line1);
    l2 = std::move (line2);
    repaint();
}

void SeqLcd::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (pal::oled);
    g.fillRoundedRectangle (r, 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, r.getY(), juce::Colours::transparentBlack, 0.0f, r.getY() + 10.0f, false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    auto inner = r.reduced (14.0f, 8.0f);
    juce::TextLayout t1, t2;
    t1.createLayout (l1, inner.getWidth());
    t2.createLayout (l2, inner.getWidth());
    const float total = t1.getHeight() + 4.0f + t2.getHeight();
    auto y = inner.getY() + (inner.getHeight() - total) * 0.5f;
    t1.draw (g, { inner.getX(), y, inner.getWidth(), t1.getHeight() });
    t2.draw (g, { inner.getX(), y + t1.getHeight() + 4.0f, inner.getWidth(), t2.getHeight() });
}

} // namespace nova::ui
