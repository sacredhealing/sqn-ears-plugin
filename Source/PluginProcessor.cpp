#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout SqnEarsProcessor::layout()
{
    juce::StringArray names;
    for (auto& s : sqn::styles()) names.add (s.name);
    auto pct = juce::AudioParameterFloatAttributes().withLabel ("%");
    APVTS::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "listen", 1 }, "Listen", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fix", 1 }, "Fix", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sound", 1 }, "Sound", names, 0));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amount", 1 }, "Amount", juce::NormalisableRange<float> (0, 100, 0.1f), 100.0f, pct));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "console", 1 }, "Vintage Console 1973", juce::NormalisableRange<float> (0, 100, 0.1f), 0.0f, pct));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "harsh", 1 }, "Harshness tamer", juce::NormalisableRange<float> (0, 100, 0.1f), 50.0f, pct));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "glue", 1 }, "Glue", juce::NormalisableRange<float> (0, 100, 0.1f), 50.0f, pct));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "limiter", 1 }, "Loud (peak limiter)", true));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "loudOff", 1 }, "Loudness +/-", juce::NormalisableRange<float> (-4, 4, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "binaural", 1 }, "Binaural beats", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "binState", 1 }, "Binaural state",
        juce::StringArray { "Auto", "Epsilon 0.5 Hz", "Delta 2 Hz", "Theta 6 Hz", "Schumann 7.83 Hz", "Alpha 10 Hz", "Beta 14 Hz", "Gamma 40 Hz" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "binCarrier", 1 }, "Binaural frequency",
        juce::StringArray { "Auto (in key)", "108 Hz", "174 Hz", "285 Hz", "396 Hz", "417 Hz", "432 Hz", "528 Hz", "639 Hz", "741 Hz", "852 Hz", "963 Hz" }, 0));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "binVol", 1 }, "Binaural volume", juce::NormalisableRange<float> (-40, -6, 0.1f), -22.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

SqnEarsProcessor::SqnEarsProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SQNEARS", layout())
{
    pListen = state.getRawParameterValue ("listen"); pFix = state.getRawParameterValue ("fix");
    pSound = state.getRawParameterValue ("sound"); pAmount = state.getRawParameterValue ("amount");
    pConsole = state.getRawParameterValue ("console"); pHarsh = state.getRawParameterValue ("harsh");
    pGlue = state.getRawParameterValue ("glue"); pLimiter = state.getRawParameterValue ("limiter");
    pLoud = state.getRawParameterValue ("loudOff");
    pBin = state.getRawParameterValue ("binaural"); pBinState = state.getRawParameterValue ("binState");
    pBinCar = state.getRawParameterValue ("binCarrier"); pBinVol = state.getRawParameterValue ("binVol");
}

bool SqnEarsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == l.getMainOutputChannelSet();
}

void SqnEarsProcessor::prepareToPlay (double sampleRate, int)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    engine.prepare (sampleRate);
    setLatencySamples (engine.latency());
}

void SqnEarsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;
    sqn::Params p;
    p.listen = pListen->load() > 0.5f; p.fix = pFix->load() > 0.5f; p.limiter = pLimiter->load() > 0.5f;
    p.sound = (int) pSound->load(); p.amount = pAmount->load(); p.console = pConsole->load();
    p.harsh = pHarsh->load(); p.glue = pGlue->load();
    p.loudOff = pLoud->load();
    p.binaural = pBin->load() > 0.5f; p.binState = (int) pBinState->load(); p.binCarrier = (int) pBinCar->load(); p.binVol = pBinVol->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) p.hostBpm = *bpm;
            p.playing = pos->getIsPlaying();
            if (auto ts = pos->getTimeInSeconds()) p.posSec = *ts;
        }
    const juce::SpinLock::ScopedLockType sl (lock);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), p);
}

juce::AudioProcessorEditor* SqnEarsProcessor::createEditor() { return new SqnEarsEditor (*this); }

//============================================================================== the analysis is saved with the session
void SqnEarsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto v = state.copyState();
    juce::ValueTree a ("analysis");
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        const auto& an = engine.an; const auto& f = engine.fx;
        a.setProperty ("have", an.have, nullptr);
        for (int i = 0; i < 9; ++i) a.setProperty ("mb" + juce::String (i), an.mb[i], nullptr);
        for (int i = 0; i < 6; ++i) a.setProperty ("mm" + juce::String (i), an.mm[i], nullptr);
        a.setProperty ("mhb", an.mhb, nullptr); a.setProperty ("mhba", an.mhba, nullptr);
        a.setProperty ("corr", an.corr, nullptr); a.setProperty ("lcorr", an.lcorr, nullptr);
        a.setProperty ("I", an.I, nullptr); a.setProperty ("tp", an.tp, nullptr);
        for (int i = 0; i < 9; ++i) { a.setProperty ("mw" + juce::String (i), an.mw[i], nullptr); a.setProperty ("weq" + juce::String (i), f.weq[i], nullptr); a.setProperty ("eq" + juce::String (i), f.eq[i], nullptr); }
        a.setProperty ("haveW", an.haveW, nullptr); a.setProperty ("songEnd", engine.songEnd, nullptr); a.setProperty ("useW", f.useW, nullptr);
        a.setProperty ("key", an.key, nullptr); a.setProperty ("minor", an.minor, nullptr); a.setProperty ("aRef", an.aRef, nullptr); a.setProperty ("bpm", an.bpm, nullptr);
        a.setProperty ("ready", f.ready, nullptr); a.setProperty ("style", f.style, nullptr);
        a.setProperty ("gain", f.gain, nullptr); a.setProperty ("trim", f.trim, nullptr); a.setProperty ("cal", f.cal, nullptr);
        a.setProperty ("caln", f.caln, nullptr); a.setProperty ("ls", f.ls, nullptr); a.setProperty ("lm", f.lm, nullptr);
        a.setProperty ("pr", f.pr, nullptr); a.setProperty ("hs", f.hs, nullptr); a.setProperty ("mudf", f.mudf, nullptr);
        a.setProperty ("mudg", f.mudg, nullptr); a.setProperty ("width", f.width, nullptr); a.setProperty ("monobass", f.monobass, nullptr);
        a.setProperty ("hbase", f.hbase, nullptr); a.setProperty ("hbabs", f.hbabs, nullptr);
    }
    v.removeChild (v.getChildWithName ("analysis"), nullptr);
    v.appendChild (a, nullptr);
    if (auto xml = v.createXml()) copyXmlToBinary (*xml, dest);
}

void SqnEarsProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto v = juce::ValueTree::fromXml (*xml);
    if (! v.isValid()) return;
    auto a = v.getChildWithName ("analysis");
    v.removeChild (a, nullptr);
    state.replaceState (v);
    if (! a.isValid()) return;
    const juce::SpinLock::ScopedLockType sl (lock);
    auto& an = engine.an; auto& f = engine.fx;
    auto fl = [&] (const char* k, float d) { return (float) a.getProperty (k, d); };
    an.have = a.getProperty ("have", false);
    for (int i = 0; i < 9; ++i) an.mb[i] = (float) a.getProperty ("mb" + juce::String (i), 0.0f);
    for (int i = 0; i < 6; ++i) an.mm[i] = (float) a.getProperty ("mm" + juce::String (i), 0.0f);
    an.mhb = fl ("mhb", 0.12f); an.mhba = fl ("mhba", 1e-6f); an.corr = fl ("corr", 1); an.lcorr = fl ("lcorr", 1);
    an.I = fl ("I", -70); an.tp = fl ("tp", -70);
    for (int i = 0; i < 9; ++i) { an.mw[i] = (float) a.getProperty ("mw" + juce::String (i), 0.0f); f.weq[i] = (float) a.getProperty ("weq" + juce::String (i), 0.0f); f.eq[i] = (float) a.getProperty ("eq" + juce::String (i), 0.0f); }
    an.haveW = a.getProperty ("haveW", false); engine.songEnd = (double) a.getProperty ("songEnd", 0.0); f.useW = a.getProperty ("useW", false);
    an.key = a.getProperty ("key", -1); an.minor = a.getProperty ("minor", false); an.aRef = fl ("aRef", 440); an.bpm = fl ("bpm", 0);
    f.ready = a.getProperty ("ready", false); f.style = a.getProperty ("style", 0);
    f.gain = fl ("gain", 0); f.trim = fl ("trim", 0); f.cal = a.getProperty ("cal", false); f.caln = a.getProperty ("caln", 0);
    f.ls = fl ("ls", 0); f.lm = fl ("lm", 0); f.pr = fl ("pr", 0); f.hs = fl ("hs", 0); f.mudf = fl ("mudf", 250); f.mudg = fl ("mudg", 0);
    f.width = fl ("width", 0); f.monobass = a.getProperty ("monobass", false); f.hbase = fl ("hbase", 0.12f); f.hbabs = fl ("hbabs", 1e-6f);
    engine.restored();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SqnEarsProcessor(); }
