#include "PluginEditor.h"

namespace
{
    using namespace MurmurColours;

    constexpr float designWidth  = 720.0f;
    constexpr float designHeight = 420.0f;

    juce::Font makeFont (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::String utf8 (const char* s)   { return juce::String (juce::CharPointer_UTF8 (s)); }

    constexpr float kLevelMinDb = -80.0f;
    constexpr float kLevelMaxDb = 0.0f;
}

//==============================================================================
// MurmurLookAndFeel
//==============================================================================
MurmurLookAndFeel::MurmurLookAndFeel()
{
    setColour (juce::Label::textColourId,               label);
    setColour (juce::Slider::textBoxTextColourId,       label);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  tealDim);
    setColour (juce::TextEditor::backgroundColourId,    lcdBg);
    setColour (juce::TextEditor::textColourId,          teal);
    setColour (juce::TextEditor::outlineColourId,       tealDim);
    setColour (juce::TextEditor::focusedOutlineColourId, teal);
    setColour (juce::CaretComponent::caretColourId,     teal);
}

juce::Font MurmurLookAndFeel::getTextButtonFont (juce::TextButton&, int)  { return makeFont (10.0f * scale, true); }
juce::Font MurmurLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getProperties().contains ("small") ? makeFont (9.5f * scale, true)
                                                : makeFont (11.0f * scale);
}

void MurmurLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f * scale;
    const bool enabled = slider.isEnabled();
    const auto accent  = enabled ? teal : labelDim.withAlpha (0.45f);

    // body
    const float body = radius * 0.70f;
    g.setGradientFill (juce::ColourGradient (panelHi, centre.x, centre.y - body,
                                             bgDark,  centre.x, centre.y + body, false));
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour (border);
    g.drawEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.0f);

    // track + value arc
    const float arcR  = radius - 2.0f * scale;
    const float thick = juce::jmax (2.0f, 3.0f * scale);
    const auto stroke = juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff0b0d10));
    g.strokePath (track, stroke);

    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    float fromAngle = startAngle;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)   // bipolar: arc grows from centre detent
        fromAngle = startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle);

    const float a0 = juce::jmin (fromAngle, angle), a1 = juce::jmax (fromAngle, angle);
    if (a1 - a0 > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (accent);
        g.strokePath (value, stroke);
    }

    // pointer
    const float dx = std::sin (angle), dy = -std::cos (angle);
    g.setColour (enabled ? label : labelDim.withAlpha (0.5f));
    g.drawLine (centre.x + dx * body * 0.30f, centre.y + dy * body * 0.30f,
                centre.x + dx * body * 0.92f, centre.y + dy * body * 0.92f,
                juce::jmax (1.5f, 2.0f * scale));
}

void MurmurLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                              bool highlighted, bool down)
{
    const auto area = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool segmented = b.isConnectedOnLeft() || b.isConnectedOnRight();
    const bool danger    = (bool) b.getProperties().getWithDefault ("danger", false);
    const bool on        = b.getToggleState();

    juce::Colour fill = juce::Colour (0xff23282e), edge = border;
    if (danger)       { fill = juce::Colour (0xff3a1c1f); edge = juce::Colour (0xff80353a); }
    else if (on)      { fill = tealFaint;                 edge = tealDim; }

    if (down)             fill = fill.brighter (0.25f);
    else if (highlighted) fill = fill.brighter (0.12f);
    if (! b.isEnabled())  fill = fill.withAlpha (0.5f);

    const float r = 4.0f * scale;
    juce::Path p;
    p.addRoundedRectangle (area.getX(), area.getY(), area.getWidth(), area.getHeight(), r, r,
                           ! (b.isConnectedOnLeft()  || b.isConnectedOnTop()),
                           ! (b.isConnectedOnRight() || b.isConnectedOnTop()),
                           ! (b.isConnectedOnLeft()  || b.isConnectedOnBottom()),
                           ! (b.isConnectedOnRight() || b.isConnectedOnBottom()));
    g.setColour (fill);
    g.fillPath (p);
    g.setColour (edge);
    g.strokePath (p, juce::PathStrokeType (1.0f));
    juce::ignoreUnused (segmented);
}

void MurmurLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool danger = (bool) b.getProperties().getWithDefault ("danger", false);
    auto c = danger ? MurmurColours::danger : (b.getToggleState() ? teal : labelDim);
    g.setColour (b.isEnabled() ? c : c.withAlpha (0.4f));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (1), juce::Justification::centred, false);
}

void MurmurLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    const auto area = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    auto fill = juce::Colour (0xff23282e);
    if (down) fill = fill.brighter (0.25f); else if (highlighted) fill = fill.brighter (0.12f);

    g.setColour (fill);
    g.fillRoundedRectangle (area, 4.0f * scale);
    g.setColour (on ? tealDim : border);
    g.drawRoundedRectangle (area, 4.0f * scale, 1.0f);

    const float d  = 7.0f * scale;
    const float lx = area.getX() + 9.0f * scale;
    const float ly = area.getCentreY();
    if (on)
    {
        g.setColour (teal.withAlpha (0.22f));
        g.fillEllipse (lx - d, ly - d, d * 2.0f, d * 2.0f);
        g.setColour (teal);
    }
    else
        g.setColour (juce::Colour (0xff2c3338));
    g.fillEllipse (lx - d * 0.5f, ly - d * 0.5f, d, d);
    if (! on)
    {
        g.setColour (ledOff.withAlpha (0.6f));
        g.drawEllipse (lx - d * 0.5f, ly - d * 0.5f, d, d, 1.0f);
    }

    g.setColour (on ? label : labelDim);
    g.setFont (makeFont (10.5f * scale, true));
    auto textArea = area.withTrimmedLeft (20.0f * scale).reduced (2.0f, 0.0f);
    g.drawText (b.getButtonText(), textArea, juce::Justification::centred, true);
}

//==============================================================================
// SegmentedChoice
//==============================================================================
SegmentedChoice::SegmentedChoice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                                  const juce::StringArray& labels)
{
    auto* param = apvts.getParameter (paramID);
    jassert (param != nullptr);

    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);   // radio group is per parent component
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                            | (i < labels.size() - 1 ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, i, param]
        {
            if (selected == i)
                return;
            if (auto* a = attachment.get())
                a->setValueAsCompleteGesture ((float) i);
            else
                jassertfalse;
            juce::ignoreUnused (param);
        };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*param,
        [this] (float v) { setSelectedVisual (juce::roundToInt (v)); });
    attachment->sendInitialUpdate();
}

SegmentedChoice::~SegmentedChoice()
{
    attachment.reset();
}

void SegmentedChoice::setSelectedVisual (int index)
{
    index = juce::jlimit (0, juce::jmax (0, buttons.size() - 1), index);
    const bool changed = (index != selected);
    selected = index;

    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);

    if (changed && onChange)
        onChange (index);
}

void SegmentedChoice::resized()
{
    auto r = getLocalBounds();
    const int n = buttons.size();
    for (int i = 0; i < n; ++i)
    {
        const int x0 = r.getX() + r.getWidth() * i / n;
        const int x1 = r.getX() + r.getWidth() * (i + 1) / n;
        buttons[i]->setBounds (x0, r.getY(), x1 - x0 + (i < n - 1 ? 1 : 0), r.getHeight());
    }
}

//==============================================================================
// StatusChip
//==============================================================================
void StatusChip::setState (bool open, const juce::String& portName)
{
    const auto n = portName.isEmpty() ? juce::String ("Murmur Out") : portName;
    if (open != isOpen || n != name)
    {
        isOpen = open;
        name = n;
        repaint();
    }
}

void StatusChip::paint (juce::Graphics& g)
{
    const float u = (float) getWidth() / 196.0f;
    auto b = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (lcdBg);
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (isOpen ? tealDim : border);
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);

    const float d = 8.0f * u;
    const float cx = b.getRight() - 12.0f * u - d * 0.5f, cy = b.getCentreY();
    if (isOpen)
    {
        g.setColour (ledGreen.withAlpha (0.25f));
        g.fillEllipse (cx - d, cy - d, d * 2.0f, d * 2.0f);
    }
    g.setColour (isOpen ? ledGreen : ledOff);
    g.fillEllipse (cx - d * 0.5f, cy - d * 0.5f, d, d);

    g.setColour (isOpen ? teal : labelDim);
    g.setFont (makeFont (10.5f * u));
    g.drawText ("MIDI port: " + name,
                b.withTrimmedLeft (11.0f * u).withTrimmedRight (26.0f * u),
                juce::Justification::centredLeft, true);
}

//==============================================================================
// LcdDisplay
//==============================================================================
LcdDisplay::LcdDisplay (MurmurProcessor& p) : processor (p)
{
    gateParam = processor.apvts.getRawParameterValue ("gate");
    setOpaque (false);
}

juce::String LcdDisplay::noteName (int n)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (n < 0)
        return utf8 ("\xe2\x80\x94");
    return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}

void LcdDisplay::tick()
{
    const float newMidi  = processor.uiMidi.load (std::memory_order_relaxed);
    const float newLevel = processor.uiLevelDb.load (std::memory_order_relaxed);
    const int   newNote  = processor.uiNote.load (std::memory_order_relaxed);
    const int   newVel   = processor.uiVelocity.load (std::memory_order_relaxed);
    const auto  counter  = processor.uiNoteCounter.load (std::memory_order_relaxed);
    const float newGate  = gateParam != nullptr ? gateParam->load (std::memory_order_relaxed) : -50.0f;

    // meter ballistics: instant attack, ~45 dB/s release
    float shown = levelShown;
    const float lv = juce::jlimit (kLevelMinDb, kLevelMaxDb, newLevel);
    shown = (lv >= shown) ? lv : juce::jmax (lv, shown - 1.5f);

    float newLed = led;
    if (firstTick)                  lastCounter = counter;
    if (counter != lastCounter)   { newLed = 1.0f; lastCounter = counter; }
    else                            newLed = juce::jmax (0.0f, newLed - 0.12f);

    bool dirty = firstTick
              || std::abs (shown - levelShown) > 0.05f
              || std::abs (newLed - led) > 0.001f
              || newNote != note || newVel != velocity
              || newGate != gateDb
              || (newMidi < 0.0f) != (midi < 0.0f)
              || (newMidi >= 0.0f && std::abs (newMidi - midi) > 0.004f);

    midi = newMidi; levelDb = newLevel; levelShown = shown; led = newLed;
    note = newNote; velocity = newVel; gateDb = newGate;
    firstTick = false;

    if (dirty)
        repaint();   // this child only: just the LCD region
}

void LcdDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float u = b.getWidth() / 696.0f;
    const auto bounds = b.reduced (0.5f);

    // glass
    g.setColour (lcdBg);
    g.fillRoundedRectangle (bounds, 7.0f * u);
    g.setColour (juce::Colours::black.withAlpha (0.14f));
    for (float yy = bounds.getY() + 2.0f; yy < bounds.getBottom() - 1.0f; yy += juce::jmax (2.0f, 3.0f * u))
        g.fillRect (bounds.getX() + 1.0f, yy, bounds.getWidth() - 2.0f, 1.0f);
    g.setColour (lcdBorder);
    g.drawRoundedRectangle (bounds, 7.0f * u, 1.0f);

    auto inner = bounds.reduced (14.0f * u, 12.0f * u);
    auto left  = inner.removeFromLeft (inner.getWidth() * 0.36f);
    inner.removeFromLeft (16.0f * u);
    auto right = inner;

    g.setColour (lcdBorder);
    g.fillRect (right.getX() - 8.0f * u, bounds.getY() + 12.0f * u, 1.0f, bounds.getHeight() - 24.0f * u);

    const bool sounding = note >= 0;

    // ---- left: output note + LED ------------------------------------------------
    {
        g.setColour (tealDim);
        g.setFont (makeFont (9.5f * u, true));
        g.drawText ("OUTPUT NOTE", left.removeFromTop (14.0f * u), juce::Justification::topLeft, false);

        const float ledR = 5.5f * u;
        const float lcx = left.getRight() - ledR - 2.0f * u, lcy = bounds.getY() + 12.0f * u + 6.0f * u;
        const float a = 0.18f + 0.82f * led;
        g.setColour (amber.withAlpha (0.28f * led));
        g.fillEllipse (lcx - ledR * 2.0f, lcy - ledR * 2.0f, ledR * 4.0f, ledR * 4.0f);
        g.setColour (juce::Colour (0xff2b2214).interpolatedWith (amber, a));
        g.fillEllipse (lcx - ledR, lcy - ledR, ledR * 2.0f, ledR * 2.0f);
        g.setColour (lcdBorder);
        g.drawEllipse (lcx - ledR, lcy - ledR, ledR * 2.0f, ledR * 2.0f, 1.0f);

        auto velArea = left.removeFromBottom (16.0f * u);
        g.setColour (sounding ? teal : tealDim);
        g.setFont (makeFont (11.0f * u, true));
        g.drawText (sounding ? "VEL " + juce::String (velocity) : juce::String ("VEL ") + utf8 ("\xe2\x80\x94"),
                    velArea, juce::Justification::centred, false);

        const float fs = juce::jmin (left.getHeight() * 0.95f, left.getWidth() * 0.42f);
        g.setFont (makeFont (fs, true));
        if (sounding)
        {
            g.setColour (amber.withAlpha (0.16f));
            g.drawText (noteName (note), left.translated (0.0f, 1.0f * u).expanded (1.0f * u), juce::Justification::centred, false);
        }
        g.setColour (sounding ? amber : tealDim.withAlpha (0.7f));
        g.drawText (noteName (note), left, juce::Justification::centred, false);
    }

    // ---- right: detected pitch / tuner / level --------------------------------------
    const float rowH = right.getHeight();
    auto pitchRow = right.removeFromTop (rowH * 0.27f);
    auto tunerRow = right.removeFromTop (rowH * 0.33f);
    auto levelRow = right;

    // detected pitch + cents
    {
        const bool voiced = midi >= 0.0f;
        const int nearest = voiced ? juce::roundToInt (midi) : -1;
        const float cents = voiced ? (midi - (float) nearest) * 100.0f : 0.0f;

        g.setColour (tealDim);
        g.setFont (makeFont (9.5f * u, true));
        g.drawText ("DETECTED", pitchRow.removeFromLeft (70.0f * u), juce::Justification::centredLeft, false);

        juce::String text = voiced ? noteName (nearest) + "   " + (cents >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (cents)) + " ct"
                                   : utf8 ("\xe2\x80\x94");
        g.setColour (voiced ? teal : tealDim.withAlpha (0.7f));
        g.setFont (makeFont (19.0f * u, true));
        g.drawText (text, pitchRow, juce::Justification::centredLeft, false);
    }

    // tuner
    {
        const bool voiced = midi >= 0.0f;
        const float cents = voiced ? juce::jlimit (-50.0f, 50.0f, (midi - (float) juce::roundToInt (midi)) * 100.0f) : 0.0f;

        auto t = tunerRow.reduced (4.0f * u, 0.0f);
        auto labels = t.removeFromBottom (11.0f * u);
        const float x0 = t.getX(), w = t.getWidth();
        auto xOf = [&] (float c) { return x0 + (c + 50.0f) / 100.0f * w; };
        const float baseY = t.getBottom() - 1.0f * u;

        g.setColour (tealDim);
        g.fillRect (x0, baseY, w, 1.0f);
        for (int c = -50; c <= 50; c += 10)
        {
            const float th = (c == 0) ? t.getHeight() * 0.75f : (c % 50 == 0 ? t.getHeight() * 0.55f : t.getHeight() * 0.35f);
            g.setColour (c == 0 ? teal.withAlpha (0.8f) : tealDim);
            g.fillRect (xOf ((float) c) - 0.5f, baseY - th, 1.0f, th);
        }

        g.setColour (tealDim);
        g.setFont (makeFont (8.5f * u));
        g.drawText ("-50", juce::Rectangle<float> (xOf (-50.0f) - 15.0f * u, labels.getY(), 30.0f * u, labels.getHeight()), juce::Justification::centred, false);
        g.drawText ("0",   juce::Rectangle<float> (xOf (0.0f)   - 15.0f * u, labels.getY(), 30.0f * u, labels.getHeight()), juce::Justification::centred, false);
        g.drawText ("+50", juce::Rectangle<float> (xOf (50.0f)  - 15.0f * u, labels.getY(), 30.0f * u, labels.getHeight()), juce::Justification::centred, false);

        if (voiced)
        {
            const bool inTune = std::abs (cents) <= 5.0f;
            const auto c = inTune ? juce::Colour (0xffb9ffe8) : teal;
            const float nx = xOf (cents);
            g.setColour (c.withAlpha (0.2f));
            g.fillRect (nx - 3.0f * u, t.getY(), 6.0f * u, baseY - t.getY());
            g.setColour (c);
            g.fillRect (nx - 1.0f * u, t.getY(), 2.0f * u, baseY - t.getY());
            juce::Path tri;
            tri.addTriangle (nx - 4.0f * u, t.getY(), nx + 4.0f * u, t.getY(), nx, t.getY() + 6.0f * u);
            g.fillPath (tri);
        }
    }

    // level meter with gate marker
    {
        auto lr = levelRow.reduced (4.0f * u, 0.0f);
        auto scaleRow = lr.removeFromBottom (11.0f * u);
        auto gateRow  = lr.removeFromTop (11.0f * u);
        auto meter    = lr.reduced (0.0f, 2.0f * u);

        auto dbToX = [&] (float db)
        {
            return meter.getX() + juce::jlimit (0.0f, 1.0f, (db - kLevelMinDb) / (kLevelMaxDb - kLevelMinDb)) * meter.getWidth();
        };

        const int segs = 64;
        const float segW = meter.getWidth() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float db = kLevelMinDb + ((float) i + 0.5f) / (float) segs * (kLevelMaxDb - kLevelMinDb);
            const bool lit = db <= levelShown;
            juce::Colour c = db > -3.0f ? danger : (db > -12.0f ? amber : teal);
            if (db < gateDb) c = c.withMultipliedAlpha (0.45f);   // below gate: not passing
            g.setColour (lit ? c : tealFaint);
            g.fillRect (meter.getX() + (float) i * segW + 0.5f, meter.getY(), juce::jmax (1.0f, segW - 1.0f), meter.getHeight());
        }

        // gate marker
        const float gx = dbToX (gateDb);
        g.setColour (label);
        g.fillRect (gx - 0.75f * u, meter.getY() - 3.0f * u, 1.5f * u, meter.getHeight() + 6.0f * u);
        juce::Path tri;
        tri.addTriangle (gx - 3.5f * u, gateRow.getBottom() - 4.0f * u, gx + 3.5f * u, gateRow.getBottom() - 4.0f * u,
                         gx, gateRow.getBottom() + 0.5f * u);
        g.fillPath (tri);
        g.setFont (makeFont (8.5f * u, true));
        const float tw = 70.0f * u;
        g.drawText ("GATE " + juce::String (juce::roundToInt (gateDb)),
                    juce::Rectangle<float> (juce::jlimit (meter.getX(), meter.getRight() - tw, gx - tw * 0.5f),
                                            gateRow.getY(), tw, gateRow.getHeight() - 3.0f * u),
                    juce::Justification::centred, false);

        g.setColour (tealDim);
        g.setFont (makeFont (8.5f * u));
        for (int db = -80; db <= 0; db += 20)
            g.drawText (juce::String (db), juce::Rectangle<float> (dbToX ((float) db) - 15.0f * u, scaleRow.getY(), 30.0f * u, scaleRow.getHeight()),
                        db == -80 ? juce::Justification::centredLeft : (db == 0 ? juce::Justification::centredRight : juce::Justification::centred), false);
    }
}

//==============================================================================
// MurmurEditor
//==============================================================================
MurmurEditor::MurmurEditor (MurmurProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor (p),
      lcd (p),
      modeSelector     (p.apvts, "mode",     { "Voice", "Guitar", "Bass" }),
      responseSelector (p.apvts, "response", { "Fast", "Balanced", "Stable" }),
      velModeSelector  (p.apvts, "velMode",  { "Dynamic", "Fixed" })
{
    setLookAndFeel (&lnf);
    virtualPortParam = processor.apvts.getRawParameterValue ("virtualPort");

    addAndMakeVisible (lcd);
    addAndMakeVisible (chip);

    setupKnob (gate,      "gate",      "GATE",      [] (double v) { return juce::String (juce::roundToInt (v)) + " dB"; });
    setupKnob (transpose, "transpose", "TRANSPOSE", [] (double v) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " st"; });
    setupKnob (channel,   "channel",   "CHANNEL",   [] (double v) { return juce::String (juce::roundToInt (v)); });
    setupKnob (fixedVel,  "fixedVel",  "FIXED VEL", [] (double v) { return juce::String (juce::roundToInt (v)); });
    setupKnob (bendRange, "bendRange", "RANGE",     [] (double v) { return utf8 ("\xc2\xb1") + juce::String (juce::roundToInt (v)) + " st"; });

    setupToggle (bendToggle,    "bend",        "Pitch bend");
    setupToggle (thruToggle,    "thru",        "Audio thru");
    setupToggle (virtualToggle, "virtualPort", "Virtual port");

    panicButton.getProperties().set ("danger", true);
    panicButton.onClick = [this] { processor.panic(); };
    addAndMakeVisible (panicButton);

    addAndMakeVisible (modeSelector);
    addAndMakeVisible (responseSelector);
    addAndMakeVisible (velModeSelector);

    modeSelector.onChange = [this] (int i)
    {
        modeIndex = i;
        repaint (footerRect);
    };
    velModeSelector.onChange = [this] (int) { updateVelocityEnablement(); };
    modeSelector.onChange (modeSelector.getSelectedIndex());
    updateVelocityEnablement();

    setResizeLimits (540, 315, 1440, 840);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setResizable (true, true);
    setSize (720, 420);

    startTimerHz (30);
}

MurmurEditor::~MurmurEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MurmurEditor::setupKnob (Knob& k, const juce::String& paramID, const juce::String& titleText,
                              std::function<juce::String (double)> textFn)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 15);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setScrollWheelEnabled (false);
    addAndMakeVisible (k.slider);

    k.title.setText (titleText, juce::dontSendNotification);
    k.title.setJustificationType (juce::Justification::centred);
    k.title.setColour (juce::Label::textColourId, labelDim);
    k.title.getProperties().set ("small", true);
    k.title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.title);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, k.slider);

    // our own value formatting, applied after the attachment installs its defaults
    k.slider.textFromValueFunction = std::move (textFn);
    if (auto* prm = processor.apvts.getParameter (paramID))
        k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    k.slider.updateText();
}

void MurmurEditor::setupToggle (Toggle& t, const juce::String& paramID, const juce::String& text)
{
    t.button.setButtonText (text);
    t.button.setClickingTogglesState (true);
    addAndMakeVisible (t.button);
    t.attachment = std::make_unique<ButtonAttachment> (processor.apvts, paramID, t.button);
}

void MurmurEditor::updateVelocityEnablement()
{
    const bool fixed = velModeSelector.getSelectedIndex() == 1;
    fixedVel.slider.setEnabled (fixed);
    fixedVel.title.setAlpha (fixed ? 1.0f : 0.4f);
    fixedVel.slider.setAlpha (fixed ? 1.0f : 0.6f);
}

void MurmurEditor::timerCallback()
{
    lcd.tick();

    if (slowTick++ % 10 == 0)
    {
        const bool wanted = virtualPortParam != nullptr && virtualPortParam->load() > 0.5f;
        chip.setState (wanted && processor.isVirtualPortOpen(), processor.getVirtualPortName());
    }
}

void MurmurEditor::paint (juce::Graphics& g)
{
    const float u = (float) getWidth() / designWidth;

    g.fillAll (bgDark);

    // header strip
    g.setColour (panel);
    g.fillRect (headerRect);
    g.setColour (border);
    g.fillRect (headerRect.getX(), headerRect.getBottom() - 1, headerRect.getWidth(), 1);

    // wordmark + subtitle
    {
        auto h = headerRect.reduced ((int) (16 * u), 0);
        g.setColour (label);
        g.setFont (juce::Font (juce::FontOptions (22.0f * u, juce::Font::bold).withKerningFactor (0.18f)));
        auto wm = h.removeFromLeft ((int) (118 * u));
        g.drawText ("MURMUR", wm, juce::Justification::centredLeft, false);
        g.setColour (teal);
        g.setFont (makeFont (11.5f * u));
        g.drawText ("audio " + utf8 ("\xe2\x86\x92") + " MIDI", h.removeFromLeft ((int) (110 * u)).withTrimmedTop ((int) (5 * u)),
                    juce::Justification::centredLeft, false);
    }

    // rack screws
    for (int sx : { 7, getWidth() - 7 })
        for (int sy : { 7, getHeight() - 7 })
        {
            const float r = 2.6f * u;
            g.setColour (juce::Colour (0xff0b0d10));
            g.fillEllipse ((float) sx - r, (float) sy - r, r * 2.0f, r * 2.0f);
            g.setColour (border);
            g.drawEllipse ((float) sx - r, (float) sy - r, r * 2.0f, r * 2.0f, 1.0f);
            g.drawLine ((float) sx - r * 0.6f, (float) sy + r * 0.6f, (float) sx + r * 0.6f, (float) sy - r * 0.6f, 1.0f);
        }

    // control groups
    static const char* titles[] = { "INPUT", "MIDI OUT", "VELOCITY", "PITCH BEND", "SYSTEM" };
    for (size_t i = 0; i < groupRects.size(); ++i)
    {
        auto r = groupRects[i].toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (r, 6.0f * u);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f * u, 1.0f);
        g.setColour (teal.withAlpha (0.55f));
        g.fillRect (r.getX() + 10.0f * u, r.getY() + 8.0f * u, 3.0f * u, 8.0f * u);
        g.setColour (labelDim);
        g.setFont (makeFont (9.5f * u, true));
        g.drawText (titles[i], r.withTrimmedLeft (18.0f * u).removeFromTop (24.0f * u), juce::Justification::centredLeft, false);
    }

    // footer hint
    static const char* hints[] = { "Sing one note at a time",
                                   "Single notes \xe2\x80\x94 mute strings you aren't playing",
                                   "Pluck clearly; one string at a time" };
    g.setColour (labelDim);
    g.setFont (makeFont (11.5f * u));
    g.drawText (utf8 (hints[juce::jlimit (0, 2, modeIndex)]), footerRect.reduced ((int) (6 * u), 0),
                juce::Justification::centredLeft, true);
}

void MurmurEditor::layoutKnob (Knob& k, juce::Rectangle<int> area, float u)
{
    k.title.setBounds (area.removeFromTop ((int) (13 * u)));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, area.getWidth(), (int) (16 * u));
    k.slider.setBounds (area);
}

void MurmurEditor::resized()
{
    const float u = (float) getWidth() / designWidth;
    lnf.setScale (u);
    auto S = [u] (float v) { return juce::roundToInt (v * u); };

    auto bounds = getLocalBounds();
    headerRect = bounds.removeFromTop (S (52));
    footerRect = bounds.removeFromBottom (S (30));
    bounds = bounds.reduced (S (12), 0);

    // header widgets
    {
        auto h = headerRect.reduced (S (14), S (11));
        chip.setBounds (h.removeFromRight (S (196)));
        modeSelector.setBounds (juce::Rectangle<int> (0, 0, S (210), h.getHeight()).withCentre (
            { S (720 * 0.5f - 18), headerRect.getCentreY() }));
    }

    bounds.removeFromTop (S (8));
    lcd.setBounds (bounds.removeFromTop (S (156)));
    bounds.removeFromTop (S (8));

    // control panel
    auto row = bounds.removeFromTop (S (166));
    const int gap = S (6);
    const float widths[] = { 166, 136, 140, 128, 104 };
    for (size_t i = 0; i < groupRects.size(); ++i)
    {
        groupRects[i] = row.removeFromLeft (i + 1 < groupRects.size() ? S (widths[i]) : row.getWidth());
        row.removeFromLeft (gap);
    }

    auto content = [&] (int i)
    {
        auto r = groupRects[(size_t) i].reduced (S (10), 0);
        r.removeFromTop (S (24));
        return r;
    };
    const int knobH = S (100), ctlH = S (24), ctlBottom = S (10);

    auto knobArea = [&] (juce::Rectangle<int> c) { return c.removeFromTop (knobH); };
    auto ctlArea  = [&] (juce::Rectangle<int> c)
    {
        return c.removeFromBottom (ctlBottom + ctlH).removeFromTop (ctlH);
    };

    {   // INPUT
        auto c = content (0);
        layoutKnob (gate, knobArea (c).withSizeKeepingCentre (S (66), knobH), u);
        responseSelector.setBounds (ctlArea (c));
    }
    {   // MIDI OUT
        auto c = content (1);
        auto k = knobArea (c);
        auto l = k.removeFromLeft (k.getWidth() / 2);
        layoutKnob (transpose, l, u);
        layoutKnob (channel,   k, u);
        virtualToggle.button.setBounds (ctlArea (c));
    }
    {   // VELOCITY
        auto c = content (2);
        layoutKnob (fixedVel, knobArea (c).withSizeKeepingCentre (S (66), knobH), u);
        velModeSelector.setBounds (ctlArea (c));
    }
    {   // PITCH BEND
        auto c = content (3);
        layoutKnob (bendRange, knobArea (c).withSizeKeepingCentre (S (66), knobH), u);
        bendToggle.button.setBounds (ctlArea (c));
    }
    {   // SYSTEM
        auto c = content (4);
        panicButton.setBounds (knobArea (c).withSizeKeepingCentre (c.getWidth(), S (38)));
        thruToggle.button.setBounds (ctlArea (c));
    }
}
