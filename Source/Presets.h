#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace apex
{
struct FactoryPreset
{
    juce::String name;
    std::vector<int> chain;
    std::vector<std::pair<juce::String, float>> values;   // real values (choice = item index, bool = 0/1)
};

const std::vector<FactoryPreset>& factoryPresets();
} // namespace apex
