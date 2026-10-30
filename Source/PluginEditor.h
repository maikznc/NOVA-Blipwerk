#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI.h"

class MainView;

class NovaBlipwerkEditor : public juce::AudioProcessorEditor
{
public:
    explicit NovaBlipwerkEditor (NovaBlipwerkProcessor&);
    ~NovaBlipwerkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 1200, baseHeight = 900;

private:
    nova::ui::NovaLnF lnf;
    std::unique_ptr<MainView> view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaBlipwerkEditor)
};
