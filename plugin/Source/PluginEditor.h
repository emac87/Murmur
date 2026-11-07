#pragma once

#include <array>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Palette shared by the whole editor (matches the companion web app).
namespace MurmurColours
{
    inline const juce::Colour bgDark    { 0xff15181c };
    inline const juce::Colour panel     { 0xff1d2126 };
    inline const juce::Colour panelHi   { 0xff262b31 };
    inline const juce::Colour border    { 0xff2e353c };
    inline const juce::Colour lcdBg     { 0xff0e1a17 };
    inline const juce::Colour lcdBorder { 0xff1d3a31 };
    inline const juce::Colour teal      { 0xff5fe0b8 };
    inline const juce::Colour tealDim   { 0xff2f7360 };
    inline const juce::Colour tealFaint { 0xff16302a };
    inline const juce::Colour amber     { 0xffffb347 };
    inline const juce::Colour label     { 0xffe9e5da };
    inline const juce::Colour labelDim  { 0xff8c959d };
    inline const juce::Colour danger    { 0xffff7b72 };
    inline const juce::Colour ledGreen  { 0xff46d37a };
    inline const juce::Colour ledOff    { 0xff5a626a };
}

//==============================================================================
/** Dark rack-unit look: arc + pointer knobs, flat buttons, LED toggles. */
class MurmurLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MurmurLookAndFeel();

    /** UI scale relative to the 720 px wide design; set by the editor on resize. */
    void setScale (float newScale) noexcept   { scale = newScale; }
    float getScale() const noexcept           { return scale; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    float scale = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MurmurLookAndFeel)
};

//==============================================================================
/** Segmented toggle (radio group of TextButtons) bound to a choice parameter.
    Follows host automation through a juce::ParameterAttachment. */
class SegmentedChoice : public juce::Component
{
public:
    SegmentedChoice (juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& paramID,
                     const juce::StringArray& segmentLabels);
    ~SegmentedChoice() override;

    /** Called (message thread) whenever the selected segment changes, from the UI or the host. */
    std::function<void (int)> onChange;

    int getSelectedIndex() const noexcept   { return selected; }
    void resized() override;

private:
    void setSelectedVisual (int index);

    juce::OwnedArray<juce::TextButton> buttons;
    int selected = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;   // declared after buttons: destroyed first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedChoice)
};

//==============================================================================
/** Small "MIDI port: Murmur Out (dot)" chip. */
class StatusChip : public juce::Component
{
public:
    StatusChip() = default;
    void setState (bool open, const juce::String& portName);
    void paint (juce::Graphics&) override;

private:
    bool isOpen = false;
    juce::String name { "Murmur Out" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusChip)
};

//==============================================================================
/** The teal LCD: output note, tuner needle, level meter with gate marker, note LED. */
class LcdDisplay : public juce::Component
{
public:
    explicit LcdDisplay (MurmurProcessor&);

    /** Poll processor state; repaints this component only if something visible changed. */
    void tick();

    void paint (juce::Graphics&) override;

    static juce::String noteName (int midiNote);   // sharps, C4 = 60

private:
    MurmurProcessor& processor;
    std::atomic<float>* gateParam = nullptr;

    float midi = -1.0f, levelDb = -100.0f, levelShown = -100.0f, gateDb = -50.0f, led = 0.0f;
    int note = -1, velocity = 0;
    uint32_t lastCounter = 0;
    bool firstTick = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LcdDisplay)
};

//==============================================================================
class MurmurEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit MurmurEditor (MurmurProcessor&);
    ~MurmurEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label title;
        std::unique_ptr<SliderAttachment> attachment;   // last member: destroyed first
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAttachment> attachment;   // last member: destroyed first
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& paramID, const juce::String& titleText,
                    std::function<juce::String (double)> textFn);
    void setupToggle (Toggle&, const juce::String& paramID, const juce::String& text);
    void layoutKnob (Knob&, juce::Rectangle<int> area, float u);
    void updateVelocityEnablement();

    MurmurProcessor& processor;

    // Declared first so it is destroyed last (after every component using it).
    MurmurLookAndFeel lnf;

    LcdDisplay lcd;
    StatusChip chip;

    Knob gate, transpose, channel, fixedVel, bendRange;
    Toggle bendToggle, thruToggle, virtualToggle;
    juce::TextButton panicButton { "PANIC" };

    SegmentedChoice modeSelector;
    SegmentedChoice responseSelector;
    SegmentedChoice velModeSelector;

    std::array<juce::Rectangle<int>, 5> groupRects;
    juce::Rectangle<int> headerRect, footerRect;
    int modeIndex = 0;
    int slowTick = 0;
    std::atomic<float>* virtualPortParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MurmurEditor)
};
