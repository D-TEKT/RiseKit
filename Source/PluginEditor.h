#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace colours
{
    const juce::Colour background  { 0xff0d0e13 };
    const juce::Colour panel       { 0xff161821 };
    const juce::Colour panelEdge   { 0xff252836 };
    const juce::Colour track       { 0xff2a2d3b };
    const juce::Colour text        { 0xffefebe4 };
    const juce::Colour textDim     { 0xff8d8c94 };
    const juce::Colour fireRed     { 0xffff3b2f };
    const juce::Colour fireOrange  { 0xffff8a1f };
    const juce::Colour fireYellow  { 0xffffd23f };
}

//==============================================================================
class RiseLook : public juce::LookAndFeel_V4
{
public:
    RiseLook();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;

private:
    void drawBigDial (juce::Graphics&, juce::Rectangle<float>, float pos, float start, float end, juce::Slider&);
    void drawSmallKnob (juce::Graphics&, juce::Rectangle<float>, float pos, float start, float end, juce::Slider&);
};

//==============================================================================
class RiseKitEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RiseKitEditor (RiseKitProcessor&);
    ~RiseKitEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob& addKnob (const char* paramId, const juce::String& title, bool bipolar = false);
    void placeKnob (Knob&, juce::Rectangle<int> area);

    RiseKitProcessor& proc;
    RiseLook look;
    juce::TooltipWindow tooltips { this, 600 };

    juce::ComboBox presetBox;

    juce::Slider smileDial;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smileAttachment;

    juce::TextButton riseButton { "RISE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> riseAttachment;

    juce::ComboBox lengthBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lengthAttachment;

    juce::ToggleButton autoDropButton { "Auto drop at the end" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoDropAttachment;

    juce::ToggleButton safetyButton { "Safety clip" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> safetyAttachment;

    juce::ComboBox reverbTypeBox, delayTimeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> reverbTypeAttachment, delayTimeAttachment;

    juce::ToggleButton pingPongButton { "Ping-pong" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAttachment;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* curveKnob = nullptr;
    std::vector<Knob*> characterKnobs, spaceKnobs, outputKnobs;

    // Layout areas (set in resized, drawn in paint)
    juce::Rectangle<int> dialPanel, risePanel, characterPanel, spacePanel, outputPanel, progressArea;

    float shownLive = -1.0f, shownProgress = -1.0f;
    int shownProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiseKitEditor)
};
