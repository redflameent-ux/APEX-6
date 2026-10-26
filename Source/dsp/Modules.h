#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Filters.h"

namespace apex
{
struct BlockInfo
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, hasPpq = false;
};

// One unit in the rack. Every unit always works on a 2-channel buffer.
// run() takes care of smooth on/off switching so the power button never clicks.
class Module
{
public:
    explicit Module (juce::AudioProcessorValueTreeState& s) : apvts (s) {}
    virtual ~Module() = default;

    virtual void prepare (double sampleRate, int maxBlockSize);
    virtual void reset() {}
    virtual int latency() const { return 0; }

    void run (juce::AudioBuffer<float>& buf, bool on, juce::AudioBuffer<float>& scratch, const BlockInfo& bi);
    // Called for units that are not in the rack, so the overall delay stays the same.
    void runAbsent (juce::AudioBuffer<float>& buf, juce::AudioBuffer<float>& scratch);

protected:
    virtual void processWet (juce::AudioBuffer<float>& buf, const BlockInfo& bi) = 0;
    virtual void delayDry (juce::AudioBuffer<float>&) {}

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }

    juce::AudioProcessorValueTreeState& apvts;
    double fs = 44100.0;
    int maxBlock = 512;
    juce::SmoothedValue<float> mixGain;
};

// The cab gets its own class because the editor talks to it (loading IR files)
class CabModule : public Module
{
public:
    explicit CabModule (juce::AudioProcessorValueTreeState& s);
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() override;

    bool loadIR (int slot, const juce::File& file);
    void clearIR (int slot);
    bool hasIR (int slot) const { return loaded[slot].load(); }

protected:
    void processWet (juce::AudioBuffer<float>& buf, const BlockInfo& bi) override;

private:
    struct Sim
    {
        Biquad hp, low, dip, pres, lp1, lp2;
        void set (double fs);
        void reset();
        inline float process (float x) noexcept
        {
            return lp2.process (lp1.process (pres.process (dip.process (low.process (hp.process (x))))));
        }
    };

public:
    static constexpr int numSlots = 4;
private:
    juce::dsp::Convolution conv[numSlots];
    std::atomic<bool> loaded[numSlots] = { false, false, false, false };
    Sim sim[numSlots][2];
    Biquad lowcut[2], highcut[2];
    float lastLow = -1, lastHigh = -1;
    juce::SmoothedValue<float> level, weight[numSlots];
    juce::AudioBuffer<float> in, tmp, acc;
    std::atomic<float> *pMix[numSlots], *pPhase[numSlots], *pLow, *pHigh, *pLevel;
};

std::unique_ptr<Module> createModule (int id, juce::AudioProcessorValueTreeState& s);
} // namespace apex
