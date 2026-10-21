#pragma once
#include "PluginProcessor.h"

class DoseLiteEditor : public juce::AudioProcessorEditor
{
public:
    explicit DoseLiteEditor (DoseLiteProcessor&);
    ~DoseLiteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider s;
        juce::Label l;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a; // zuletzt deklariert = zuerst zerstoert
    };

    void addKnob (const char* id, const char* label);
    void send (const juce::String& text);

    DoseLiteProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;

    juce::Label title, logLabel;
    juce::TextEditor input;
    juce::TextButton sendBtn { "SEND" }, anotherBtn { "ANOTHER ONE" };
    juce::ComboBox keyBox, scaleBox;
    juce::ToggleButton phoneBtn { "PHONE" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phoneAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoseLiteEditor)
};
