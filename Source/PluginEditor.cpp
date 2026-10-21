#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour bgTop    { 0xff1a0d22 };
const Colour bgBottom { 0xff0b0610 };
const Colour accent   { 0xffff4fa3 };
const Colour dim      { 0xff8a6a99 };
const Colour panel    { 0xff2a1535 };
}

DoseLiteEditor::DoseLiteEditor (DoseLiteProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (780, 540);

    title.setText ("DOSELITE  -  type what you want", dontSendNotification);
    title.setColour (Label::textColourId, accent);
    title.setFont (FontOptions (15.0f, Font::bold));
    addAndMakeVisible (title);

    input.setTextToShowWhenEmpty ("e.g. make my mix good / max autotune / more reverb / demon voice / key of F# minor", dim);
    input.setColour (TextEditor::backgroundColourId, panel);
    input.setColour (TextEditor::textColourId, Colours::white);
    input.setColour (TextEditor::outlineColourId, accent.withAlpha (0.5f));
    input.setColour (TextEditor::focusedOutlineColourId, accent);
    input.setFont (FontOptions (15.0f));
    input.onReturnKey = [this] { send (input.getText()); input.clear(); };
    addAndMakeVisible (input);

    for (auto* b : { &sendBtn, &anotherBtn })
    {
        b->setColour (TextButton::buttonColourId, panel);
        b->setColour (TextButton::textColourOffId, accent);
        addAndMakeVisible (b);
    }
    sendBtn.onClick = [this] { send (input.getText()); input.clear(); };
    anotherBtn.onClick = [this] { send ("another one"); };

    logLabel.setText ("Ready. Say what you want to hear.", dontSendNotification);
    logLabel.setJustificationType (Justification::topLeft);
    logLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.85f));
    logLabel.setColour (Label::backgroundColourId, panel.darker (0.3f));
    logLabel.setFont (FontOptions (13.0f));
    addAndMakeVisible (logLabel);

    for (auto* c : { &keyBox, &scaleBox })
    {
        c->setColour (ComboBox::backgroundColourId, panel);
        c->setColour (ComboBox::textColourId, Colours::white);
        c->setColour (ComboBox::outlineColourId, accent.withAlpha (0.5f));
        addAndMakeVisible (c);
    }
    keyBox.addItemList (StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleBox.addItemList (StringArray { "Chromatic", "Major", "Minor" }, 1);
    keyAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);

    phoneBtn.setColour (ToggleButton::textColourId, Colours::white);
    phoneBtn.setColour (ToggleButton::tickColourId, accent);
    addAndMakeVisible (phoneBtn);
    phoneAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "phone", phoneBtn);

    addKnob ("volume", "VOLUME");   addKnob ("tune", "TUNE");       addKnob ("speed", "SPEED");     addKnob ("shift", "SHIFT");
    addKnob ("warm", "WARM");       addKnob ("presence", "PRESENCE"); addKnob ("air", "AIR");       addKnob ("gate", "GATE");
    addKnob ("comp", "COMP");       addKnob ("deess", "DE-ESS");    addKnob ("drive", "DRIVE");     addKnob ("doubler", "DOUBLER");
    addKnob ("delay", "DELAY");     addKnob ("delayTime", "DLY TIME"); addKnob ("reverb", "REVERB"); addKnob ("mix", "MIX");
}

DoseLiteEditor::~DoseLiteEditor() = default;

void DoseLiteEditor::addKnob (const char* id, const char* label)
{
    auto k = std::make_unique<Knob>();
    k->s.setSliderStyle (Slider::RotaryVerticalDrag);
    k->s.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
    k->s.setColour (Slider::rotarySliderFillColourId, accent);
    k->s.setColour (Slider::rotarySliderOutlineColourId, panel);
    k->s.setColour (Slider::thumbColourId, Colours::white);
    k->s.setColour (Slider::textBoxTextColourId, Colours::white);
    k->s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k->l.setText (label, dontSendNotification);
    k->l.setJustificationType (Justification::centred);
    k->l.setColour (Label::textColourId, dim);
    k->l.setFont (FontOptions (12.0f, Font::bold));
    addAndMakeVisible (k->s);
    addAndMakeVisible (k->l);
    k->a = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->s);
    knobs.push_back (std::move (k));
}

void DoseLiteEditor::send (const String& text)
{
    logLabel.setText (proc.runCommand (text), dontSendNotification);
}

void DoseLiteEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (bgTop, 0, 0, bgBottom, 0, (float) getHeight(), false));
    g.fillAll();
}

void DoseLiteEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    title.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);

    auto row = r.removeFromTop (34);
    anotherBtn.setBounds (row.removeFromRight (120));
    row.removeFromRight (8);
    sendBtn.setBounds (row.removeFromRight (70));
    row.removeFromRight (8);
    input.setBounds (row);

    r.removeFromTop (8);
    logLabel.setBounds (r.removeFromTop (92));
    r.removeFromTop (10);

    auto combos = r.removeFromTop (28);
    keyBox.setBounds (combos.removeFromLeft (90));
    combos.removeFromLeft (8);
    scaleBox.setBounds (combos.removeFromLeft (130));
    combos.removeFromLeft (16);
    phoneBtn.setBounds (combos.removeFromLeft (100));
    r.removeFromTop (10);

    const int cols = 8, rows = 2;
    const int cw = r.getWidth() / cols, ch = r.getHeight() / rows;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int cx = (int) i % cols, cy = (int) i / cols;
        Rectangle<int> cell (r.getX() + cx * cw, r.getY() + cy * ch, cw, ch);
        knobs[i]->l.setBounds (cell.removeFromTop (16));
        knobs[i]->s.setBounds (cell.reduced (4));
    }
}
