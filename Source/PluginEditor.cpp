#include "PluginEditor.h"
#include "Presets.h"

using namespace apex;
using namespace apex::ui;

static juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

// =============================================================================================
LnF::LnF()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::PopupMenu::backgroundColourId, col::panel2);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, col::panel2);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::ToggleButton::tickColourId, col::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, col::dim);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::ScrollBar::thumbColourId, col::line.brighter (0.3f));
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                            float start, float end, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float r = (float) juce::jmin (w, h) * 0.5f - 4.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float ang = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (cx, cy, r, r, 0.0f, start, end, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (cx, cy, r, r, 0.0f, start, ang, true);
    g.setColour (s.isEnabled() ? accent : col::dim);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r - 7.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b44), cx, cy - kr, juce::Colour (0xff141418), cx, cy + kr, false));
    g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cx - kr, cy - kr, kr * 2, kr * 2, 1.2f);

    const auto pt = [&] (float len) { return juce::Point<float> (cx + std::sin (ang) * len, cy - std::cos (ang) * len); };
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.drawLine ({ pt (kr * 0.3f), pt (kr * 0.85f) }, 2.5f);
}

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = col::panel2;
    if (b.getToggleState()) c = col::accent.withAlpha (0.5f);
    if (over) c = c.brighter (0.12f);
    if (down) c = c.brighter (0.25f);
    if (! b.isEnabled()) c = c.withAlpha (0.4f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::line.brighter (0.15f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::line.brighter (0.15f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::dim);
    g.fillPath (arrow);
}

juce::Font LnF::getComboBoxFont (juce::ComboBox&) { return font (14.0f, true); }
juce::Font LnF::getTextButtonFont (juce::TextButton&, int) { return font (13.0f, true); }
juce::Font LnF::getPopupMenuFont() { return font (15.0f); }

// =============================================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (slider);

    name.setText (label.toUpperCase(), juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (font (11.5f, true));
    name.setColour (juce::Label::textColourId, col::dim);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (font (11.5f));
    value.setColour (juce::Label::textColourId, col::text);
    value.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (value);

    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.onValueChange = [this] { updateValue(); };
    updateValue();
}

void Knob::updateValue() { value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification); }

void Knob::resized()
{
    auto r = getLocalBounds();
    auto vr = r.removeFromBottom (15);
    auto nr = r.removeFromBottom (15);
    const int sz = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (sz, sz));
    name.setBounds (nr);
    value.setBounds (vr);
}

// =============================================================================================
void PowerButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();
    g.setColour (col::bg);
    g.fillEllipse (r);
    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (r.expanded (1.5f));
        g.setColour (accent);
        g.fillEllipse (r.reduced (3.0f));
    }
    g.setColour (over ? col::text : col::dim);
    g.drawEllipse (r, 1.4f);
}

// =============================================================================================
RackUnit::RackUnit (ApexProcessor& p, int id)
    : moduleId (id), proc (p), mi (info (id)), power (mi.colour)
{
    auto& s = proc.apvts;

    power.setTooltip ("Turn this unit on/off");
    addAndMakeVisible (power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, onId (id), power);

    removeBtn.setTooltip ("Remove from the rack");
    removeBtn.onClick = [this] { if (onRemove) onRemove (moduleId); };
    addAndMakeVisible (removeBtn);

    for (auto& c : mi.choices)
    {
        auto* box = combos.add (new juce::ComboBox());
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (c.id)))
            box->addItemList (cp->choices, 1);
        box->setColour (juce::ComboBox::backgroundColourId, col::panel2);
        addAndMakeVisible (box);
        comboAtts.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (s, c.id, *box));

        auto* l = comboLabels.add (new juce::Label());
        l->setText (c.label.toUpperCase(), juce::dontSendNotification);
        l->setFont (font (11.5f, true));
        l->setColour (juce::Label::textColourId, col::dim);
        addAndMakeVisible (l);
    }

    for (auto& k : mi.knobs)
        addAndMakeVisible (knobs.add (new Knob (s, k.id, k.label, mi.colour)));

    for (auto& t : mi.toggles)
    {
        auto* b = toggles.add (new juce::ToggleButton (t.label));
        addAndMakeVisible (b);
        toggleAtts.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (s, t.id, *b));
    }

    if (id == Cab)
    {
        const char* letters = "ABCD";
        for (int slot = 0; slot < 4; ++slot)
        {
            const juce::String L = juce::String::charToString (letters[slot]);
            irBtn[slot].onClick = [this, slot] { chooseIR (slot); };
            irBtn[slot].setTooltip ("Load an impulse response (.wav) into slot " + L);
            addAndMakeVisible (irBtn[slot]);

            phaseBtn[slot].setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc3\x98")));
            phaseBtn[slot].setClickingTogglesState (true);
            phaseBtn[slot].setTooltip ("Flip the polarity of slot " + L + " (fixes thin, hollow blends)");
            addAndMakeVisible (phaseBtn[slot]);
            phaseAtts.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (s, "cab_phase" + L, phaseBtn[slot]));

            irClear[slot].setButtonText ("x");
            irClear[slot].setTooltip ("Clear slot " + L + " (falls back to the built-in 4x12)");
            irClear[slot].onClick = [this, slot] { proc.clearIR (slot); refreshIR(); };
            addAndMakeVisible (irClear[slot]);
        }
        refreshIR();
    }

    lastOn = power.getToggleState();
}

void RackUnit::refreshIR()
{
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto name = proc.getIRName (slot);
        const juce::String tag = juce::String::charToString ("ABCD"[slot]) + ": ";
        irBtn[slot].setButtonText (tag + (name.isEmpty() ? "built-in 4x12" : name));
        irClear[slot].setEnabled (name.isNotEmpty());
    }
}

void RackUnit::chooseIR (int slot)
{
    chooser = std::make_unique<juce::FileChooser> ("Choose an impulse response", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, slot] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                              {
                                  if (! proc.loadIR (slot, f))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "APEX-6",
                                                                              "That file couldn't be opened as an impulse response.");
                                  refreshIR();
                              }
                          });
}

void RackUnit::poll()
{
    const bool on = power.getToggleState();
    if (on != lastOn)
    {
        lastOn = on;
        for (auto* k : knobs) k->setAlpha (on ? 1.0f : 0.45f);
        for (auto* c : combos) c->setAlpha (on ? 1.0f : 0.45f);
        repaint();
    }
    if (moduleId == Cab)
    {
        refreshIR();
        float sig = 0;
        for (auto L : { "A", "B", "C", "D" })
            sig = sig * 7.0f + proc.apvts.getRawParameterValue (juce::String ("cab_mix") + L)->load();
        if (sig != lastMixSig) { lastMixSig = sig; repaint (0, 0, stripW, getHeight()); }
    }
}

void RackUnit::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool on = power.getToggleState();

    g.setColour (dragging ? col::panel2 : col::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (dragging ? mi.colour.withAlpha (0.7f) : col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    auto strip = r.withWidth ((float) stripW);
    g.setGradientFill (juce::ColourGradient (mi.colour.withAlpha (on ? 0.22f : 0.06f), strip.getX(), strip.getY(),
                                             mi.colour.withAlpha (0.0f), strip.getRight(), strip.getY(), false));
    g.fillRoundedRectangle (strip, 8.0f);
    g.setColour (on ? mi.colour : col::dim);
    g.fillRoundedRectangle (strip.withWidth (4.0f).reduced (0, 10), 2.0f);

    g.setColour (on ? mi.colour : col::dim);
    g.setFont (font (17.0f, true));
    g.drawText (mi.name, 46, 14, stripW - 50, 22, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (font (12.0f));
    g.drawText (mi.tagline, 46, 36, stripW - 50, 16, juce::Justification::centredLeft);
    if (moduleId == Cab)
    {
        // blend bar: how much of each slot is in the mix
        float w[4], total = 0;
        const char* letters = "ABCD";
        for (int i = 0; i < 4; ++i)
        {
            w[i] = proc.apvts.getRawParameterValue (juce::String ("cab_mix") + letters[i])->load();
            total += w[i];
        }
        auto bar = juce::Rectangle<float> (16.0f, 66.0f, (float) stripW - 32.0f, 18.0f);
        g.setColour (col::bg);
        g.fillRoundedRectangle (bar, 4.0f);
        if (total > 0.0f)
        {
            float x = bar.getX();
            for (int i = 0; i < 4; ++i)
            {
                const float segW = bar.getWidth() * w[i] / total;
                if (segW < 1.0f) continue;
                auto seg = juce::Rectangle<float> (x, bar.getY(), segW, bar.getHeight()).reduced (0.5f, 2.0f);
                g.setColour (mi.colour.withMultipliedBrightness (1.0f - 0.2f * (float) i).withAlpha (on ? 0.9f : 0.3f));
                g.fillRoundedRectangle (seg, 3.0f);
                if (segW > 14.0f)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.75f));
                    g.setFont (font (11.0f, true));
                    g.drawText (juce::String::charToString (letters[i]), seg, juce::Justification::centred);
                }
                x += segW;
            }
        }
        g.setColour (col::dim);
        g.setFont (font (10.5f, true));
        g.drawText ("BLEND", 16, 88, 60, 14, juce::Justification::centredLeft);
    }

    g.setFont (font (10.5f, true));
    g.drawText (mi.category, 16, getHeight() - 26, stripW - 40, 14, juce::Justification::centredLeft);

    // grip dots: drag here to reorder
    g.setColour (col::dim.withAlpha (0.6f));
    for (int row = 0; row < 3; ++row)
        for (int c = 0; c < 2; ++c)
            g.fillEllipse ((float) (stripW - 28 + c * 7), (float) (getHeight() - 30 + row * 7), 3.0f, 3.0f);

    g.setColour (col::line);
    g.drawVerticalLine (stripW, 10.0f, (float) getHeight() - 10.0f);
}

void RackUnit::resized()
{
    power.setBounds (14, 14, 24, 24);
    removeBtn.setBounds (getWidth() - 32, 10, 22, 22);

    auto area = getLocalBounds().withTrimmedLeft (stripW + 14).withTrimmedRight (40).reduced (0, 8);

    if (! combos.isEmpty())
    {
        auto colm = area.removeFromLeft (108);
        area.removeFromLeft (10);
        const int each = juce::jmin (52, colm.getHeight() / combos.size());
        auto c = colm.withSizeKeepingCentre (108, each * combos.size());
        for (int i = 0; i < combos.size(); ++i)
        {
            auto slot = c.removeFromTop (each);
            comboLabels[i]->setBounds (slot.removeFromTop (17));
            combos[i]->setBounds (slot.removeFromTop (28));
        }
    }

    if (moduleId == Cab)
    {
        auto row = area.removeFromTop (34);
        area.removeFromTop (8);
        const int cardW = (row.getWidth() - 3 * 8) / 4;
        for (int slot = 0; slot < 4; ++slot)
        {
            auto card = row.removeFromLeft (cardW);
            row.removeFromLeft (8);
            irClear[slot].setBounds (card.removeFromRight (26));
            card.removeFromRight (3);
            phaseBtn[slot].setBounds (card.removeFromRight (28));
            card.removeFromRight (3);
            irBtn[slot].setBounds (card);
        }
        area = area.removeFromTop (108);
    }

    for (auto* k : knobs)
        k->setBounds (area.removeFromLeft (74));

    for (auto* t : toggles)
    {
        area.removeFromLeft (6);
        t->setBounds (area.removeFromLeft (110).withSizeKeepingCentre (110, 28));
    }
}

void RackUnit::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < stripW)
    {
        dragging = true;
        repaint();
        if (onDragStart) onDragStart (*this, e);
    }
}

void RackUnit::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging && onDragMove) onDragMove (*this, e);
}

void RackUnit::mouseUp (const juce::MouseEvent& e)
{
    if (dragging)
    {
        dragging = false;
        repaint();
        if (onDragEnd) onDragEnd (*this, e);
    }
}

// =============================================================================================
RackView::RackView (ApexProcessor& p) : proc (p) { sync(); }

void RackView::sync()
{
    const int v = proc.getChainVersion();
    if (v == lastVersion || dragUnit != nullptr) return;
    lastVersion = v;

    const auto chain = proc.getChain();
    bool same = (int) chain.size() == units.size();
    for (int i = 0; same && i < units.size(); ++i)
        same = units[i]->moduleId == chain[(size_t) i];
    if (! same) rebuild();
}

void RackView::rebuild()
{
    units.clear();
    for (int id : proc.getChain())
    {
        auto* u = units.add (new RackUnit (proc, id));
        u->onDragStart = [this] (RackUnit& r, const juce::MouseEvent& e) { startDrag (r, e); };
        u->onDragMove  = [this] (RackUnit& r, const juce::MouseEvent& e) { moveDrag (r, e); };
        u->onDragEnd   = [this] (RackUnit& r, const juce::MouseEvent& e) { endDrag (r, e); };
        u->onRemove = [this] (int id2)
        {
            auto c = proc.getChain();
            c.erase (std::remove (c.begin(), c.end(), id2), c.end());
            proc.setChain (c);
        };
        addAndMakeVisible (u);
    }
    layoutUnits();
    repaint();
}

int RackView::contentHeight() const
{
    int h = gap;
    for (auto* u : units) h += u->preferredHeight() + gap;
    return h;
}

void RackView::layoutUnits()
{
    const int w = getWidth() - 2 * gap;
    std::vector<RackUnit*> order;
    for (auto* u : units)
        if (u != dragUnit) order.push_back (u);
    if (dragUnit != nullptr)
        order.insert (order.begin() + juce::jlimit (0, (int) order.size(), dragTarget), dragUnit);

    int y = gap;
    for (auto* u : order)
    {
        if (u != dragUnit)
            u->setBounds (gap, y, w, u->preferredHeight());
        y += u->preferredHeight() + gap;
    }
}

int RackView::targetFor (int centreY) const
{
    int idx = 0, y = gap;
    for (auto* u : units)
    {
        if (u == dragUnit) continue;
        if (centreY > y + u->preferredHeight() / 2) ++idx;
        y += u->preferredHeight() + gap;
    }
    return idx;
}

void RackView::startDrag (RackUnit& u, const juce::MouseEvent& e)
{
    dragUnit = &u;
    dragOffset = e.getEventRelativeTo (this).getPosition().y - u.getY();
    dragTarget = units.indexOf (&u);
    u.toFront (false);
}

void RackView::moveDrag (RackUnit& u, const juce::MouseEvent& e)
{
    const int y = juce::jlimit (0, juce::jmax (0, getHeight() - u.getHeight()), e.getEventRelativeTo (this).getPosition().y - dragOffset);
    u.setTopLeftPosition (u.getX(), y);
    const int t = targetFor (y + u.getHeight() / 2);
    if (t != dragTarget)
    {
        dragTarget = t;
        layoutUnits();
    }
}

void RackView::endDrag (RackUnit& u, const juce::MouseEvent&)
{
    const int from = units.indexOf (&u);
    if (from >= 0) units.move (from, dragTarget);
    dragUnit = nullptr;
    layoutUnits();

    std::vector<int> order;
    for (auto* r : units) order.push_back (r->moduleId);
    proc.setChain (order);
    lastVersion = proc.getChainVersion();
}

void RackView::paint (juce::Graphics& g)
{
    if (units.isEmpty())
    {
        g.setColour (col::dim);
        g.setFont (font (16.0f));
        g.drawText ("The rack is empty - add units from the left.", getLocalBounds(), juce::Justification::centred);
    }
}

// =============================================================================================
Sidebar::Sidebar (ApexProcessor& p) : proc (p)
{
    for (auto& cat : categories)
        for (auto& m : modules())
            if (m.category == cat)
            {
                auto* b = buttons.add (new juce::TextButton ("+  " + m.name));
                const int id = m.id;
                b->onClick = [this, id] { add (id); };
                b->setColour (juce::TextButton::textColourOffId, m.colour);
                b->setTooltip (m.tagline);
                addAndMakeVisible (b);
                ids.push_back (id);
            }
    sync();
}

void Sidebar::add (int id)
{
    auto c = proc.getChain();
    if (std::find (c.begin(), c.end(), id) != c.end()) return;
    const int rank = info (id).naturalRank;
    auto pos = std::find_if (c.begin(), c.end(), [rank] (int other) { return info (other).naturalRank > rank; });
    c.insert (pos, id);
    proc.setChain (c);
    proc.apvts.getParameter (onId (id))->setValueNotifyingHost (1.0f);
}

void Sidebar::sync()
{
    const auto c = proc.getChain();
    for (size_t i = 0; i < ids.size(); ++i)
        buttons[(int) i]->setEnabled (std::find (c.begin(), c.end(), ids[i]) == c.end());
}

void Sidebar::resized()
{
    headings.clear();
    int y = 44;
    size_t i = 0;
    for (auto& cat : categories)
    {
        headings.push_back ({ cat, y });
        y += 22;
        for (; i < ids.size() && info (ids[i]).category == cat; ++i)
        {
            buttons[(int) i]->setBounds (14, y, getWidth() - 24, 32);
            y += 37;
        }
        y += 10;
    }
}

void Sidebar::paint (juce::Graphics& g)
{
    g.setColour (col::panel);
    g.fillRect (getLocalBounds());
    g.setColour (col::line);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    g.setColour (col::text);
    g.setFont (font (14.0f, true));
    g.drawText ("ADD TO RACK", 14, 12, getWidth() - 20, 20, juce::Justification::centredLeft);

    g.setColour (col::dim);
    g.setFont (font (11.0f, true));
    for (auto& [name, y] : headings)
        g.drawText (name, 14, y, getWidth() - 20, 18, juce::Justification::centredLeft);

    g.setFont (font (11.5f));
    g.drawFittedText ("Drag a unit by its name to\nreorder. Double-click a knob\nto reset it.",
                      14, getHeight() - 64, getWidth() - 24, 54, juce::Justification::bottomLeft, 3);
}

// =============================================================================================
ApexEditor::ApexEditor (ApexProcessor& p)
    : AudioProcessorEditor (p), proc (p), sidebar (p), rack (p),
      inKnob (p.apvts, "in_gain", "Input", col::accent),
      outKnob (p.apvts, "out_gain", "Output", col::accent)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (sidebar);
    viewport.setViewedComponent (&rack, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);

    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) proc.applyFactoryPreset (idx);
    };
    addAndMakeVisible (presetBox);

    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePreset(); };
    loadBtn.onClick = [this] { loadPreset(); };
    saveBtn.setTooltip ("Save the whole rig (including your IRs) to a file");
    loadBtn.setTooltip ("Load a rig file");
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &loadBtn })
        addAndMakeVisible (b);

    addAndMakeVisible (inKnob);
    addAndMakeVisible (outKnob);

    setSize (1120, 780);
    startTimerHz (30);
    timerCallback();
}

ApexEditor::~ApexEditor() { setLookAndFeel (nullptr); }

void ApexEditor::stepPreset (int delta)
{
    const int n = (int) factoryPresets().size();
    int idx = presetBox.getSelectedId() - 1;
    idx = idx < 0 ? 0 : (idx + delta + n) % n;
    presetBox.setSelectedId (idx + 1, juce::sendNotificationSync);
}

void ApexEditor::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Save rig", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.apex6");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension ("apex6");
                              proc.currentPresetName = f.getFileNameWithoutExtension();
                              proc.apvts.state.setProperty ("presetName", proc.currentPresetName, nullptr);
                              if (auto xml = proc.apvts.copyState().createXml())
                                  xml->writeTo (f);
                          });
}

void ApexEditor::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Load rig", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.apex6");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              if (auto xml = juce::parseXML (f))
                              {
                                  juce::MemoryBlock mb;
                                  juce::AudioProcessor::copyXmlToBinary (*xml, mb);
                                  proc.setStateInformation (mb.getData(), (int) mb.getSize());
                                  proc.currentPresetName = f.getFileNameWithoutExtension();
                              }
                          });
}

void ApexEditor::timerCallback()
{
    rack.sync();
    rack.pollUnits();
    sidebar.sync();

    const int w = viewport.getWidth() - viewport.getScrollBarThickness();
    const int h = juce::jmax (viewport.getHeight(), rack.contentHeight());
    if (rack.getWidth() != w || rack.getHeight() != h)
        rack.setSize (w, h);

    if (proc.currentPresetName != lastPresetName)
    {
        lastPresetName = proc.currentPresetName;
        int found = 0;
        const auto& presets = factoryPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].name == lastPresetName) found = i + 1;
        if (found > 0) presetBox.setSelectedId (found, juce::dontSendNotification);
        else presetBox.setText (lastPresetName, juce::dontSendNotification);
    }

    auto smooth = [] (float& disp, float v) { disp = v > disp ? v : disp * 0.85f + v * 0.15f; };
    smooth (inDisp, proc.getInLevel());
    smooth (outDisp, proc.getOutLevel());
    repaint (inMeter.expanded (2));
    repaint (outMeter.expanded (2));
}

void ApexEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> r, float level)
{
    g.setColour (col::bg);
    g.fillRoundedRectangle (r.toFloat(), 3.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto fill = r.toFloat().reduced (2.0f);
    fill = fill.removeFromBottom (fill.getHeight() * frac);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff3b30), 0, (float) r.getY(),
                                             juce::Colour (0xff4cd964), 0, (float) r.getBottom(), false));
    g.fillRoundedRectangle (fill, 2.0f);
}

void ApexEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // header
    auto head = getLocalBounds().removeFromTop (84);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1d23), 0, 0, col::bg, 0, 84, false));
    g.fillRect (head);
    g.setColour (col::line);
    g.drawHorizontalLine (83, 0.0f, (float) getWidth());

    // faint matrix grid behind the logo
    g.setColour (col::accent.withAlpha (0.05f));
    for (int x = 0; x < 300; x += 12) g.drawVerticalLine (x, 0.0f, 83.0f);
    for (int y = 0; y < 84; y += 12) g.drawHorizontalLine (y, 0.0f, 300.0f);

    g.setFont (font (38.0f, true));
    g.setColour (col::accent.withAlpha (0.25f));
    g.drawText ("APEX-6", 23, 9, 260, 44, juce::Justification::centredLeft);
    g.setColour (col::text);
    g.drawText ("APEX", 22, 8, 260, 44, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.drawText ("-6", 22 + (int) juce::GlyphArrangement::getStringWidth (font (38.0f, true), "APEX"), 8, 80, 44, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (font (12.0f, true));
    g.drawText ("G U I T A R   T O N E   M A T R I X", 24, 52, 300, 16, juce::Justification::centredLeft);

    g.setFont (font (11.0f, true));
    g.drawText ("PRESET", presetBox.getX(), presetBox.getY() - 18, 100, 16, juce::Justification::centredLeft);
    g.drawText ("IN", inMeter.getX() - 4, inMeter.getBottom() + 1, inMeter.getWidth() + 8, 14, juce::Justification::centred);
    g.drawText ("OUT", outMeter.getX() - 6, outMeter.getBottom() + 1, outMeter.getWidth() + 12, 14, juce::Justification::centred);

    drawMeter (g, inMeter, inDisp);
    drawMeter (g, outMeter, outDisp);
}

void ApexEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (84);

    auto right = head.removeFromRight (250).reduced (18, 4);
    outMeter = right.removeFromRight (10).withTrimmedTop (10).withTrimmedBottom (20);
    right.removeFromRight (4);
    outKnob.setBounds (right.removeFromRight (70));
    right.removeFromRight (14);
    inMeter = right.removeFromRight (10).withTrimmedTop (10).withTrimmedBottom (20);
    right.removeFromRight (4);
    inKnob.setBounds (right.removeFromRight (70));

    auto mid = head.withTrimmedLeft (330).withTrimmedRight (10);
    auto row = mid.withSizeKeepingCentre (mid.getWidth(), 32).translated (0, 8);
    prevBtn.setBounds (row.removeFromLeft (32));
    row.removeFromLeft (4);
    presetBox.setBounds (row.removeFromLeft (250));
    row.removeFromLeft (4);
    nextBtn.setBounds (row.removeFromLeft (32));
    row.removeFromLeft (12);
    saveBtn.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (6);
    loadBtn.setBounds (row.removeFromLeft (64));

    sidebar.setBounds (r.removeFromLeft (200));
    viewport.setBounds (r.reduced (6, 6));
    timerCallback();
}
