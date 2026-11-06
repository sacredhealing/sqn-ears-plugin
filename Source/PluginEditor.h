#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

struct SqnBrowser : juce::WebBrowserComponent
{
    using WebBrowserComponent::WebBrowserComponent;
    bool pageAboutToLoad (const juce::String& url) override { return url.startsWith (getResourceProviderRoot()); }
};

class SqnEarsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SqnEarsEditor (SqnEarsProcessor&);
    ~SqnEarsEditor() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff030303)); }
    void resized() override { web.setBounds (getLocalBounds()); }
    int getControlParameterIndex (juce::Component&) override { return paramIndex.getControlParameterIndex(); }

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> resource (const juce::String& url);

    SqnEarsProcessor& proc;
    juce::WebToggleButtonRelay listenR { "listen" }, fixR { "fix" }, limiterR { "limiter" }, binR { "binaural" };
    juce::WebComboBoxRelay soundR { "sound" }, binStateR { "binState" }, binCarR { "binCarrier" };
    juce::WebSliderRelay amountR { "amount" }, consoleR { "console" }, tapeR { "tape" }, clarR { "clarity" }, widthR { "widthUser" }, harshR { "harsh" }, glueR { "glue" }, binVolR { "binVol" }, loudR { "loudOff" };
    juce::WebControlParameterIndexReceiver paramIndex;

    SqnBrowser web { juce::WebBrowserComponent::Options {}
                         .withNativeIntegrationEnabled()
                         .withKeepPageLoadedWhenBrowserIsHidden()
                         .withOptionsFrom (listenR).withOptionsFrom (fixR).withOptionsFrom (limiterR).withOptionsFrom (binR)
                         .withOptionsFrom (soundR).withOptionsFrom (binStateR).withOptionsFrom (binCarR).withOptionsFrom (binVolR).withOptionsFrom (loudR)
                         .withOptionsFrom (amountR).withOptionsFrom (consoleR).withOptionsFrom (tapeR).withOptionsFrom (clarR).withOptionsFrom (widthR).withOptionsFrom (harshR).withOptionsFrom (glueR)
                         .withOptionsFrom (paramIndex)
                         .withResourceProvider ([this] (const auto& url) { return resource (url); }) };

    juce::WebToggleButtonParameterAttachment listenA, fixA, limiterA, binA;
    juce::WebComboBoxParameterAttachment soundA, binStateA, binCarA;
    juce::WebSliderParameterAttachment amountA, consoleA, tapeA, clarA, widthA, harshA, glueA, binVolA, loudA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqnEarsEditor)
};
