#include "PluginEditor.h"

using namespace nova;
using namespace nova::ui;

namespace
{
    juce::String pad2 (int v) { return juce::String (v).paddedLeft ('0', 2); }

    void addRun (juce::AttributedString& a, const juce::String& text, juce::Colour c, float size = 12.0f)
    {
        a.append (text, Fonts::mono (size), c);
    }
}

//==============================================================================
class NavButton : public juce::Component
{
public:
    explicit NavButton (bool pointsLeft) : left (pointsLeft) {}
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (pal::well);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (isMouseOver() ? pal::ledLine : pal::wellLine);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        const auto c = r.getCentre();
        juce::Path p;
        const float s = 5.0f, dir = left ? 1.0f : -1.0f;
        p.startNewSubPath (c.x + s * 0.6f * dir, c.y - s);
        p.lineTo (c.x - s * 0.6f * dir, c.y);
        p.lineTo (c.x + s * 0.6f * dir, c.y + s);
        g.setColour (isMouseOver() ? pal::ledHi : pal::ink);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void mouseUp (const juce::MouseEvent& e) override { if (onClick && getLocalBounds().contains (e.getPosition())) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool left;
};

//==============================================================================
class OledDisplay : public juce::Component
{
public:
    explicit OledDisplay (NovaBlipwerkProcessor& p) : proc (p) {}

    void showParam (const juce::String& label, const juce::String& value, float norm)
    {
        pLabel = label; pValue = value; pNorm = norm;
        showUntil = juce::Time::getMillisecondCounter() + 1500;
        repaint();
    }

    void tick()
    {
        const int idx = proc.presetIndex.load();
        const bool ed = proc.edited.load();
        if (idx != lastPreset) { lastPreset = idx; showUntil = 0; }
        lastEdited = ed;

        const int w = proc.scopeWrite.load();
        const int N = NovaBlipwerkProcessor::scopeSize;
        for (int i = 0; i < (int) buf.size(); ++i)
            buf[(size_t) i] = proc.scope[(size_t) ((w - (int) buf.size() + i + N) & (N - 1))];
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (pal::oled);
        g.fillRoundedRectangle (r, 10.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, r.getY(), juce::Colours::transparentBlack, 0.0f, r.getY() + 12.0f, false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        auto inner = r.reduced (14.0f, 11.0f);
        auto scopeArea = inner.removeFromRight (inner.getWidth() * 0.36f);
        inner.removeFromRight (14.0f);

        const int idx = proc.presetIndex.load();
        const auto& pr = presets()[(size_t) idx];

        g.setFont (Fonts::mono (10.0f, 0.12f));
        g.setColour (pal::oledDim);
        const juce::String top = "PRESET " + pad2 (idx + 1) + "/" + juce::String ((int) presets().size());
        g.drawText (top, inner.getX(), inner.getY(), 140.0f, 12.0f, juce::Justification::centredLeft, false);
        if (proc.edited.load())
        {
            auto tag = juce::Rectangle<float> (inner.getX() + 118.0f, inner.getY() - 1.0f, 34.0f, 14.0f);
            g.setColour (pal::oledAcc);
            g.drawRoundedRectangle (tag, 3.0f, 1.0f);
            g.setFont (Fonts::mono (9.0f, 0.14f));
            g.drawText ("EDIT", tag, juce::Justification::centred, false);
        }

        nameArea = juce::Rectangle<float> (inner.getX(), inner.getY() + 17.0f, inner.getWidth(), 24.0f);
        g.setFont (Fonts::display (16.5f, 0.06f));
        g.setColour (pal::oledInk);
        g.drawText (juce::String::fromUTF8 (pr.name), nameArea, juce::Justification::centredLeft, true);
        // small caret: the name opens the preset list
        const float caretX = nameArea.getX() + juce::jmin (nameArea.getWidth() - 10.0f,
                                                         juce::GlyphArrangement::getStringWidth (Fonts::display (16.5f, 0.06f), juce::String::fromUTF8 (pr.name)) + 10.0f);
        juce::Path caret;
        caret.addTriangle (caretX, nameArea.getCentreY() - 2.0f, caretX + 8.0f, nameArea.getCentreY() - 2.0f, caretX + 4.0f, nameArea.getCentreY() + 3.0f);
        g.setColour (pal::oledDim);
        g.fillPath (caret);

        auto sub = juce::Rectangle<float> (inner.getX(), nameArea.getBottom() + 4.0f, inner.getWidth(), inner.getBottom() - nameArea.getBottom() - 4.0f);
        if (juce::Time::getMillisecondCounter() < showUntil)
        {
            g.setFont (Fonts::mono (11.5f, 0.08f));
            g.setColour (pal::oledDim);
            g.drawText (pLabel.toUpperCase(), sub.withHeight (16.0f), juce::Justification::centredLeft, true);
            g.setColour (pal::oledInk);
            g.drawText (pValue, sub.withHeight (16.0f), juce::Justification::centredRight, false);
            auto bar = juce::Rectangle<float> (sub.getX(), sub.getY() + 22.0f, sub.getWidth(), 3.0f);
            g.setColour (pal::slabTrack);
            g.fillRoundedRectangle (bar, 1.5f);
            g.setGradientFill (juce::ColourGradient (pal::led, bar.getX(), 0.0f, pal::cyan, bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, pNorm)), 1.5f);
        }
        else
        {
            g.setFont (Fonts::mono (11.0f));
            g.setColour (pal::oledDim);
            g.drawFittedText (juce::String::fromUTF8 (pr.description), sub.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
        }

        drawScope (g, scopeArea);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! nameArea.contains (e.position)) return;
        juce::PopupMenu m;
        const auto& list = presets();
        for (int i = 0; i < (int) list.size(); ++i)
            m.addItem (i + 1, pad2 (i + 1) + "  " + juce::String::fromUTF8 (list[(size_t) i].name), true, i == proc.presetIndex.load());
        juce::Component::SafePointer<OledDisplay> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            safe->proc.loadPreset (result - 1);
            if (! safe->proc.running.load()) safe->proc.audition (60);
            safe->repaint();
        });
    }

    NovaBlipwerkProcessor& proc;

private:
    void drawScope (juce::Graphics& g, juce::Rectangle<float> a)
    {
        g.setColour (pal::oledDim.withAlpha (0.3f));
        for (int i = 1; i < 4; ++i)
            g.drawVerticalLine ((int) (a.getX() + a.getWidth() * i / 4.0f), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

        int s0 = 0;
        const int half = (int) buf.size() / 2;
        for (int i = 1; i < half; ++i)
            if (buf[(size_t) i - 1] < 0.0f && buf[(size_t) i] >= 0.0f) { s0 = i; break; }
        const int span = juce::jmin (700, (int) buf.size() - s0);
        juce::Path p;
        for (int i = 0; i < span; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / (float) (span - 1);
            const float y = a.getCentreY() - juce::jlimit (-1.0f, 1.0f, buf[(size_t) (s0 + i)] * 1.6f) * (a.getHeight() * 0.5f - 3.0f);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (pal::led.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (4.0f));
        g.setGradientFill (juce::ColourGradient (pal::led, a.getX(), 0.0f, pal::cyan, a.getRight(), 0.0f, false));
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }

    std::array<float, 2048> buf {};
    juce::Rectangle<float> nameArea;
    juce::String pLabel, pValue;
    float pNorm = 0.0f;
    juce::uint32 showUntil = 0;
    int lastPreset = -1;
    bool lastEdited = false;
};

//==============================================================================
class SeqPanel : public juce::Component
{
public:
    explicit SeqPanel (NovaBlipwerkProcessor& p) : proc (p), big (true)
    {
        big.spec = [this] { return bigSpec(); };
        addAndMakeVisible (big);

        for (auto* b : { &bStop, &bPlay, &bShift, &bChan, &bRecall, &bStore, &bRand }) addAndMakeVisible (*b);
        bRand.dice = true;
        bPlay.onClick = [this] { proc.pressPlay(); refreshAll(); };
        bStop.onClick = [this] { proc.pressStop(); refreshAll(); };
        bRand.onClick = [this] { randomize (shiftOn()); };
        bShift.onPress   = [this] { shiftPtr = true; shiftDownAt = juce::Time::getMillisecondCounter(); refreshAll(); };
        bShift.onRelease = [this]
        {
            shiftPtr = false;
            if (juce::Time::getMillisecondCounter() - shiftDownAt < 300) shiftLatch = ! shiftLatch;
            refreshAll();
        };
        auto latch = [this] (bool& flag)
        {
            const bool was = flag;
            chan = recall = store = false;
            flag = ! was;
            refreshAll();
        };
        bChan.onClick   = [this, latch] { latch (chan); };
        bRecall.onClick = [this, latch] { latch (recall); };
        bStore.onClick  = [this, latch] { latch (store); };

        addAndMakeVisible (lcd);

        for (int i = 0; i < 16; ++i)
        {
            auto& e = enc[(size_t) i];
            e.spec = [this, i] { return encSpec (i); };
            addAndMakeVisible (e);

            const juce::String silk = i < 8 ? kScales[i].silk : (i < 12 ? kDirSilk[i - 8] : kDivNames[i - 12]);
            pads[(size_t) i] = std::make_unique<PadComp> (i, silk);
            pads[(size_t) i]->onPress = [this] (int index) { padPress (index); };
            addAndMakeVisible (*pads[(size_t) i]);
        }
        refreshAll();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        juce::Path body; body.addRoundedRectangle (r, 16.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 28, { 0, 14 }).drawForPath (g, body);
        g.setGradientFill (juce::ColourGradient (pal::slabHi, 0.0f, r.getY(), pal::slab, 0.0f, r.getBottom(), false));
        g.fillPath (body);
        g.setColour (pal::slabLine);
        g.strokePath (body, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, 16.0f, r.getRight() - 16.0f);

        g.setFont (Fonts::display (11.0f, 0.26f));
        g.setColour (pal::slabInk2);
        g.drawText (juce::CharPointer_UTF8 ("STEP\xc2\xb7" "16"), 20, 16, 150, 14, juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        big.setBounds (18, 36, 150, 136);
        const int bx = 18, bw = 75, by = 186, bh = 38, gap = 8;
        bStop.setBounds   (bx,      by,                   bw, bh);
        bPlay.setBounds   (bx + bw, by,                   bw, bh);
        bShift.setBounds  (bx,      by + (bh + gap),      bw, bh);
        bChan.setBounds   (bx + bw, by + (bh + gap),      bw, bh);
        bRecall.setBounds (bx,      by + 2 * (bh + gap),  bw, bh);
        bStore.setBounds  (bx + bw, by + 2 * (bh + gap),  bw, bh);
        bRand.setBounds   (bx + 8,  by + 3 * (bh + gap) + 6, 2 * bw - 16, 44);

        const int x0 = 190, w = getWidth() - 18 - x0, col = w / 8;
        lcd.setBounds (x0, 18, w, 54);
        for (int i = 0; i < 16; ++i)
        {
            const int row = i / 8, c = i % 8;
            enc[(size_t) i].setBounds (x0 + c * col, 84 + row * 78, col, 76);
            pads[(size_t) i]->setBounds (x0 + c * col, 248 + row * 104, col, 104);
        }
    }

    void tick()
    {
        const bool ks = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
        if (ks != shiftKey) { shiftKey = ks; refreshAll(); return; }

        // repaint encoders when anything they show changed (RAND, recall, host state load)
        juce::int64 sig = (shiftOn() ? 1 : 0) + proc.seq.scale.load() * 3 + (proc.seq.transpose.load() + 30) * 31;
        for (int i = 0; i < 16; ++i) sig = sig * 131 + proc.seq.notes[(size_t) i].load();
        sig = sig * 7 + juce::roundToInt (proc.seq.gate.load() * 100) * 13 + juce::roundToInt (proc.seq.swing.load() * 100) + (proc.seq.legato.load() ? 5 : 0);
        sig = sig * 3 + (juce::int64) proc.hostBpm.load() + (juce::int64) proc.internalBpm.load() * 1000;
        if (sig != encSig) { encSig = sig; for (auto& e : enc) e.repaint(); big.repaint(); }

        refreshPads();
        refreshButtons();
        refreshLcd();
    }

private:
    bool shiftOn() const { return shiftLatch || shiftKey || shiftPtr; }
    enum class View { steps, shift, len, recall, store };
    View view() const { return recall ? View::recall : store ? View::store : chan ? View::len : shiftOn() ? View::shift : View::steps; }

    void msg (const juce::String& t) { msgText = t; msgUntil = juce::Time::getMillisecondCounter() + 1600; refreshLcd(); }

    void refreshAll()
    {
        for (auto& e : enc) e.repaint();
        big.repaint();
        refreshPads(); refreshButtons(); refreshLcd();
    }

    int quantizedNote (int i) const
    {
        return juce::jlimit (0, 127, quantize (proc.seq.notes[(size_t) i].load(), proc.seq.scale.load()) + proc.seq.transpose.load());
    }

    void previewStep (int i)
    {
        if (proc.running.load()) return;
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastPreview < 70) return;
        lastPreview = now;
        proc.audition (juce::jlimit (12, 120, quantizedNote (i)));
    }

    KnobSpec noteSpec (int i)
    {
        KnobSpec s;
        s.label = juce::String (i + 1);
        s.min = 36; s.max = 84; s.step = 1; s.def = 60; s.sens = 300;
        s.value = proc.seq.notes[(size_t) i].load();
        s.set = [this, i] (double v) { proc.seq.notes[(size_t) i].store ((int) v); previewStep (i); };
        s.fmt = [this] (double v)
        {
            return noteName (juce::jlimit (0, 127, quantize ((int) v, proc.seq.scale.load()) + proc.seq.transpose.load()));
        };
        return s;
    }

    KnobSpec encSpec (int i)
    {
        auto& sq = proc.seq;
        if (shiftOn())
        {
            KnobSpec s; s.hl = true; s.sens = 200;
            if (i == 0) { s.label = "GATE";  s.min = 0.05; s.max = 1.0;  s.def = 0.55; s.value = sq.gate.load();  s.set = [&sq] (double v) { sq.gate.store ((float) v); };  s.fmt = [] (double v) { return fmtPct ((float) v); }; return s; }
            if (i == 1) { s.label = "SWING"; s.min = 0.5;  s.max = 0.75; s.def = 0.5;  s.value = sq.swing.load(); s.set = [&sq] (double v) { sq.swing.store ((float) v); }; s.fmt = [] (double v) { return fmtPct ((float) v); }; return s; }
            if (i == 2) { s.label = "LEGATO"; s.min = 0; s.max = 1; s.step = 1; s.def = 0; s.sens = 60; s.value = sq.legato.load() ? 1 : 0; s.set = [&sq] (double v) { sq.legato.store (v > 0.5); }; s.fmt = [] (double v) { return juce::String (v > 0.5 ? "ON" : "OFF"); }; return s; }
            auto n = noteSpec (i); n.dim = true; return n;
        }
        return noteSpec (i);
    }

    KnobSpec bigSpec()
    {
        KnobSpec s;
        if (shiftOn())
        {
            s.label = "TRANSPOSE"; s.hl = true; s.min = -24; s.max = 24; s.step = 1; s.def = 0; s.sens = 260;
            s.value = proc.seq.transpose.load();
            s.set = [this] (double v) { proc.seq.transpose.store ((int) v); };
            s.fmt = [] (double v) { return fmtSigned ((int) v, " st"); };
            return s;
        }
        const double host = proc.hostBpm.load();
        s.min = 60; s.max = 200; s.step = 1; s.def = 140; s.sens = 420;
        if (host > 1.0)
        {
            s.label = juce::CharPointer_UTF8 ("RATE \xc2\xb7 DAW");
            s.value = juce::jlimit (60.0, 200.0, host);
            s.set = [this] (double) { msg ("Das Tempo kommt von der DAW"); };
            s.fmt = [host] (double) { return juce::String (host, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " BPM"; };
        }
        else
        {
            s.label = "RATE";
            s.value = proc.internalBpm.load();
            s.set = [this] (double v) { proc.internalBpm.store ((float) v); };
            s.fmt = [] (double v) { return juce::String ((int) v) + " BPM"; };
        }
        return s;
    }

    void padPress (int i)
    {
        auto& sq = proc.seq;
        if (recall)
        {
            const juce::ScopedLock sl (proc.uiLock);
            sq.load (proc.slots[(size_t) i]); proc.seqName = proc.slots[(size_t) i].name; proc.slot = i;
            recall = false; msg ("SEQ " + pad2 (i + 1) + " geladen");
        }
        else if (store)
        {
            const juce::ScopedLock sl (proc.uiLock);
            proc.slots[(size_t) i] = sq.snapshot (proc.seqName); proc.slot = i;
            store = false; msg ("In SEQ " + pad2 (i + 1) + " gespeichert");
        }
        else if (chan) { sq.len.store (i + 1); chan = false; msg (juce::String (juce::CharPointer_UTF8 ("L\xc3\xa4nge ")) + juce::String (i + 1) + " Steps"); }
        else if (shiftOn())
        {
            if (i < 8)       { sq.scale.store (i); msg ("Skala: " + juce::String::fromUTF8 (kScales[i].name)); }
            else if (i < 12) { sq.dir.store (i - 8); msg ("Richtung: " + juce::String::fromUTF8 (kDirNames[i - 8])); }
            else             { sq.div.store (i - 12); msg ("Raster: " + juce::String (kDivNames[i - 12])); }
        }
        else
        {
            sq.on[(size_t) i].store (! sq.on[(size_t) i].load());
        }
        refreshAll();
    }

    void randomize (bool notesOnly)
    {
        auto& sq = proc.seq;
        const int L = juce::jlimit (1, 16, sq.len.load());
        if (! notesOnly)
        {
            const int k = juce::jlimit (juce::jmin (2, L), L, juce::roundToInt (L * (0.35 + rng.nextDouble() * 0.35)));
            const int rot = rng.nextBool() ? 0 : rng.nextInt (L);
            std::array<bool, 16> e {};
            int prev = -1;
            for (int i = 0; i < L; ++i) { const int v = (i * k) / L; e[(size_t) i] = v != prev; prev = v; }
            for (int i = 0; i < 16; ++i) sq.on[(size_t) i].store (i < L ? e[(size_t) ((i + rot) % L)] : rng.nextBool());
            if (L > 3 && rng.nextBool()) { const int j = rng.nextInt (L); sq.on[(size_t) j].store (! sq.on[(size_t) j].load()); }
            bool any = false;
            for (int i = 0; i < L; ++i) any = any || sq.on[(size_t) i].load();
            if (! any) sq.on[0].store (true);
        }

        const auto& sc = kScales[juce::jlimit (0, 7, sq.scale.load())];
        auto has = [&sc] (int v) { for (int i = 0; i < sc.count; ++i) if (sc.degrees[i] == v) return true; return false; };
        const int third = has (3) ? 3 : has (4) ? 4 : sc.degrees[juce::jmin (2, sc.count - 1)];
        const int fifth = has (7) ? 7 : sc.degrees[juce::jmin (4, sc.count - 1)];
        const int sev   = has (10) ? 10 : has (11) ? 11 : sc.degrees[sc.count - 1];
        std::vector<std::pair<int, double>> pool { { 12, 4 }, { fifth, 3 }, { third, 2.5 }, { sev, 2 }, { -12, 1.5 },
                                                   { 12 + fifth, 1 }, { 12 + third, 1 }, { sc.degrees[1], 1 } };
        std::vector<int> motif { 0 };
        const int size = 2 + rng.nextInt (3);
        while ((int) motif.size() < size && ! pool.empty())
        {
            double tot = 0; for (auto& pe : pool) tot += pe.second;
            double r = rng.nextDouble() * tot;
            size_t j = 0;
            for (; j < pool.size(); ++j) { r -= pool[j].second; if (r <= 0) break; }
            j = juce::jmin (j, pool.size() - 1);
            motif.push_back (pool[j].first);
            pool.erase (pool.begin() + (long) j);
        }
        const double rootW = 0.35 + rng.nextDouble() * 0.3;
        for (int i = 0; i < 16; ++i)
        {
            int iv = rng.nextDouble() < rootW ? 0 : motif[(size_t) (1 + rng.nextInt ((int) motif.size() - 1))];
            if (rng.nextDouble() < 0.07) iv += 12;
            sq.notes[(size_t) i].store (juce::jlimit (36, 84, 60 + iv));
        }
        if (! notesOnly) { const juce::ScopedLock sl (proc.uiLock); proc.seqName = "Random"; }
        msg (notesOnly ? "Neue Noten, Rhythmus bleibt" : "Neues Zufallsmuster");

        bRand.setLit (true);
        juce::Component::SafePointer<SeqPanel> safe (this);
        juce::Timer::callAfterDelay (160, [safe] { if (safe != nullptr) safe->bRand.setLit (false); });
        refreshAll();
    }

    void refreshPads()
    {
        auto& sq = proc.seq;
        const auto v = view();
        const int len = sq.len.load(), cur = proc.curStep.load();
        int slotNow;
        { const juce::ScopedLock sl (proc.uiLock); slotNow = proc.slot; }
        for (int i = 0; i < 16; ++i)
        {
            bool on = false, opt = false, out = false, isCur = false;
            if (v == View::steps)      { out = i >= len; on = sq.on[(size_t) i].load() && ! out; isCur = i == cur; }
            else if (v == View::shift) { opt = true; on = (i < 8 && sq.scale.load() == i) || (i >= 8 && i < 12 && sq.dir.load() == i - 8) || (i >= 12 && sq.div.load() == i - 12); }
            else if (v == View::len)   { opt = i < len; on = i == len - 1; }
            else                       { opt = true; on = i == slotNow; }
            pads[(size_t) i]->setState (on, opt, out, isCur, v == View::shift);
        }
    }

    void refreshButtons()
    {
        bPlay.setLit (proc.running.load());
        bShift.setLit (shiftOn());
        bChan.setLit (chan);
        bRecall.setLit (recall);
        bStore.setLit (store);
    }

    void refreshLcd()
    {
        auto& sq = proc.seq;
        juce::String name; int slotNow;
        { const juce::ScopedLock sl (proc.uiLock); name = proc.seqName; slotNow = proc.slot; }
        const double host = proc.hostBpm.load();
        const juce::String bpmText = host > 1.0 ? juce::String (juce::roundToInt (host)) + " DAW" : juce::String ((int) proc.internalBpm.load());

        juce::AttributedString a;
        auto item = [&a] (const juce::String& key, const juce::String& val)
        {
            addRun (a, key + " ", pal::oledDim);
            addRun (a, val + "    ", pal::oledInk);
        };
        item ("SEQ", pad2 (slotNow + 1) + " " + name);
        item ("SKALA", kScales[juce::jlimit (0, 7, sq.scale.load())].silk);
        item ("RASTER", kDivNames[juce::jlimit (0, 3, sq.div.load())]);
        item ("DIR", kDirSilk[juce::jlimit (0, 3, sq.dir.load())]);
        item ("LEN", juce::String (sq.len.load()));
        item ("TRN", fmtSigned (sq.transpose.load()));
        item ("BPM", bpmText);

        juce::AttributedString b;
        const float hs = 11.5f;
        const auto v = view();
        const bool running = proc.running.load();
        if (juce::Time::getMillisecondCounter() < msgUntil) addRun (b, msgText, pal::oledAcc, hs);
        else if (v == View::shift)
        {
            addRun (b, "SHIFT", pal::oledAcc, hs);
            addRun (b, juce::CharPointer_UTF8 (" \xc2\xb7 Pad 1\xe2\x80\x93" "8 Skala \xc2\xb7 9\xe2\x80\x93" "12 Richtung \xc2\xb7 13\xe2\x80\x93" "16 Raster \xc2\xb7 Encoder 1\xe2\x80\x93" "3 Gate/Swing/Legato \xc2\xb7 gro\xc3\x9f" "er Regler Transpose \xc2\xb7 RAND nur Noten"), pal::oledDim, hs);
        }
        else if (v == View::len)    { addRun (b, juce::CharPointer_UTF8 ("L\xc3\x84NGE"), pal::oledAcc, hs); addRun (b, juce::CharPointer_UTF8 (" \xc2\xb7 tippe das Pad, das der letzte Step sein soll"), pal::oledDim, hs); }
        else if (v == View::recall) { addRun (b, "RECALL", pal::oledAcc, hs); addRun (b, juce::CharPointer_UTF8 (" \xc2\xb7 Pad w\xc3\xa4hlt die Sequenz 1\xe2\x80\x93" "16"), pal::oledDim, hs); }
        else if (v == View::store)  { addRun (b, "STORE", pal::oledAcc, hs); addRun (b, juce::CharPointer_UTF8 (" \xc2\xb7 Pad w\xc3\xa4hlt den Speicherplatz"), pal::oledDim, hs); }
        else if (! running && ! proc.seqOn.load())
        {
            addRun (b, "AUS", pal::oledAcc, hs);
            addRun (b, juce::CharPointer_UTF8 (" \xc2\xb7 PLAY schaltet den Sequencer wieder ein. MIDI-Noten spielen den Synth jederzeit."), pal::oledDim, hs);
        }
        else if (! running && host > 1.0)
        {
            addRun (b, "BEREIT", pal::oledAcc, hs);
            addRun (b, juce::CharPointer_UTF8 (" \xc2\xb7 l\xc3\xa4uft mit der DAW. PLAY hier startet ihn auch im Stopp. RAND w\xc3\xbcrfelt ein Muster."), pal::oledDim, hs);
        }
        else if (! running)
        {
            addRun (b, "PLAY", pal::oledAcc, hs);
            addRun (b, juce::CharPointer_UTF8 (" dr\xc3\xbc" "cken. RAND w\xc3\xbcrfelt ein neues Muster."), pal::oledDim, hs);
        }
        else
        {
            const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));
            addRun (b, juce::String (juce::CharPointer_UTF8 ("Encoder = Tonh\xc3\xb6he")) + dot + "Pad = Step an/aus" + dot + "Gate " + fmtPct (sq.gate.load())
                         + dot + "Swing " + fmtPct (sq.swing.load()) + (sq.legato.load() ? dot + "Legato" : juce::String()),
                    pal::oledDim, hs);
        }
        lcd.setText (a, b);
    }

    NovaBlipwerkProcessor& proc;
    HwKnob big;
    HwButton bStop { "STOP" }, bPlay { "PLAY" }, bShift { "SHIFT" }, bChan { "CHAN" }, bRecall { "RECALL" }, bStore { "STORE" }, bRand { "RAND", true };
    SeqLcd lcd;
    std::array<HwKnob, 16> enc;
    std::array<std::unique_ptr<PadComp>, 16> pads;

    bool shiftLatch = false, shiftKey = false, shiftPtr = false, chan = false, recall = false, store = false;
    juce::uint32 shiftDownAt = 0, msgUntil = 0, lastPreview = 0;
    juce::String msgText;
    juce::int64 encSig = 0;
    juce::Random rng;
};

//==============================================================================
class MainView : public juce::Component, private juce::Timer
{
public:
    explicit MainView (NovaBlipwerkProcessor& p) : proc (p), oled (p), prevBtn (true), nextBtn (false), seqPanel (p)
    {
        addAndMakeVisible (oled);
        addAndMakeVisible (prevBtn);
        addAndMakeVisible (nextBtn);
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (+1); };

        struct K { const char* id; const char* label; int drag; };
        struct Section { const char* title; bool hl; std::vector<K> knobs; };
        const std::vector<Section> sections {
            { "OSC 1",  false, { { "wave", "Wave", 130 }, { "oct", "Octave", 110 }, { "ratio", "FM Ratio", 220 }, { "fm", "FM Amt", 200 } } },
            { "OSC 2",  false, { { "wave2", "Wave", 130 }, { "tune", "Tune", 300 }, { "det", "Detune", 200 }, { "lvl2", "Level", 200 }, { "mode2", "Mode", 70 } } },
            { "BLIP",   true,  { { "pAmt", "Amount", 280 }, { "pDec", "P Decay", 200 }, { "fmDec", "FM Decay", 200 }, { "click", "Click", 200 } } },
            { "FILTER", false, { { "ftype", "Type", 100 }, { "cutoff", "Cutoff", 200 }, { "reso", "Reso", 200 }, { "fEnv", "Env Amt", 200 }, { "fDec", "F Decay", 200 } } },
            { "AMP",    false, { { "aDec", "Decay", 200 }, { "drive", "Drive", 200 }, { "bits", "Crush", 200 } } },
            { "MOTION", false, { { "lRate", "Rate", 150 }, { "lDepth", "Depth", 200 }, { "lDest", "Dest", 100 }, { "rand", "Random", 200 } } },
            { "ECHO \xc2\xb7 SPACE", false, { { "dTime", "Time", 150 }, { "dFb", "Feedback", 200 }, { "dMix", "Mix", 200 }, { "damp", "Damp", 200 }, { "verb", "Space", 200 } } },
        };
        for (const auto& s : sections)
        {
            auto panel = std::make_unique<ModulePanel> (juce::String::fromUTF8 (s.title), s.hl);
            for (const auto& k : s.knobs) panel->add (makeKnob (k.id, k.label, k.drag));
            addAndMakeVisible (*panel);
            modules.push_back (std::move (panel));
        }
        volume = makeKnob ("level", "Volume", 200);
        volume->onRelease = nullptr;
        addAndMakeVisible (*volume);
        addAndMakeVisible (seqPanel);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (pal::bg);

        auto plate = juce::Rectangle<float> (16.0f, 16.0f, 1168.0f, 394.0f);
        juce::Path body; body.addRoundedRectangle (plate, 16.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 30, { 0, 14 }).drawForPath (g, body);
        g.setGradientFill (juce::ColourGradient (pal::plateTop, 0.0f, plate.getY(), pal::plate, 0.0f, plate.getBottom(), false));
        g.fillPath (body);
        g.setGradientFill (juce::ColourGradient (pal::ledWash, plate.getX() + 90.0f, plate.getY(), pal::ledWash.withAlpha (0.0f), plate.getX() + 560.0f, plate.getY() + 140.0f, true));
        g.fillPath (body);
        g.setColour (pal::plateEdge);
        g.strokePath (body, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine (17, 32.0f, plate.getRight() - 16.0f);

        // brand: Nova mark + name
        const juce::Point<float> c (45.0f, 52.0f);
        g.setColour (pal::ledHi);
        g.fillEllipse (juce::Rectangle<float> (6.4f, 6.4f).withCentre (c));
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::pi * 0.25f * (float) i;
            const float r0 = (i % 2 == 0) ? 6.0f : 5.0f, r1 = (i % 2 == 0) ? 10.5f : 8.4f;
            g.drawLine ({ c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1 }, 1.6f);
        }
        g.setFont (Fonts::display (11.0f, 0.42f));
        g.drawText ("NOVA", 61, 45, 120, 14, juce::Justification::centredLeft, false);
        g.setColour (pal::ink);
        g.setFont (Fonts::display (26.0f, 0.14f));
        g.drawText ("BLIPWERK", 34, 66, 280, 32, juce::Justification::centredLeft, false);
        g.setColour (pal::ink2);
        g.setFont (Fonts::label (12.0f, 0.28f));
        g.drawText ("BLIP SYNTH", 35, 100, 200, 14, juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        prevBtn.setBounds (330, 34, 32, 94);
        oled.setBounds (370, 34, 520, 94);
        nextBtn.setBounds (898, 34, 32, 94);
        volume->setBounds (1100, 32, 64, 96);

        layoutRow ({ 0, 1, 2 }, 142);
        layoutRow ({ 3, 4, 5, 6 }, 274);
        seqPanel.setBounds (16, 424, 1168, 460);
    }

private:
    std::unique_ptr<ParamKnob> makeKnob (const char* id, const char* label, int drag)
    {
        auto k = std::make_unique<ParamKnob> (proc.apvts, id, label, drag);
        k->onShow = [this] (const juce::String& l, const juce::String& v, float n) { oled.showParam (l, v, n); };
        k->onRelease = [this] { if (! proc.running.load()) proc.audition (60); };
        return k;
    }

    void layoutRow (std::vector<int> idx, int y)
    {
        const int x0 = 34, total = 1168 - 36, gap = 12, h = 122;
        int minSum = 0, kSum = 0;
        for (int i : idx) { const int k = (int) modules[(size_t) i]->knobs.size(); minSum += k * 62 + 18; kSum += k; }
        const int extra = total - gap * ((int) idx.size() - 1) - minSum;
        int x = x0;
        for (size_t j = 0; j < idx.size(); ++j)
        {
            const int k = (int) modules[(size_t) idx[j]]->knobs.size();
            int w = k * 62 + 18 + extra * k / kSum;
            if (j == idx.size() - 1) w = x0 + total - x;
            modules[(size_t) idx[j]]->setBounds (x, y, w, h);
            x += w + gap;
        }
    }

    void stepPreset (int delta)
    {
        const int n = (int) presets().size();
        proc.loadPreset ((proc.presetIndex.load() + delta + n) % n);
        if (! proc.running.load()) proc.audition (60);
        oled.repaint();
    }

    void timerCallback() override
    {
        oled.tick();
        seqPanel.tick();
    }

    NovaBlipwerkProcessor& proc;
    OledDisplay oled;
    NavButton prevBtn, nextBtn;
    std::vector<std::unique_ptr<ModulePanel>> modules;
    std::unique_ptr<ParamKnob> volume;
    SeqPanel seqPanel;
};

//==============================================================================
NovaBlipwerkEditor::NovaBlipwerkEditor (NovaBlipwerkProcessor& p)
    : AudioProcessorEditor (&p)
{
    setLookAndFeel (&lnf);
    view = std::make_unique<MainView> (p);
    addAndMakeVisible (*view);
    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 3 / 2, baseHeight * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth * 8 / 10, baseHeight * 8 / 10);
}

NovaBlipwerkEditor::~NovaBlipwerkEditor()
{
    view = nullptr;
    setLookAndFeel (nullptr);
}

void NovaBlipwerkEditor::paint (juce::Graphics& g)
{
    g.fillAll (pal::bg);
}

void NovaBlipwerkEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    view->setBounds (0, 0, baseWidth, baseHeight);
    view->setTransform (juce::AffineTransform::scale (scale));
}
