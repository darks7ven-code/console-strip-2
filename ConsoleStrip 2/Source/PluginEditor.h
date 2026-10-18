#pragma once
#include "PluginProcessor.h"

// Look & feel "console" : potards à capuchon coloré, boutons carrés à LED, fader de console
class ConsoleLnF : public juce::LookAndFeel_V4
{
public:
    ConsoleLnF();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

// Un potard avec son nom au-dessus et les valeurs min / max en dessous
class ConsoleKnob : public juce::Component
{
public:
    ConsoleKnob (const juce::String& name, const juce::String& minText,
                 const juce::String& maxText, juce::Colour cap);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;

private:
    juce::String name, minText, maxText;
};

class ChannelStripEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChannelStripEditor (ChannelStripProcessor&);
    ~ChannelStripEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct KnobEntry
    {
        std::unique_ptr<ConsoleKnob> knob;
        std::unique_ptr<APVTS::SliderAttachment> att;
        juce::Point<float> centre;
    };
    struct ToggleEntry
    {
        std::unique_ptr<juce::ToggleButton> btn;
        std::unique_ptr<APVTS::ButtonAttachment> att;
        juce::Rectangle<float> rect;
    };

    void addKnob (const juce::String& id, const juce::String& name, const juce::String& minText,
                  const juce::String& maxText, juce::Colour cap, float cx, float cy);
    void addToggle (const juce::String& id, const juce::String& text, float x, float y, float w, float h);
    void timerCallback() override;

    ChannelStripProcessor& proc;
    ConsoleLnF lnf;

    std::vector<KnobEntry> knobs;
    std::vector<ToggleEntry> toggles;

    juce::ComboBox preBox, eqBox, dynBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> preAtt, eqAtt, dynAtt;

    juce::Slider fader, mixBar, outTrimBar;
    std::unique_ptr<APVTS::SliderAttachment> faderAtt, mixAtt, outTrimAtt;

    juce::TextButton undoBtn { "UNDO" }, redoBtn { "REDO" }, aBtn { "A" }, bBtn { "B" }, orderBtn;
    std::unique_ptr<APVTS::ButtonAttachment> orderAtt;

    float inM[2] {}, outM[2] {}, grM = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripEditor)
};
