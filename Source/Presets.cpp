#include "Presets.h"
#include "Params.h"

namespace apex
{
// Factory rigs. Presets never touch the IRs you loaded, so your favourite cab stays put.
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = {
        { "Init Rig", { Gate, Boost, Amp, Cab, Delay, Verb }, {} },

        { "Low Tuned Wall", { Gate, Boost, Amp, Cab },
          { { "gate_thresh", -52.0f }, { "gate_release", 60.0f },
            { "boost_tight", 0.7f }, { "boost_edge", 0.6f }, { "boost_level", 4.0f },
            { "amp_channel", 3 }, { "amp_gain", 6.0f }, { "amp_bass", 5.5f }, { "amp_mid", 6.0f },
            { "amp_treble", 6.5f }, { "amp_presence", 6.0f }, { "amp_depth", 6.0f }, { "amp_master", 7.5f },
            { "cab_lowcut", 80.0f }, { "cab_highcut", 8500.0f } } },

        { "Djent Clock", { Gate, Boost, Amp, Cab, Verb },
          { { "gate_thresh", -46.0f }, { "gate_release", 35.0f },
            { "boost_tight", 0.85f }, { "boost_edge", 0.8f }, { "boost_level", 3.0f },
            { "amp_channel", 3 }, { "amp_gain", 5.5f }, { "amp_bass", 4.5f }, { "amp_mid", 7.0f },
            { "amp_treble", 6.0f }, { "amp_presence", 7.0f }, { "amp_depth", 5.0f },
            { "cab_lowcut", 95.0f }, { "cab_highcut", 9500.0f },
            { "verb_size", 0.3f }, { "verb_mix", 0.08f } } },

        { "Lead Scream", { Gate, Boost, Amp, Cab, Delay, Verb },
          { { "gate_thresh", -64.0f },
            { "boost_tight", 0.45f }, { "boost_edge", 0.7f }, { "boost_level", 8.0f },
            { "amp_channel", 2 }, { "amp_gain", 7.0f }, { "amp_mid", 7.0f }, { "amp_treble", 5.5f },
            { "amp_presence", 5.5f }, { "amp_depth", 4.5f },
            { "delay_sync", 3 }, { "delay_feedback", 0.35f }, { "delay_mix", 0.22f }, { "delay_pingpong", 1 },
            { "verb_size", 0.7f }, { "verb_mix", 0.22f } } },

        { "Skank Dub Clean", { Amp, Cab, Delay, Verb },
          { { "amp_channel", 0 }, { "amp_gain", 3.5f }, { "amp_bass", 3.5f }, { "amp_mid", 6.5f },
            { "amp_treble", 7.0f }, { "amp_presence", 6.0f }, { "amp_depth", 2.0f },
            { "cab_lowcut", 150.0f },
            { "delay_sync", 2 }, { "delay_feedback", 0.58f }, { "delay_tone", 0.3f }, { "delay_mix", 0.35f },
            { "verb_size", 0.55f }, { "verb_mix", 0.18f } } },

        { "Glass Ambient", { Amp, Cab, Tape, Delay, Verb },
          { { "amp_channel", 0 }, { "amp_gain", 3.0f }, { "amp_treble", 6.0f },
            { "tape_wow", 0.25f }, { "tape_flutter", 0.15f }, { "tape_age", 0.2f },
            { "delay_sync", 1 }, { "delay_feedback", 0.62f }, { "delay_tone", 0.45f }, { "delay_mix", 0.35f }, { "delay_pingpong", 1 },
            { "verb_size", 0.92f }, { "verb_damp", 0.35f }, { "verb_mix", 0.45f } } },

        { "Rust Engine", { Gate, Crush, Amp, Cab, Glitch, Verb },
          { { "gate_thresh", -50.0f },
            { "crush_bits", 6.0f }, { "crush_rate", 4.0f }, { "crush_fuzz", 0.6f }, { "crush_mix", 0.8f },
            { "amp_channel", 1 }, { "amp_gain", 7.0f }, { "amp_mid", 4.0f }, { "amp_master", 3.0f },
            { "glitch_div", 2 }, { "glitch_shred", 1 }, { "glitch_chance", 0.35f }, { "glitch_reverse", 0.25f },
            { "verb_size", 0.35f }, { "verb_mix", 0.15f } } },

        { "Memphis Tape Haze", { Amp, Cab, Crush, Tape, Delay },
          { { "amp_channel", 1 }, { "amp_gain", 4.0f }, { "amp_treble", 4.0f },
            { "crush_bits", 10.0f }, { "crush_rate", 2.0f }, { "crush_fuzz", 0.1f }, { "crush_mix", 0.4f },
            { "tape_wow", 0.5f }, { "tape_flutter", 0.35f }, { "tape_age", 0.7f },
            { "delay_sync", 2 }, { "delay_feedback", 0.4f }, { "delay_tone", 0.25f }, { "delay_mix", 0.2f } } },

        { "Void Drone", { Amp, Cab, Glitch, Tape, Delay, Verb },
          { { "amp_channel", 1 }, { "amp_gain", 5.0f }, { "amp_master", 2.5f },
            { "glitch_div", 0 }, { "glitch_shred", 2 }, { "glitch_chance", 0.5f }, { "glitch_reverse", 0.8f }, { "glitch_mix", 0.7f },
            { "tape_wow", 0.6f }, { "tape_age", 0.5f },
            { "delay_sync", 1 }, { "delay_feedback", 0.75f }, { "delay_tone", 0.2f }, { "delay_mix", 0.4f },
            { "verb_size", 0.98f }, { "verb_damp", 0.6f }, { "verb_mix", 0.6f } } },

        { "Broken Fuzz", { Gate, Crush, Amp, Cab },
          { { "gate_thresh", -55.0f },
            { "crush_bits", 4.0f }, { "crush_rate", 1.0f }, { "crush_fuzz", 1.0f }, { "crush_mix", 1.0f },
            { "amp_channel", 2 }, { "amp_gain", 5.0f }, { "amp_bass", 6.0f }, { "amp_treble", 4.0f }, { "amp_master", 7.5f } } },
    };
    return list;
}
} // namespace apex
