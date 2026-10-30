// Headless checks for Nova Blipwerk: renders every preset, checks DAW sync, state round trip and draws the UI to a PNG.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

namespace
{
struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 138.0, ppq = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        return p;
    }
};

int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (writer != nullptr) writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

juce::AudioBuffer<float> render (NovaBlipwerkProcessor& p, double seconds, FakePlayHead* ph = nullptr, std::function<void (int)> perBlock = {})
{
    const double sr = 48000.0; const int bs = 512;
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total), block (2, bs);
    juce::MidiBuffer midi;
    for (int pos = 0, blockIndex = 0; pos < total; pos += bs, ++blockIndex)
    {
        const int n = std::min (bs, total - pos);
        block.setSize (2, n, false, false, true);
        p.processBlock (block, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, block, c, 0, n);
        if (ph != nullptr) ph->ppq += n * ph->bpm / 60.0 / sr;
        if (perBlock) perBlock (blockIndex);
    }
    return out;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "test-out");
    outDir.createDirectory();
    const double sr = 48000.0;

    // 1) every preset, free running (no DAW), sequence 1
    std::cout << "Presets (4 s each, internal clock 140 BPM)" << std::endl;
    for (int i = 0; i < (int) nova::presets().size(); ++i)
    {
        NovaBlipwerkProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        p.loadPreset (i);
        p.pressPlay();
        auto b = render (p, 4.0);
        bool finite = true;
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < b.getNumSamples(); ++s)
                if (! std::isfinite (b.getSample (c, s))) finite = false;
        const float peak = b.getMagnitude (0, b.getNumSamples());
        const float rms = 0.5f * (b.getRMSLevel (0, 0, b.getNumSamples()) + b.getRMSLevel (1, 0, b.getNumSamples()));
        const juce::String name = juce::String::fromUTF8 (nova::presets()[(size_t) i].name);
        check (finite && peak > 0.02f && peak < 1.0f,
               name.paddedRight (' ', 16) + " peak " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS  rms "
               + juce::String (juce::Decibels::gainToDecibels (rms), 1) + " dBFS");
        writeWav (outDir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + "-" + name.removeCharacters (" ") + ".wav"), b, sr);
    }

    // 2) DAW sync: 138 BPM, all 16 steps on, 1/16, forward. Expect 16 steps per bar and step 0 on the downbeat.
    std::cout << "DAW sync" << std::endl;
    {
        NovaBlipwerkProcessor p;
        FakePlayHead ph;
        p.setPlayHead (&ph);
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        for (auto& o : p.seq.on) o.store (true);
        p.seq.dir.store (0); p.seq.div.store (2); p.seq.len.store (16); p.seq.swing.store (0.5f);

        int changes = 0, last = -1;
        std::vector<int> order;
        render (p, 4 * 4 * 60.0 / ph.bpm, &ph, [&] (int)
        {
            const int s = p.curStep.load();
            if (s != last) { ++changes; order.push_back (s); last = s; }
        });
        check (changes == 64, "64 steps in 4 bars (got " + juce::String (changes) + ")");
        bool inOrder = order.size() >= 16;
        for (size_t i = 0; i < order.size() && i < 64; ++i) inOrder = inOrder && order[i] == (int) (i % 16);
        check (inOrder, "steps run 1..16 aligned to the bar");
        check (p.running.load() && p.hostBpm.load() == 138.0, "running and following the DAW tempo");

        // loop back to bar 1: sequence must restart at step 1
        ph.ppq = 0.0;
        render (p, 0.05, &ph);
        check (p.curStep.load() == 0, "DAW loop jumps back to step 1");

        // DAW stops: sequencer stops (no jam)
        ph.playing = false;
        render (p, 0.1, &ph);
        check (! p.running.load(), "stops when the DAW stops");
        // PLAY in the plugin while the DAW is stopped: free running
        p.pressPlay();
        render (p, 0.1, &ph);
        check (p.running.load(), "PLAY runs the sequencer while the DAW is stopped");
        p.pressStop();
        render (p, 0.05, &ph);
        check (! p.running.load() && p.curStep.load() == -1, "STOP stops and resets");
    }

    // 3) MIDI input plays the synth even with the sequencer off
    std::cout << "MIDI input" << std::endl;
    {
        NovaBlipwerkProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        p.pressStop();
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 10);
        p.processBlock (b, m);
        float peak = b.getMagnitude (0, 512);
        juce::MidiBuffer none;
        for (int i = 0; i < 4; ++i) { p.processBlock (b, none); peak = std::max (peak, b.getMagnitude (0, 512)); }
        check (peak > 0.02f, "note-on produces sound");
    }

    // 4) state round trip
    std::cout << "State" << std::endl;
    {
        NovaBlipwerkProcessor a;
        a.loadPreset (7);
        a.apvts.getParameter ("cutoff")->setValueNotifyingHost (0.3f);
        a.seq.notes[3].store (71); a.seq.len.store (7); a.seq.transpose.store (-5);
        a.internalBpm.store (128.0f);
        { const juce::ScopedLock sl (a.uiLock); a.seqName = "Test"; a.slot = 4; a.slots[9].notes[0] = 48; }
        juce::MemoryBlock mb;
        a.getStateInformation (mb);

        NovaBlipwerkProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (b.presetIndex.load() == 7 && b.edited.load(), "preset index and edit flag restored");
        check (std::abs (b.apvts.getParameter ("cutoff")->getValue() - 0.3f) < 1.0e-4f, "parameter value restored");
        check (b.seq.notes[3].load() == 71 && b.seq.len.load() == 7 && b.seq.transpose.load() == -5, "sequence restored");
        check (b.internalBpm.load() == 128.0f && b.seqName == "Test" && b.slot == 4 && b.slots[9].notes[0] == 48, "tempo, name and slots restored");
    }

    // 5) UI snapshot of the real editor
    std::cout << "UI" << std::endl;
    {
        NovaBlipwerkProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        p.loadPreset (0);
        p.pressPlay();
        render (p, 1.3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (NovaBlipwerkEditor::baseWidth, NovaBlipwerkEditor::baseHeight);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile ("ui.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        check (os.openedOk() && png.writeImageToStream (img, os), "editor renders to " + f.getFileName());
        ed = nullptr;
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURE(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
