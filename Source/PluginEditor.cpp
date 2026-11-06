#include "PluginEditor.h"
#include "BinaryData.h"

static juce::AudioParameterFloat& fparam (SqnEarsProcessor& p, const char* id) { return *dynamic_cast<juce::AudioParameterFloat*> (p.state.getParameter (id)); }

SqnEarsEditor::SqnEarsEditor (SqnEarsProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      listenA (*p.state.getParameter ("listen"), listenR, nullptr),
      fixA (*p.state.getParameter ("fix"), fixR, nullptr),
      limiterA (*p.state.getParameter ("limiter"), limiterR, nullptr),
      soundA (*p.state.getParameter ("sound"), soundR, nullptr),
      amountA (fparam (p, "amount"), amountR, nullptr),
      consoleA (fparam (p, "console"), consoleR, nullptr),
      harshA (fparam (p, "harsh"), harshR, nullptr),
      glueA (fparam (p, "glue"), glueR, nullptr)
{
    proc.engine.uiOpen = true;
    addAndMakeVisible (web);
    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setResizable (true, true);
    setResizeLimits (720, 480, 1600, 1080);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (980.0 / 660.0);
    setSize (900, 606);
    startTimerHz (30);
}

SqnEarsEditor::~SqnEarsEditor() { proc.engine.uiOpen = false; }

void SqnEarsEditor::timerCallback()
{
    auto& r = proc.engine.ro;
    auto* o = new juce::DynamicObject();
    o->setProperty ("state", r.state.load()); o->setProperty ("style", r.style.load()); o->setProperty ("cal", r.cal.load());
    o->setProperty ("lufsIn", r.lufsIn.load()); o->setProperty ("tpIn", r.tpIn.load()); o->setProperty ("corr", r.corr.load());
    o->setProperty ("gain", r.gain.load()); o->setProperty ("secs", r.secs.load()); o->setProperty ("lufsOut", r.lufsOut.load());
    o->setProperty ("peakOut", r.peakOut.load()); o->setProperty ("turnDown", r.turnDown.load());
    o->setProperty ("harsh", r.harsh.load()); o->setProperty ("glue", r.glue.load()); o->setProperty ("lim", r.lim.load());
    o->setProperty ("ls", r.ls.load()); o->setProperty ("lm", r.lm.load()); o->setProperty ("pr", r.pr.load()); o->setProperty ("hs", r.hs.load());
    o->setProperty ("mudf", r.mudf.load()); o->setProperty ("mudg", r.mudg.load()); o->setProperty ("width", r.width.load());
    o->setProperty ("latencyMs", 1000.0 * proc.getLatencySamples() / juce::jmax (1.0, proc.getSampleRate()));
    juce::Array<juce::var> spec;
    for (auto& s : r.spec) spec.add (s.load());
    o->setProperty ("spec", spec);
    web.emitEventIfBrowserIsVisible ("meters", juce::var (o));
}

std::optional<juce::WebBrowserComponent::Resource> SqnEarsEditor::resource (const juce::String& url)
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
