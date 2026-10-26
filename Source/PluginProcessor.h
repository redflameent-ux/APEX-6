#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Modules.h"

class ApexProcessor : public juce::AudioProcessor
{
public:
    ApexProcessor();
    ~ApexProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "APEX-6 Guitar Tone Matrix"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- rack (call from the message thread) ----
    std::vector<int> getChain() const;
    void setChain (const std::vector<int>& chain);
    int getChainVersion() const { return chainVersion.load(); }

    bool loadIR (int slot, const juce::File& file);
    void clearIR (int slot);
    juce::String getIRName (int slot) const;

    void applyFactoryPreset (int index);
    juce::String currentPresetName { "Init Rig" };

    float getInLevel() const  { return inLevel.load(); }
    float getOutLevel() const { return outLevel.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void processChunk (juce::AudioBuffer<float>& buf, apex::BlockInfo bi);

    std::array<std::unique_ptr<apex::Module>, apex::NumModules> mods;
    apex::CabModule* cab = nullptr;
    std::array<std::atomic<float>*, apex::NumModules> onParams {};
    std::atomic<float>*inGain = nullptr, *outGain = nullptr;
    juce::SmoothedValue<float> inSm, outSm;

    std::atomic<juce::uint64> packedChain { 0 };
    std::atomic<int> chainVersion { 0 };

    juce::AudioBuffer<float> scratch, work;
    int maxBlock = 512;
    double freeRunPpq = 0.0;

    std::atomic<float> inLevel { 0 }, outLevel { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexProcessor)
};
