#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

const juce::StringArray& ZionArkProcessor::knobIds()
{
    static const juce::StringArray ids { "inG","mix","outG","bk","phRA","phRB","phDepth","phFb","phMix","eSend","time","eInt","eAge","eSpr","eRet",
                                         "sSend","sDrive","sDecay","sTone","sRet","srPitch","srRate","srDepth","srWave","tDrive","tWow","tMatch" };
    return ids;
}
const juce::StringArray& ZionArkProcessor::toggleIds()
{
    static const juce::StringArray ids { "phOn","phSq","sync","aux","byp","cut","mono","thE","thS","warp","siren","kick" };
    return ids;
}
static const char* PADK[6] = { "cut","thE","thS","warp","kick","siren" };
static const char* AID[6] = { "a_cut","a_thE","a_thS","a_warp","a_kick","a_siren" };
static const char* LID[6] = { "l_cut","l_thE","l_thS","l_warp","l_kick","l_siren" };
const juce::StringArray& ZionArkProcessor::choiceIds()
{
    static const juce::StringArray ids = [] { juce::StringArray s { "mode","match" }; for (auto* k : PADK) { s.add (juce::String ("a_") + k); s.add (juce::String ("l_") + k); } return s; }();
    return ids;
}

APVTS::ParameterLayout ZionArkProcessor::layout()
{
    // defaults = the "Ark '77 · Party Time" preset
    const std::map<juce::String, std::pair<juce::String, float>> K {
        { "inG", { "Input", .5f } }, { "mix", { "Mix", 1 } }, { "outG", { "Output", .5f } }, { "bk", { "Big Knob step", 0 } },
        { "phRA", { "Phase rate A", .3f } }, { "phRB", { "Phase rate B", .42f } }, { "phDepth", { "Phase depth", .75f } }, { "phFb", { "Phase feedback", .5f } }, { "phMix", { "Phase mix", .8f } },
        { "eSend", { "Echo send", .15f } }, { "time", { "Echo time", .5f } }, { "eInt", { "Echo repeat", .45f } }, { "eAge", { "Tape age", .55f } }, { "eSpr", { "Echo to spring", .2f } }, { "eRet", { "Echo return", .7f } },
        { "sSend", { "Spring send", .25f } }, { "sDrive", { "Spring drive", .45f } }, { "sDecay", { "Spring decay", .45f } }, { "sTone", { "Spring tone", .55f } }, { "sRet", { "Spring return", .6f } },
        { "srPitch", { "Siren pitch", .5f } }, { "srRate", { "Siren rate", .45f } }, { "srDepth", { "Siren depth", .5f } }, { "srWave", { "Siren shape", 2.0f / 3.0f } },
        { "tDrive", { "Tape drive", .35f } }, { "tWow", { "Tape wow", .2f } }, { "tMatch", { "Match amount", .85f } } };
    const std::map<juce::String, std::pair<juce::String, bool>> T {
        { "phOn", { "Phase on", true } }, { "phSq", { "Phase square sweep", false } }, { "sync", { "Echo host sync", true } }, { "aux", { "AUX 100% wet", false } },
        { "byp", { "Bypass", false } }, { "cut", { "Dry cut", false } }, { "mono", { "Mono", true } },
        { "thE", { "Throw echo", false } }, { "thS", { "Throw spring", false } }, { "warp", { "Warp", false } }, { "siren", { "Siren", false } }, { "kick", { "Kick spring", false } } };
    APVTS::ParameterLayout l;
    for (auto& id : knobIds())
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, K.at (id).first, juce::NormalisableRange<float> (0, 1), K.at (id).second));
    for (auto& id : toggleIds())
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, T.at (id).first, T.at (id).second));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Echo heads", juce::StringArray { "Head 1","Head 2","Head 3","1+2","2+3","1+3","All" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "match", 1 }, "Reference match", juce::StringArray { "Off","Ark '77","Rebel '70","One '72","Now" }, 1));
    // Dub Desk auto-programmer: fire each pad at the end of every N bars, for the last part of that bar
    const char* padName[6] = { "Dry cut", "Throw echo", "Throw spring", "Warp", "Kick", "Siren" };
    const int lenDef[6] = { 1, 0, 1, 2, 1, 2 };
    for (int k = 0; k < 6; ++k)
    {
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { juce::String ("a_") + PADK[k], 1 }, juce::String (padName[k]) + " auto", juce::StringArray { "Off","Every 1 bar","Every 2 bars","Every 4 bars","Every 8 bars" }, 0));
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { juce::String ("l_") + PADK[k], 1 }, juce::String (padName[k]) + " length", juce::StringArray { "Last 1/8","Last 1/4","Last 1/2","Last bar" }, lenDef[k]));
    }
    return l;
}

ZionArkProcessor::ZionArkProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "ZIONARK", layout())
{
    for (auto* ids : { &knobIds(), &toggleIds(), &choiceIds() })
        for (auto& id : *ids) raw[id.toStdString()] = state.getRawParameterValue (id);
    startTimer (300);
}

bool ZionArkProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
    const bool okIn = in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    return okOut && okIn && ! (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono());
}

void ZionArkProcessor::loadSpring (float decayNorm)
{
    std::vector<float> L, R;
    za::makeSpringIR (L, R, za::lin (.8f, 4.5f, decayNorm), sr);
    juce::AudioBuffer<float> ir (2, (int) L.size());
    ir.copyFrom (0, 0, L.data(), (int) L.size());
    ir.copyFrom (1, 0, R.data(), (int) R.size());
    conv.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    loadedDecay = decayNorm;
}

void ZionArkProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = juce::jmax (64, samplesPerBlock);
    engine.prepare (sampleRate, maxBlock);
    conv.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    work.setSize (2, maxBlock);
    loadSpring (r ("sDecay"));
}

void ZionArkProcessor::timerCallback()
{
    // the spring tank is rebuilt off the audio thread when DECAY moves
    const float d = r ("sDecay");
    if (sr > 0 && std::abs (d - loadedDecay) > 0.01f) loadSpring (d);
}

void ZionArkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels(), n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() == 0) return;

    za::Params p;
    p.inG = r ("inG"); p.mix = r ("mix"); p.outG = r ("outG"); p.bk = r ("bk");
    p.phRA = r ("phRA"); p.phRB = r ("phRB"); p.phDepth = r ("phDepth"); p.phFb = r ("phFb"); p.phMix = r ("phMix");
    p.phOn = r ("phOn") > .5f; p.phSq = r ("phSq") > .5f;
    p.eSend = r ("eSend"); p.time = r ("time"); p.eInt = r ("eInt"); p.eAge = r ("eAge"); p.eSpr = r ("eSpr"); p.eRet = r ("eRet");
    p.mode = (int) r ("mode"); p.sync = r ("sync") > .5f;
    p.sSend = r ("sSend"); p.sDrive = r ("sDrive"); p.sDecay = r ("sDecay"); p.sTone = r ("sTone"); p.sRet = r ("sRet");
    p.srPitch = r ("srPitch"); p.srRate = r ("srRate"); p.srDepth = r ("srDepth"); p.srWave = r ("srWave");
    p.tDrive = r ("tDrive"); p.tWow = r ("tWow"); p.tMatch = r ("tMatch"); p.match = (int) r ("match"); p.mono = r ("mono") > .5f;
    p.aux = r ("aux") > .5f; p.byp = r ("byp") > .5f; p.cut = r ("cut") > .5f;
    p.thE = r ("thE") > .5f; p.thS = r ("thS") > .5f; p.warp = r ("warp") > .5f; p.siren = r ("siren") > .5f; p.kick = r ("kick") > .5f;
    bool playing = false; double ppq = 0, barBeats = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) p.hostBpm = *bpm;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q; else playing = false;
            if (auto ts = pos->getTimeSignature()) if (ts->numerator > 0 && ts->denominator > 0) barBeats = ts->numerator * 4.0 / ts->denominator;
        }
    const bool manual[6] = { p.cut, p.thE, p.thS, p.warp, p.kick, p.siren };
    const double qnPerSample = (p.hostBpm > 20 ? p.hostBpm : 120.0) / 60.0 / sr;

    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - start);
        // Dub Desk auto-programmer (follows the Mixbus timeline)
        int mask = 0;
        if (playing)
        {
            static const int EV[5] = { 0, 1, 2, 4, 8 };
            static const double LEN[3] = { 0.5, 1.0, 2.0 };
            const double q = ppq + start * qnPerSample;
            const double barD = std::floor (q / barBeats);
            const long long bar = (long long) barD;
            const double pos = q - barD * barBeats;
            for (int k = 0; k < 6; ++k)
            {
                const int ev = EV[juce::jlimit (0, 4, (int) r (AID[k]))];
                if (ev == 0) continue;
                const int li = juce::jlimit (0, 3, (int) r (LID[k]));
                const double L = juce::jmin (barBeats, li == 3 ? barBeats : LEN[li]);
                bool on = (((bar % ev) + ev) % ev == ev - 1) && pos >= barBeats - L - 1e-9;
                if (k == 4) on = on && pos < barBeats - L + 0.25;   // kick = one hit at the start of its window
                if (on) mask |= 1 << k;
            }
        }
        autoMask = mask;
        p.cut = manual[0] || (mask & 1); p.thE = manual[1] || (mask & 2); p.thS = manual[2] || (mask & 4);
        p.warp = manual[3] || (mask & 8); p.kick = manual[4] || (mask & 16); p.siren = manual[5] || (mask & 32);
        float* L = work.getWritePointer (0); float* R = work.getWritePointer (1);
        juce::FloatVectorOperations::copy (L, buffer.getReadPointer (0, start), len);
        juce::FloatVectorOperations::copy (R, buffer.getReadPointer (inCh > 1 ? 1 : 0, start), len);
        engine.processA (L, R, len, p);
        float* sp[2] = { engine.springL.data(), engine.springR.data() };
        juce::dsp::AudioBlock<float> blk (sp, 2, (size_t) len);
        conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
        engine.processB (L, R, len);
        if (outCh > 1) { buffer.copyFrom (0, start, L, len); buffer.copyFrom (1, start, R, len); }
        else { for (int i = 0; i < len; ++i) L[i] = 0.5f * (L[i] + R[i]); buffer.copyFrom (0, start, L, len); }
    }
    for (int c = juce::jmax (outCh, 2); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

juce::AudioProcessorEditor* ZionArkProcessor::createEditor() { return new ZionArkEditor (*this); }

void ZionArkProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = state.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void ZionArkProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (state.state.getType())) state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZionArkProcessor(); }
