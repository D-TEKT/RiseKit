#include "PluginEditor.h"

using namespace juce;

namespace
{
    Font font (float size, bool bold = false)
    {
        return Font (FontOptions (size, bold ? Font::bold : Font::plain));
    }

    ColourGradient fireGradient (Rectangle<float> r)
    {
        ColourGradient g (colours::fireRed, r.getBottomLeft(), colours::fireYellow, r.getTopRight(), false);
        g.addColour (0.5, colours::fireOrange);
        return g;
    }

    void drawPanel (Graphics& g, Rectangle<int> area, const String& title)
    {
        auto r = area.toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        g.setColour (colours::textDim);
        g.setFont (font (11.0f, true));
        g.drawText (title, area.getX() + 14, area.getY() + 8, area.getWidth() - 28, 16, Justification::centredLeft);
    }
}

//==============================================================================
RiseLook::RiseLook()
{
    setColour (Slider::textBoxTextColourId,        colours::text);
    setColour (Slider::textBoxOutlineColourId,     Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId,  Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId,   colours::fireOrange.withAlpha (0.4f));

    setColour (Label::textColourId,                colours::text);

    setColour (ComboBox::backgroundColourId,       colours::track);
    setColour (ComboBox::outlineColourId,          colours::panelEdge);
    setColour (ComboBox::textColourId,             colours::text);
    setColour (ComboBox::arrowColourId,            colours::fireOrange);

    setColour (PopupMenu::backgroundColourId,            colours::panel);
    setColour (PopupMenu::textColourId,                  colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::fireOrange);
    setColour (PopupMenu::highlightedTextColourId,       colours::background);

    setColour (ToggleButton::textColourId,         colours::text);
    setColour (ToggleButton::tickColourId,         colours::fireOrange);
    setColour (ToggleButton::tickDisabledColourId, colours::textDim);

    setColour (TextButton::buttonColourId,         colours::track);
    setColour (TextButton::buttonOnColourId,       colours::fireOrange);
    setColour (TextButton::textColourOffId,        colours::text);
    setColour (TextButton::textColourOnId,         colours::background);
}

void RiseLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                 float startAngle, float endAngle, Slider& s)
{
    auto area = Rectangle<int> (x, y, w, h).toFloat();
    if ((bool) s.getProperties().getWithDefault ("big", false))
        drawBigDial (g, area, pos, startAngle, endAngle, s);
    else
        drawSmallKnob (g, area, pos, startAngle, endAngle, s);
}

void RiseLook::drawBigDial (Graphics& g, Rectangle<float> area, float pos, float start, float end, Slider& s)
{
    const float live = jlimit (0.0f, 1.0f, (float) (double) s.getProperties().getWithDefault ("live", 0.0));
    const float radius = jmin (area.getWidth(), area.getHeight()) * 0.5f - 10.0f;
    const auto c = area.getCentre();
    const auto box = Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);

    // Glow behind the face, grows with the live smile
    if (live > 0.001f)
    {
        g.setGradientFill (ColourGradient (colours::fireOrange.withAlpha (0.28f * live), c,
                                           colours::fireOrange.withAlpha (0.0f), c.translated (radius * 1.1f, 0.0f), true));
        g.fillEllipse (box.expanded (14.0f));
    }

    // Track
    Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, start, end, true);
    g.setColour (colours::track);
    g.strokePath (track, PathStrokeType (10.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Knob value (what you set / automate)
    if (pos > 0.001f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, radius, radius, 0.0f, start, start + pos * (end - start), true);
        g.setGradientFill (fireGradient (box));
        g.strokePath (value, PathStrokeType (10.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Live value (knob + auto rise), thin inner ring
    const float innerR = radius - 15.0f;
    Path liveTrack;
    liveTrack.addCentredArc (c.x, c.y, innerR, innerR, 0.0f, start, end, true);
    g.setColour (colours::track.withAlpha (0.6f));
    g.strokePath (liveTrack, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    if (live > 0.001f)
    {
        Path liveArc;
        liveArc.addCentredArc (c.x, c.y, innerR, innerR, 0.0f, start, start + live * (end - start), true);
        g.setColour (colours::fireYellow);
        g.strokePath (liveArc, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // The face: the smile gets wider and deeper as the build-up rises
    const auto faceColour = colours::text.interpolatedWith (colours::fireYellow, live);
    const float eyeR = radius * 0.055f;
    g.setColour (faceColour);
    g.fillEllipse (Rectangle<float> (eyeR * 2.0f, eyeR * 2.0f).withCentre (c.translated (-radius * 0.2f, -radius * 0.2f)));
    g.fillEllipse (Rectangle<float> (eyeR * 2.0f, eyeR * 2.0f).withCentre (c.translated ( radius * 0.2f, -radius * 0.2f)));

    const float mouthW = radius * (0.26f + 0.14f * live);
    const float mouthY = c.y + radius * 0.08f;
    const float depth  = radius * (0.02f + 0.30f * live);
    Path mouth;
    mouth.startNewSubPath (c.x - mouthW, mouthY);
    mouth.quadraticTo (c.x, mouthY + depth * 2.0f, c.x + mouthW, mouthY);
    g.strokePath (mouth, PathStrokeType (radius * 0.045f, PathStrokeType::curved, PathStrokeType::rounded));

    // Percentage
    g.setColour (colours::text);
    g.setFont (font (18.0f, true));
    g.drawText (String (roundToInt (live * 100.0f)) + "%",
                Rectangle<float> (radius, 24.0f).withCentre (c.translated (0.0f, radius * 0.72f)),
                Justification::centred);
}

void RiseLook::drawSmallKnob (Graphics& g, Rectangle<float> area, float pos, float start, float end, Slider& s)
{
    const float radius = jmin (area.getWidth(), area.getHeight()) * 0.5f - 4.0f;
    const auto c = area.getCentre();
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

    Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, start, end, true);
    g.setColour (colours::track);
    g.strokePath (track, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const float angle = start + pos * (end - start);
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.01f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, radius, radius, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (colours::fireOrange);
        g.strokePath (value, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    g.setColour (colours::panelEdge);
    g.fillEllipse (Rectangle<float> (radius * 1.3f, radius * 1.3f).withCentre (c));

    const auto tip = c.getPointOnCircumference (radius * 0.55f, angle);
    g.setColour (colours::text);
    g.drawLine ({ c.getPointOnCircumference (radius * 0.15f, angle), tip }, 2.5f);
}

void RiseLook::drawButtonBackground (Graphics& g, Button& b, const Colour& background, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);

    if (b.getToggleState())
    {
        g.setGradientFill (fireGradient (r));
        g.fillRoundedRectangle (r, 8.0f);
    }
    else
    {
        g.setColour (background.brighter (highlighted ? 0.12f : 0.0f).darker (down ? 0.2f : 0.0f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::fireOrange.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 8.0f, 1.5f);
    }
}

//==============================================================================
RiseKitEditor::RiseKitEditor (RiseKitProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    auto& apvts = proc.apvts;

    // Presets
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != proc.getCurrentProgram())
            proc.setCurrentProgram (index);
        shownProgram = index;
    };
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    addAndMakeVisible (presetBox);

    // Big smile dial
    smileDial.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    smileDial.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    smileDial.getProperties().set ("big", true);
    smileDial.setMouseDragSensitivity (300);
    smileDial.setDoubleClickReturnValue (true, 0.0);
    smileDial.setTooltip ("Smile: drag or automate this. Double-click to reset.");
    addAndMakeVisible (smileDial);
    smileAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, params::smile, smileDial);

    // Auto rise
    riseButton.setClickingTogglesState (true);
    addAndMakeVisible (riseButton);
    riseAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, params::rise, riseButton);

    if (auto* choice = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (params::length)))
        lengthBox.addItemList (choice->choices, 1);
    addAndMakeVisible (lengthBox);
    lengthAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, params::length, lengthBox);

    addAndMakeVisible (autoDropButton);
    autoDropAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, params::autoDrop, autoDropButton);

    addAndMakeVisible (safetyButton);
    safetyAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, params::safety, safetyButton);

    // Space section choices
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (params::reverbType)))
        reverbTypeBox.addItemList (choice->choices, 1);
    addAndMakeVisible (reverbTypeBox);
    reverbTypeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, params::reverbType, reverbTypeBox);

    if (auto* choice = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (params::delayTime)))
        delayTimeBox.addItemList (choice->choices, 1);
    addAndMakeVisible (delayTimeBox);
    delayTimeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, params::delayTime, delayTimeBox);

    addAndMakeVisible (pingPongButton);
    pingPongAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, params::pingPong, pingPongButton);

    curveKnob = &addKnob (params::curve, "Curve", true);

    characterKnobs = { &addKnob (params::filter,    "Filter"),
                       &addKnob (params::resonance, "Reso"),
                       &addKnob (params::phaser,    "Phaser"),
                       &addKnob (params::drive,     "Drive"),
                       &addKnob (params::width,     "Width"),
                       &addKnob (params::chop,      "Chop") };

    spaceKnobs = { &addKnob (params::reverb,   "Reverb"),
                   &addKnob (params::decay,    "Decay"),
                   &addKnob (params::delay,    "Delay"),
                   &addKnob (params::feedback, "Feedback"),
                   &addKnob (params::wash,     "Wash") };

    spaceKnobs.back()->slider.setTooltip ("Wash: how much of the dry sound disappears into the reverb and delay at full smile.");

    outputKnobs = { &addKnob (params::ride,   "Ride", true),
                    &addKnob (params::mix,    "Mix"),
                    &addKnob (params::output, "Output", true) };

    setSize (860, 590);
    startTimerHz (30);
}

RiseKitEditor::~RiseKitEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

RiseKitEditor::Knob& RiseKitEditor::addKnob (const char* paramId, const String& title, bool bipolar)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 72, 16);
    k->slider.getProperties().set ("bipolar", bipolar);
    k->label.setText (title, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    k->label.setFont (font (12.0f, true));
    k->label.setColour (Label::textColourId, colours::textDim);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);

    if (auto* param = proc.apvts.getParameter (paramId))
        k->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    knobs.push_back (std::move (k));
    return *knobs.back();
}

void RiseKitEditor::placeKnob (Knob& k, Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void RiseKitEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (colours::background.brighter (0.05f), 0.0f, 0.0f,
                                       colours::background, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Header
    g.setGradientFill (fireGradient ({ 16.0f, 12.0f, 160.0f, 32.0f }));
    g.setFont (font (28.0f, true));
    g.drawText ("RISEKIT", 18, 10, 200, 36, Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("build-up machine", 152, 22, 200, 16, Justification::centredLeft);

    drawPanel (g, dialPanel, "SMILE");
    drawPanel (g, risePanel, "AUTO RISE");
    drawPanel (g, characterPanel, "CHARACTER  (how much of each effect at full smile)");
    drawPanel (g, spacePanel, "SPACE  (reverb, delay and how far it washes out)");
    drawPanel (g, outputPanel, "OUTPUT");

    // Labels above the combo boxes
    g.setColour (colours::textDim);
    g.setFont (font (12.0f, true));
    auto labelAbove = [&g] (const Component& c, const String& text)
    {
        g.drawText (text, c.getX(), c.getY() - 18, c.getWidth(), 16, Justification::centredLeft);
    };
    labelAbove (lengthBox, "Length");
    labelAbove (reverbTypeBox, "Reverb type");
    labelAbove (delayTimeBox, "Delay time");

    // Rise progress bar
    const float progress = jlimit (0.0f, 1.0f, proc.riseProgress.load());
    auto bar = progressArea.toFloat();
    g.setColour (colours::track);
    g.fillRoundedRectangle (bar, 5.0f);
    if (progress > 0.0f)
    {
        g.setGradientFill (fireGradient (bar));
        g.fillRoundedRectangle (bar.withWidth (jmax (10.0f, bar.getWidth() * progress)), 5.0f);
    }

    // Bar ticks per bar of the chosen length
    const int lengthIndex = lengthBox.getSelectedId() - 1;
    if (isPositiveAndBelow (lengthIndex, (int) params::lengthBars.size()))
    {
        const int bars = params::lengthBars[(size_t) lengthIndex];
        if (bars > 1 && bars <= 16)
        {
            g.setColour (colours::background.withAlpha (0.6f));
            for (int i = 1; i < bars; ++i)
            {
                const float tx = bar.getX() + bar.getWidth() * (float) i / (float) bars;
                g.drawVerticalLine (roundToInt (tx), bar.getY(), bar.getBottom());
            }
        }
    }

    g.setColour (colours::textDim);
    g.setFont (font (11.5f));
    g.drawFittedText ("Automate RISE on where the build-up starts and off at the drop. "
                      "The smile then climbs by itself over the chosen length, in time with your project.",
                      risePanel.getX() + 16, progressArea.getBottom() + 8, risePanel.getWidth() - 32, 30,
                      Justification::topLeft, 2);
}

void RiseKitEditor::resized()
{
    dialPanel      = { 16, 60, 300, 270 };
    risePanel      = { 328, 60, 516, 270 };
    characterPanel = { 16, 342, 536, 112 };
    outputPanel    = { 564, 342, 280, 112 };
    spacePanel     = { 16, 466, 828, 112 };

    presetBox.setBounds (getWidth() - 16 - 240, 16, 240, 28);

    smileDial.setBounds (dialPanel.reduced (18).withTrimmedTop (14));

    // Auto rise
    const int rx = risePanel.getX() + 16, ry = risePanel.getY();
    riseButton.setBounds (rx, ry + 44, 170, 60);
    lengthBox.setBounds (rx + 190, ry + 62, 130, 28);
    placeKnob (*curveKnob, { rx + 350, ry + 30, 120, 96 });
    autoDropButton.setBounds (rx, ry + 140, 220, 24);
    safetyButton.setBounds (rx + 236, ry + 140, 160, 24);
    progressArea = { rx, ry + 190, risePanel.getWidth() - 32, 14 };

    auto placeRow = [this] (const std::vector<Knob*>& row, Rectangle<int> area)
    {
        area = area.withTrimmedTop (28).reduced (8, 4);
        const int w = area.getWidth() / (int) row.size();
        for (auto* k : row)
            placeKnob (*k, area.removeFromLeft (w));
    };
    placeRow (characterKnobs, characterPanel);
    placeRow (outputKnobs, outputPanel);

    // Space: two choice boxes + ping-pong on the left, knobs on the right
    const int sx = spacePanel.getX() + 14, sy = spacePanel.getY();
    reverbTypeBox.setBounds (sx, sy + 50, 140, 26);
    delayTimeBox.setBounds (sx + 154, sy + 50, 140, 26);
    pingPongButton.setBounds (sx + 154, sy + 82, 140, 22);
    placeRow (spaceKnobs, spacePanel.withTrimmedLeft (310));
}

void RiseKitEditor::timerCallback()
{
    const float live = proc.liveSmile.load();
    const float progress = proc.riseProgress.load();

    if (std::abs (live - shownLive) > 0.002f)
    {
        shownLive = live;
        smileDial.getProperties().set ("live", live);
        smileDial.repaint();
    }

    if (std::abs (progress - shownProgress) > 0.001f)
    {
        shownProgress = progress;
        repaint (progressArea.expanded (2));
    }

    const String riseText = riseButton.getToggleState() ? (progress >= 1.0f ? "AT THE TOP" : "RISING...") : "RISE";
    if (riseButton.getButtonText() != riseText)
        riseButton.setButtonText (riseText);

    const int program = proc.getCurrentProgram();
    if (program != shownProgram)
    {
        shownProgram = program;
        presetBox.setSelectedId (program + 1, dontSendNotification);
    }
}
