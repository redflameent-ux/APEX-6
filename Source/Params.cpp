#include "Params.h"

namespace apex
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Fmt = std::function<juce::String (float)>;

const std::vector<ModuleInfo>& modules()
{
    static const std::vector<ModuleInfo> list = {
        { Gate, "gate", "GATEKEEPER", "noise gate", "DYNAMICS", juce::Colour (0xff5fd3c4),
          {}, { { "gate_thresh", "Threshold" }, { "gate_release", "Release" } }, {}, 0 },

        { Boost, "boost", "TIGHT", "one-knob tightener + boost", "DYNAMICS", juce::Colour (0xff7ee36b),
          {}, { { "boost_tight", "Tight" }, { "boost_edge", "Edge" }, { "boost_level", "Boost" } }, {}, 1 },

        { Crush, "crush", "SCRAPYARD", "bit crusher / fuzz", "WEIRD PEDALS", juce::Colour (0xffe8d64a),
          {}, { { "crush_bits", "Bits" }, { "crush_rate", "Rate" }, { "crush_fuzz", "Fuzz" }, { "crush_mix", "Mix" } }, {}, 2 },

        { Amp, "amp", "APEX HEAD", "4-channel amp", "AMP + CAB", juce::Colour (0xffff5a36),
          { { "amp_channel", "Channel" } },
          { { "amp_gain", "Gain" }, { "amp_bass", "Bass" }, { "amp_mid", "Mid" }, { "amp_treble", "Treble" },
            { "amp_presence", "Presence" }, { "amp_depth", "Depth" }, { "amp_master", "Master" } }, {}, 3 },

        { Cab, "cab", "IR MATRIX", "6-IR blend cab", "AMP + CAB", juce::Colour (0xffffb347),
          {}, { { "cab_mixA", "Mix A" }, { "cab_mixB", "Mix B" }, { "cab_mixC", "Mix C" }, { "cab_mixD", "Mix D" },
                { "cab_mixE", "Mix E" }, { "cab_mixF", "Mix F" },
                { "cab_lowcut", "Low Cut" }, { "cab_highcut", "High Cut" }, { "cab_level", "Level" } }, {}, 4 },

        { Tape, "tape", "WOW TAPE", "warble + tape age", "WEIRD PEDALS", juce::Colour (0xffc9a27e),
          {}, { { "tape_wow", "Wow" }, { "tape_flutter", "Flutter" }, { "tape_age", "Age" }, { "tape_mix", "Mix" } }, {}, 5 },

        { Glitch, "glitch", "GLITCH LOOM", "tempo stutter", "WEIRD PEDALS", juce::Colour (0xffff4fd8),
          { { "glitch_div", "Grid" }, { "glitch_shred", "Shred" } },
          { { "glitch_chance", "Chance" }, { "glitch_reverse", "Reverse" }, { "glitch_mix", "Mix" } }, {}, 6 },

        { Delay, "delay", "ECHO CHAMBER", "dub delay", "SPACE", juce::Colour (0xff3fc7ff),
          { { "delay_sync", "Sync" } },
          { { "delay_time", "Time" }, { "delay_feedback", "Feedback" }, { "delay_tone", "Tone" }, { "delay_mix", "Mix" } },
          { { "delay_pingpong", "Ping-pong" } }, 7 },

        { Verb, "verb", "VOID HALL", "reverb", "SPACE", juce::Colour (0xff9b7bff),
          {}, { { "verb_size", "Size" }, { "verb_damp", "Damping" }, { "verb_width", "Width" }, { "verb_mix", "Mix" } }, {}, 8 },
    };
    return list;
}

const ModuleInfo& info (int id)
{
    for (auto& m : modules())
        if (m.id == id)
            return m;
    return modules().front();
}

juce::String onId (int id) { return info (id).prefix + "_on"; }

// ---------------------------------------------------------------------------------------------
static juce::String db (float v)   { return juce::String (v, 1) + " dB"; }
static juce::String pct (float v)  { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
static juce::String ten (float v)  { return juce::String (v, 1); }
static juce::String hz (float v)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
static juce::String ms (float v)   { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms"; }

static juce::NormalisableRange<float> lin (float a, float b) { return { a, b }; }
static juce::NormalisableRange<float> skew (float a, float b, float centre)
{
    juce::NormalisableRange<float> r (a, b);
    r.setSkewForCentre (centre);
    return r;
}

static void addF (Layout& l, const juce::String& id, const juce::String& name,
                  juce::NormalisableRange<float> r, float def, Fmt fmt)
{
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([fmt] (float v, int) { return fmt (v); })));
}

static void addC (Layout& l, const juce::String& id, const juce::String& name, juce::StringArray items, int def)
{
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
}

static void addB (Layout& l, const juce::String& id, const juce::String& name, bool def)
{
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}

Layout createLayout()
{
    Layout l;

    addF (l, "in_gain", "Input", lin (-24.0f, 24.0f), 0.0f, db);
    addF (l, "out_gain", "Output", lin (-36.0f, 12.0f), 0.0f, db);

    for (auto& m : modules())
        addB (l, onId (m.id), m.name + " On", true);

    // Gatekeeper
    addF (l, "gate_thresh", "Gate Threshold", lin (-90.0f, -10.0f), -60.0f, db);
    addF (l, "gate_release", "Gate Release", skew (5.0f, 500.0f, 80.0f), 80.0f, ms);

    // Tight
    addF (l, "boost_tight", "Tight", lin (0.0f, 1.0f), 0.5f,
          [] (float v) { return hz (40.0f * std::pow (10.0f, v)); });
    addF (l, "boost_edge", "Edge", lin (0.0f, 1.0f), 0.5f, pct);
    addF (l, "boost_level", "Boost", lin (0.0f, 24.0f), 6.0f, db);

    // Apex Head
    addC (l, "amp_channel", "Amp Channel", { "Clean", "Crunch", "Lead", "Modern" }, 2);
    addF (l, "amp_gain", "Amp Gain", lin (0.0f, 10.0f), 6.0f, ten);
    addF (l, "amp_bass", "Amp Bass", lin (0.0f, 10.0f), 5.0f, ten);
    addF (l, "amp_mid", "Amp Mid", lin (0.0f, 10.0f), 5.0f, ten);
    addF (l, "amp_treble", "Amp Treble", lin (0.0f, 10.0f), 5.0f, ten);
    addF (l, "amp_presence", "Amp Presence", lin (0.0f, 10.0f), 5.0f, ten);
    addF (l, "amp_depth", "Amp Depth", lin (0.0f, 10.0f), 5.0f, ten);
    addF (l, "amp_master", "Amp Master", lin (0.0f, 10.0f), 5.0f, ten);

    // IR Matrix
    for (auto slot : { "A", "B", "C", "D", "E", "F" })
    {
        addF (l, juce::String ("cab_mix") + slot, juce::String ("Cab Mix ") + slot, lin (0.0f, 1.0f),
              juce::String (slot) == "A" ? 1.0f : 0.0f, pct);
        addB (l, juce::String ("cab_phase") + slot, juce::String ("Cab Phase Flip ") + slot, false);
    }
    addF (l, "cab_lowcut", "Cab Low Cut", skew (20.0f, 300.0f, 80.0f), 70.0f, hz);
    addF (l, "cab_highcut", "Cab High Cut", skew (2000.0f, 20000.0f, 8000.0f), 9000.0f, hz);
    addF (l, "cab_level", "Cab Level", lin (-24.0f, 12.0f), 0.0f, db);

    // Scrapyard
    addF (l, "crush_bits", "Crush Bits", lin (2.0f, 16.0f), 8.0f, [] (float v) { return juce::String (v, 1) + " bit"; });
    addF (l, "crush_rate", "Crush Rate", skew (1.0f, 32.0f, 6.0f), 4.0f, [] (float v) { return juce::String (v, 1) + "x"; });
    addF (l, "crush_fuzz", "Crush Fuzz", lin (0.0f, 1.0f), 0.3f, pct);
    addF (l, "crush_mix", "Crush Mix", lin (0.0f, 1.0f), 1.0f, pct);

    // Glitch Loom
    addC (l, "glitch_div", "Glitch Grid", { "1/4", "1/8", "1/16", "1/32" }, 2);
    addC (l, "glitch_shred", "Glitch Shred", { "x2", "x4", "x8", "x16" }, 1);
    addF (l, "glitch_chance", "Glitch Chance", lin (0.0f, 1.0f), 0.3f, pct);
    addF (l, "glitch_reverse", "Glitch Reverse", lin (0.0f, 1.0f), 0.2f, pct);
    addF (l, "glitch_mix", "Glitch Mix", lin (0.0f, 1.0f), 1.0f, pct);

    // Wow Tape
    addF (l, "tape_wow", "Tape Wow", lin (0.0f, 1.0f), 0.3f, pct);
    addF (l, "tape_flutter", "Tape Flutter", lin (0.0f, 1.0f), 0.2f, pct);
    addF (l, "tape_age", "Tape Age", lin (0.0f, 1.0f), 0.3f, pct);
    addF (l, "tape_mix", "Tape Mix", lin (0.0f, 1.0f), 1.0f, pct);

    // Echo Chamber
    addC (l, "delay_sync", "Delay Sync", { "Free", "1/4", "1/8 dot", "1/8", "1/8 trip", "1/16" }, 2);
    addF (l, "delay_time", "Delay Time", skew (20.0f, 2000.0f, 400.0f), 375.0f, ms);
    addF (l, "delay_feedback", "Delay Feedback", lin (0.0f, 0.95f), 0.4f, pct);
    addF (l, "delay_tone", "Delay Tone", lin (0.0f, 1.0f), 0.5f, pct);
    addF (l, "delay_mix", "Delay Mix", lin (0.0f, 1.0f), 0.25f, pct);
    addB (l, "delay_pingpong", "Delay Ping-pong", false);

    // Void Hall
    addF (l, "verb_size", "Reverb Size", lin (0.0f, 1.0f), 0.6f, pct);
    addF (l, "verb_damp", "Reverb Damping", lin (0.0f, 1.0f), 0.5f, pct);
    addF (l, "verb_width", "Reverb Width", lin (0.0f, 1.0f), 1.0f, pct);
    addF (l, "verb_mix", "Reverb Mix", lin (0.0f, 1.0f), 0.25f, pct);

    return l;
}
} // namespace apex
