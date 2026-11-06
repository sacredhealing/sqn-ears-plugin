#include "PluginEditor.h"
#include "BinaryData.h"

ZionArkEditor::ZionArkEditor (ZionArkProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    auto opts = juce::WebBrowserComponent::Options {}
                    .withNativeIntegrationEnabled()
                    .withKeepPageLoadedWhenBrowserIsHidden()
                    .withResourceProvider ([this] (const auto& url) { return resource (url); });

    for (auto& id : ZionArkProcessor::knobIds()) { sliders.push_back (std::make_unique<juce::WebSliderRelay> (id)); opts = opts.withOptionsFrom (*sliders.back()); }
    for (auto& id : ZionArkProcessor::toggleIds()) { toggles.push_back (std::make_unique<juce::WebToggleButtonRelay> (id)); opts = opts.withOptionsFrom (*toggles.back()); }
    for (auto& id : ZionArkProcessor::choiceIds()) { combos.push_back (std::make_unique<juce::WebComboBoxRelay> (id)); opts = opts.withOptionsFrom (*combos.back()); }

    web = std::make_unique<ZionBrowser> (opts);
    addAndMakeVisible (*web);

    auto& s = proc.state;
    int i = 0;
    for (auto& id : ZionArkProcessor::knobIds()) sliderA.push_back (std::make_unique<juce::WebSliderParameterAttachment> (*s.getParameter (id), *sliders[(size_t) i++], nullptr));
    i = 0;
    for (auto& id : ZionArkProcessor::toggleIds()) toggleA.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (*s.getParameter (id), *toggles[(size_t) i++], nullptr));
    i = 0;
    for (auto& id : ZionArkProcessor::choiceIds()) comboA.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (*s.getParameter (id), *combos[(size_t) i++], nullptr));

    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setResizable (true, true);
    setResizeLimits (896, 532, 1920, 1140);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1280.0 / 760.0);
    setSize (1280, 760);
    startTimerHz (30);
}

void ZionArkEditor::timerCallback()
{
    auto& r = proc.engine.ro;
    auto* o = new juce::DynamicObject();
    o->setProperty ("in", r.inDb.load()); o->setProperty ("out", r.outDb.load()); o->setProperty ("bpm", r.bpm.load());
    juce::Array<juce::var> mg;
    for (auto& m : r.mg) mg.add (m.load());
    o->setProperty ("mg", mg);
    web->emitEventIfBrowserIsVisible ("meters", juce::var (o));
}

std::optional<juce::WebBrowserComponent::Resource> ZionArkEditor::resource (const juce::String& url)
{
    auto make = [] (const char* data, int size, const char* mime)
    {
        std::vector<std::byte> v ((size_t) size);
        std::memcpy (v.data(), data, (size_t) size);
        return juce::WebBrowserComponent::Resource { std::move (v), juce::String (mime) };
    };
    const auto path = url.upToFirstOccurrenceOf ("?", false, false);
    if (path == "/" || path == "/index.html") return make (BinaryData::index_html, BinaryData::index_htmlSize, "text/html");
    if (path == "/juce/index.js") return make (BinaryData::index_js, BinaryData::index_jsSize, "text/javascript");
    if (path == "/juce/check_native_interop.js") return make (BinaryData::check_native_interop_js, BinaryData::check_native_interop_jsSize, "text/javascript");
    return std::nullopt;
}
