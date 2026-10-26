#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace apex
{
// Every module in the rack. The number is stored in saved sessions, so never reorder these.
enum ModuleId { Gate = 0, Boost, Amp, Cab, Crush, Glitch, Tape, Delay, Verb, NumModules };

struct Ctl { juce::String id, label; };

struct ModuleInfo
{
    int id;
    juce::String prefix, name, tagline, category;
    juce::Colour colour;
    std::vector<Ctl> choices;   // drop-down menus
    std::vector<Ctl> knobs;     // rotary knobs
    std::vector<Ctl> toggles;   // on/off switches (besides the power button)
    int naturalRank;            // where "Add" puts it in the chain
};

const std::vector<ModuleInfo>& modules();
const ModuleInfo& info (int id);
juce::String onId (int id);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace apex
