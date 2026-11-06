#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

struct ZionBrowser : juce::WebBrowserComponent
{
    using WebBrowserComponent::WebBrowserComponent;
    bool pageAboutToLoad (const juce::String& url) override { return url.startsWith (getResourceProviderRoot()); }
};

class ZionArkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ZionArkEditor (ZionArkProcessor&);
    ~ZionArkEditor() override = default;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff050505)); }
    void resized() override { if (web) web->setBounds (getLocalBounds()); }

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> resource (const juce::String& url);

    ZionArkProcessor& proc;
    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliders;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggles;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>> combos;
    std::unique_ptr<ZionBrowser> web;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderA;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleA;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZionArkEditor)
};
