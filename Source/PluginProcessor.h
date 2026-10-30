#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Params.h"
#include "Sequencer.h"
#include "SynthEngine.h"

class NovaBlipwerkProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    NovaBlipwerkProcessor();
    ~NovaBlipwerkProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Blipwerk"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Shared with the editor
    juce::AudioProcessorValueTreeState apvts;
    nova::SeqLive seq;

    // message-thread only (guarded by uiLock when saving/loading state)
    juce::CriticalSection uiLock;
    std::array<nova::SeqData, 16> slots;
    int slot = 0;
    juce::String seqName { "Hypno" };

    std::atomic<float>  internalBpm { 140.0f };
    std::atomic<double> hostBpm { 0.0 };          // 0 when the host gives no tempo
    std::atomic<bool>   seqOn { true }, jam { false }, running { false }, hostPlaying { false };
    std::atomic<int>    curStep { -1 };
    std::atomic<int>    presetIndex { 0 };
    std::atomic<bool>   edited { false };

    void loadPreset (int index);
    void pressPlay();
    void pressStop();
    void audition (int midiNote) { auditionNote.store (midiNote); }

    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    nova::SynthParams readParams() const;
    void triggerStep (long long k, int sample, double stepLen, double bpm, double ppq);

    struct Event { int sample = 0; int type = 0; int note = 0; double gate = 0; bool held = false; bool glide = false; double beat = 0; };
    enum { evNoteOn, evNoteOff, evAllOff };
    void addEvent (const Event& e) { if (numEvents < (int) events.size()) events[(size_t) numEvents++] = e; }

    std::array<std::atomic<float>*, 31> raw {};
    nova::Engine engine;
    std::vector<float> mono, scratchR;

    std::atomic<bool> stopRequest { false };
    std::atomic<int>  auditionNote { -1 };
    std::atomic<bool> loadingPreset { false };

    double sr = 48000.0, internalPpq = 0.0, freeBeat = 0.0, expectedPpq = -1.0;
    long long lastStepK = 0;
    bool haveLastStep = false, prevOn = false, wasHostPlaying = false;
    juce::Random rng;

    std::array<Event, 512> events {};
    int numEvents = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBlipwerkProcessor)
};
