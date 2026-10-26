#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace apex;

static juce::uint64 packChain (const std::vector<int>& c)
{
    juce::uint64 v = (juce::uint64) juce::jmin ((int) c.size(), (int) NumModules);
    for (size_t k = 0; k < c.size() && k < (size_t) NumModules; ++k)
        v |= ((juce::uint64) (c[k] & 0xF)) << (4 + 4 * k);
    return v;
}

static int unpackChain (juce::uint64 v, int* out)
{
    const int n = (int) (v & 0xF);
    for (int k = 0; k < n; ++k)
        out[k] = (int) ((v >> (4 + 4 * k)) & 0xF);
    return n;
}

ApexProcessor::ApexProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "APEX6", createLayout())
{
    for (int i = 0; i < NumModules; ++i)
    {
        mods[(size_t) i] = createModule (i, apvts);
        onParams[(size_t) i] = apvts.getRawParameterValue (onId (i));
    }
    cab = dynamic_cast<CabModule*> (mods[Cab].get());
    inGain = apvts.getRawParameterValue ("in_gain");
    outGain = apvts.getRawParameterValue ("out_gain");

    setChain (factoryPresets().front().chain);
}

bool ApexProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = l.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ApexProcessor::prepareToPlay (double sr, int block)
{
    maxBlock = juce::jmax (32, block);
    scratch.setSize (2, maxBlock, false, false, true);
    for (auto& m : mods)
    {
        m->prepare (sr, maxBlock);
        m->reset();
    }
    inSm.reset (sr, 0.05);
    outSm.reset (sr, 0.05);
    inSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inGain->load()));
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outGain->load()));
    setLatencySamples (mods[Amp]->latency());
}

void ApexProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels();

    if (buffer.getNumChannels() < 2)
        return;
    if (ins == 0)
        buffer.clear();
    else if (ins == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    BlockInfo bi;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bi.bpm = *b;
            if (auto q = pos->getPpqPosition()) { bi.ppq = *q; bi.hasPpq = true; }
            bi.playing = pos->getIsPlaying();
        }

    const double spb = getSampleRate() * 60.0 / juce::jmax (1.0, bi.bpm);
    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), 2, start, len);
        BlockInfo b2 = bi;
        b2.ppq += start / spb;
        processChunk (chunk, b2);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

void ApexProcessor::processChunk (juce::AudioBuffer<float>& buf, BlockInfo bi)
{
    const int n = buf.getNumSamples();
    auto* L = buf.getWritePointer (0);
    auto* R = buf.getWritePointer (1);

    inSm.setTargetValue (juce::Decibels::decibelsToGain (inGain->load()));
    float pkIn = 0;
    for (int i = 0; i < n; ++i)
    {
        const float g = inSm.getNextValue();
        L[i] *= g; R[i] *= g;
        pkIn = juce::jmax (pkIn, std::abs (L[i]), std::abs (R[i]));
    }
    inLevel.store (pkIn);

    int chain[NumModules];
    const int count = unpackChain (packedChain.load(), chain);
    bool present[NumModules] = {};
    juce::AudioBuffer<float> sc (scratch.getArrayOfWritePointers(), 2, 0, n);

    for (int k = 0; k < count; ++k)
    {
        const int id = chain[k];
        if (id < 0 || id >= NumModules || present[id]) continue;
        present[id] = true;
        mods[(size_t) id]->run (buf, onParams[(size_t) id]->load() > 0.5f, sc, bi);
    }
    for (int id = 0; id < NumModules; ++id)
        if (! present[id] && mods[(size_t) id]->latency() > 0)
            mods[(size_t) id]->runAbsent (buf, sc);

    // output level + a gentle safety ceiling so nothing ever slams past 0 dB
    outSm.setTargetValue (juce::Decibels::decibelsToGain (outGain->load()));
    float pkOut = 0;
    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.getNextValue();
        for (auto* d : { L, R })
        {
            float x = d[i] * g;
            const float a = std::abs (x);
            if (a > 0.8f)
                x = std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
            if (! std::isfinite (x)) x = 0.0f;
            d[i] = x;
            pkOut = juce::jmax (pkOut, std::abs (x));
        }
    }
    outLevel.store (pkOut);
}

// ---------------------------------------------------------------------------------------------
std::vector<int> ApexProcessor::getChain() const
{
    int c[NumModules];
    const int n = unpackChain (packedChain.load(), c);
    return std::vector<int> (c, c + n);
}

void ApexProcessor::setChain (const std::vector<int>& chain)
{
    std::vector<int> clean;
    for (int id : chain)
        if (id >= 0 && id < NumModules && std::find (clean.begin(), clean.end(), id) == clean.end())
            clean.push_back (id);

    packedChain.store (packChain (clean));
    juce::StringArray s;
    for (int id : clean) s.add (juce::String (id));
    apvts.state.setProperty ("chain", s.joinIntoString (","), nullptr);
    ++chainVersion;
}

bool ApexProcessor::loadIR (int slot, const juce::File& file)
{
    if (! cab->loadIR (slot, file))
        return false;
    apvts.state.setProperty ("ir" + juce::String (slot), file.getFullPathName(), nullptr);
    return true;
}

void ApexProcessor::clearIR (int slot)
{
    cab->clearIR (slot);
    apvts.state.removeProperty ("ir" + juce::String (slot), nullptr);
}

juce::String ApexProcessor::getIRName (int slot) const
{
    if (! cab->hasIR (slot)) return {};
    const auto path = apvts.state.getProperty ("ir" + juce::String (slot)).toString();
    return juce::File::isAbsolutePath (path) ? juce::File (path).getFileNameWithoutExtension() : juce::String();
}

void ApexProcessor::applyFactoryPreset (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    const auto& pr = list[(size_t) index];

    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "in_gain" && rp->getParameterID() != "out_gain")
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, v] : pr.values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));

    setChain (pr.chain);
    currentPresetName = pr.name;
    apvts.state.setProperty ("presetName", pr.name, nullptr);
}

// ---------------------------------------------------------------------------------------------
void ApexProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    if (auto xml = st.createXml())
        copyXmlToBinary (*xml, dest);
}

void ApexProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    std::vector<int> chain;
    const auto chainStr = apvts.state.getProperty ("chain").toString();
    if (chainStr.isNotEmpty())
        for (auto& t : juce::StringArray::fromTokens (chainStr, ",", ""))
            chain.push_back (t.getIntValue());
    else
        chain = factoryPresets().front().chain;
    setChain (chain);

    for (int slot = 0; slot < 2; ++slot)
    {
        const auto path = apvts.state.getProperty ("ir" + juce::String (slot)).toString();
        if (juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile()) cab->loadIR (slot, juce::File (path));
        else cab->clearIR (slot);
    }
    currentPresetName = apvts.state.getProperty ("presetName", "Init Rig").toString();
}

juce::AudioProcessorEditor* ApexProcessor::createEditor() { return new ApexEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ApexProcessor(); }
