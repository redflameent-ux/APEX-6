#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace apex::ui
{
namespace col
{
    const juce::Colour bg     { 0xff0e0e11 };
    const juce::Colour panel  { 0xff1a1a1f };
    const juce::Colour panel2 { 0xff222228 };
    const juce::Colour line   { 0xff2c2c34 };
    const juce::Colour text   { 0xffe8e8ee };
    const juce::Colour dim    { 0xff8a8a96 };
    const juce::Colour accent { 0xffff5a36 };
}

struct LnF : public juce::LookAndFeel_V4
{
    LnF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getPopupMenuFont() override;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label, juce::Colour accent);
    void resized() override;

private:
    void updateValue();
    juce::Slider slider;
    juce::Label name, value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class PowerButton : public juce::ToggleButton
{
public:
    explicit PowerButton (juce::Colour c) : accent (c) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour accent;
};

class RackUnit : public juce::Component
{
public:
    RackUnit (ApexProcessor&, int moduleId);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void poll();
    int preferredHeight() const { return moduleId == Cab ? 176 : 124; }

    const int moduleId;
    std::function<void (RackUnit&, const juce::MouseEvent&)> onDragStart, onDragMove, onDragEnd;
    std::function<void (int)> onRemove;

    static constexpr int stripW = 168;

private:
    void chooseIR (int slot);
    void refreshIR();

    ApexProcessor& proc;
    const ModuleInfo& mi;
    PowerButton power;
    juce::TextButton removeBtn { "x" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::Label> comboLabels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtts;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAtts;
    juce::TextButton irBtn[4], irClear[4], phaseBtn[4];
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAtts;
    float lastMixSig = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragging = false, lastOn = true;
};

class RackView : public juce::Component
{
public:
    explicit RackView (ApexProcessor&);
    void sync();          // rebuild if the processor's chain changed
    void layoutUnits();
    void paint (juce::Graphics&) override;
    void resized() override { layoutUnits(); }
    int contentHeight() const;
    void pollUnits() { for (auto* u : units) u->poll(); }

    static constexpr int gap = 10;

private:
    void rebuild();
    void startDrag (RackUnit&, const juce::MouseEvent&);
    void moveDrag (RackUnit&, const juce::MouseEvent&);
    void endDrag (RackUnit&, const juce::MouseEvent&);
    int targetFor (int y) const;

    ApexProcessor& proc;
    juce::OwnedArray<RackUnit> units;
    int lastVersion = -1;
    RackUnit* dragUnit = nullptr;
    int dragOffset = 0, dragTarget = 0;
};

class Sidebar : public juce::Component
{
public:
    explicit Sidebar (ApexProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void sync();

private:
    void add (int id);
    ApexProcessor& proc;
    juce::OwnedArray<juce::TextButton> buttons;
    std::vector<int> ids;
    juce::StringArray categories { "DYNAMICS", "AMP + CAB", "WEIRD PEDALS", "SPACE" };
    std::vector<std::pair<juce::String, int>> headings;   // category -> y
};
} // namespace apex::ui

class ApexEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ApexEditor (ApexProcessor&);
    ~ApexEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepPreset (int delta);
    void savePreset();
    void loadPreset();
    void drawMeter (juce::Graphics&, juce::Rectangle<int>, float level);

    ApexProcessor& proc;
    apex::ui::LnF lnf;
    apex::ui::Sidebar sidebar;
    juce::Viewport viewport;
    apex::ui::RackView rack;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    apex::ui::Knob inKnob, outKnob;
    std::unique_ptr<juce::FileChooser> chooser;
    float inDisp = 0, outDisp = 0;
    juce::Rectangle<int> inMeter, outMeter;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexEditor)
};
