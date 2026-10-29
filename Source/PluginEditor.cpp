/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).
*/
#include "PluginEditor.h"
#include "SpaceArt.h"

using namespace brewui;

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::headerTextColourId, Colours::amber);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::bg);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::bg);
    setColour (juce::ToggleButton::textColourId, Colours::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    const bool big = (bool) s.getProperties()["big"];
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (big ? 12.0f : 7.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float lineW = big ? 5.0f : juce::jmax (2.5f, radius * 0.11f);
    const float arcR = radius - lineW * 0.5f;

    // tick marks
    const int ticks = big ? 31 : 11;
    g.setColour (Colours::dim.withAlpha (0.55f));
    for (int i = 0; i < ticks; ++i)
    {
        const float a = start + (end - start) * (float) i / (float) (ticks - 1);
        const auto p = centre.getPointOnCircumference (radius + (big ? 9.0f : 5.0f), a);
        const float d = big && i % 5 == 0 ? 3.0f : 2.0f;
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (Colours::track);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const float from = bipolar ? start + (float) s.getProperties()["zeroPos"] * (end - start) : start;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setGradientFill (juce::ColourGradient (Colours::copper, centre.x - radius, centre.y + radius,
                                             Colours::amber, centre.x + radius, centre.y - radius, false));
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body with a soft top light
    const float knobR = arcR - lineW * (big ? 2.2f : 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (centre.x - knobR, centre.y - knobR + (big ? 6.0f : 3.0f), knobR * 2.0f, knobR * 2.0f);
    g.setGradientFill (juce::ColourGradient (Colours::panelEdge.brighter (s.isMouseOverOrDragging() ? 0.3f : 0.15f), centre.x, centre.y - knobR,
                                             Colours::panel.darker (0.3f), centre.x, centre.y + knobR, false));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (Colours::panelEdge.brighter (0.25f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    const auto dot = centre.getPointOnCircumference (knobR * 0.78f, angle);
    const float dr = big ? 4.0f : 2.6f;
    g.setColour (Colours::amber);
    g.fillEllipse (dot.x - dr, dot.y - dr, dr * 2.0f, dr * 2.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);

    if ((bool) b.getProperties()["tab"])
    {
        if (b.getToggleState())
        {
            g.setColour (Colours::text.withAlpha (0.9f));
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);
        }
        else if (over)
        {
            g.setColour (Colours::panelHi);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        }
        return;
    }

    if ((bool) b.getProperties()["overlay"])
    {
        g.setColour (juce::Colours::black.withAlpha (over ? 0.65f : 0.45f));
        g.fillRoundedRectangle (r, 5.0f);
        return;
    }

    const bool on = b.getToggleState();
    const bool stored = (bool) b.getProperties()["stored"];
    g.setColour (down ? Colours::amber.darker (0.3f) : on ? Colours::amber
                 : stored ? Colours::panelEdge.brighter (0.25f) : over ? Colours::panelEdge.brighter (0.2f)
                 : (bool) b.getProperties()["snap"] ? Colours::panel : Colours::panelEdge);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::amber.withAlpha (over || on ? 0.9f : 0.45f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool tab = (bool) b.getProperties()["tab"];
    const bool on = b.getToggleState();

    g.setFont (juce::FontOptions ((float) b.getProperties().getWithDefault ("fontSize", 13.0f), juce::Font::bold));
    g.setColour (tab ? (on ? Colours::text : Colours::dim)
                     : (on && ! (bool) b.getProperties()["overlay"] ? Colours::bg : Colours::text));

    if ((bool) b.getProperties()["snap"])
    {
        const bool stored = (bool) b.getProperties()["stored"];
        g.setColour (on ? Colours::bg : stored ? Colours::text : Colours::dim.withAlpha (0.5f));
        g.drawText (b.getButtonText(), r.withTrimmedBottom (14.0f), juce::Justification::centred);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (stored ? (on ? "ACTIVE" : "STORED") : "EMPTY", r.withTrimmedTop (r.getHeight() - 24.0f), juce::Justification::centred);
        return;
    }

    if (tab && (bool) b.getProperties()["dot"])
    {
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), b.getButtonText());
        const float x = r.getCentreX() - tw * 0.5f - 12.0f;
        g.setColour (Colours::amber);
        g.fillEllipse (x, r.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (on ? Colours::text : Colours::dim);
    }
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (over ? Colours::panelEdge.brighter (0.2f) : Colours::panelEdge);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colours::dim.withAlpha (0.5f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    const float d = 8.0f;
    g.setColour (on ? Colours::amber : Colours::track);
    g.fillEllipse (r.getX() + 14.0f, r.getCentreY() - d * 0.5f, d, d);
    if (on)
    {
        g.setColour (Colours::amber.withAlpha (0.25f));
        g.fillEllipse (r.getX() + 10.0f, r.getCentreY() - d, d * 2.0f, d * 2.0f);
    }
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (22.0f), juce::Justification::centred);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text, bool isBig)
    : attachment (s, id, slider), big (isBig)
{
    slider.getProperties().set ("big", isBig);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 18);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::dim);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    if (auto* p = s.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        const auto range = p->getNormalisableRange();
        slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
        slider.getProperties().set ("zeroPos", range.start < 0.0f && range.end > 0.0f ? range.convertTo0to1 (0.0f) : 0.0f);
    }
    addAndMakeVisible (slider);

    label.setText (text.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (isBig ? 14.0f : 11.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, isBig ? Colours::text : Colours::text.withAlpha (0.8f));
    addAndMakeVisible (label);
}

void Knob::resized()
{
    // knob, then its value, then the label underneath
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (big ? 22 : 16));
    slider.setBounds (r);
}

//==============================================================================
SpaceView::SpaceView (BrewVerbProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next, &load, &builtIn })
    {
        b->getProperties().set ("overlay", true);
        addAndMakeVisible (b);
    }
    prev.getProperties().set ("fontSize", 18.0f);
    next.getProperties().set ("fontSize", 18.0f);
    prev.setTooltip ("Previous space");
    next.setTooltip ("Next space");

    auto step = [this] (int delta)
    {
        const int n = brew::getNumSpaces();
        if (onSelectSpace) onSelectSpace ((proc.getCurrentSpace() + delta + n) % n);
    };
    prev.onClick = [step] { step (-1); };
    next.onClick = [step] { step (1); };
    load.onClick = [this] { if (onLoadIR) onLoadIR(); };
    builtIn.onClick = [this] { if (onBuiltIn) onBuiltIn(); };

    display = proc.getDisplay();
}

void SpaceView::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (78);
    nameArea = bottom.reduced (60, 8).withTrimmedBottom (22);
    prev.setBounds (bottom.getX() + 14, bottom.getY() + 14, 34, 34);
    next.setBounds (bottom.getRight() - 48, bottom.getY() + 14, 34, 34);
    builtIn.setBounds (getWidth() - 96, 12, 84, 26);
    load.setBounds (getWidth() - 186, 12, 84, 26);
}

void SpaceView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    juce::Path clip;
    clip.addRoundedRectangle (bounds, 14.0f);

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);

        const bool user = proc.hasUserIR();
        const auto info = brew::getSpaceInfo (proc.getCurrentSpace());
        const float size = proc.apvts.getRawParameterValue (ParamID::size)->load() * 0.01f;
        const float posX = proc.apvts.getRawParameterValue (ParamID::posx)->load() * 0.01f;
        const float dist = proc.apvts.getRawParameterValue (ParamID::posdist)->load() * 0.01f;

        if (! user)
        {
            drawSpaceArt (g, bounds, info, size, posX, dist);
        }
        else
        {
            // user IR: show its decay as the "picture"
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff221d17), bounds.getCentre(),
                                                     juce::Colour (0xff0e0c0a), bounds.getTopLeft(), true));
            g.fillRect (bounds);
            auto plot = bounds.reduced (30.0f, 60.0f).withTrimmedBottom (20.0f);
            const float midY = plot.getCentreY(), half = plot.getHeight() * 0.5f;
            for (int pass = 0; pass < 2; ++pass)
            {
                const auto& env = pass == 0 ? display.envL : display.envR;
                if (env.empty()) continue;
                juce::Path p;
                p.startNewSubPath (plot.getX(), midY);
                for (size_t i = 0; i < env.size(); ++i)
                {
                    const float x = plot.getX() + plot.getWidth() * (float) i / (float) (env.size() - 1);
                    const float off = juce::jlimit (0.0f, 1.0f, (env[i] + 72.0f) / 72.0f) * half;
                    p.lineTo (x, pass == 0 ? midY - off : midY + off);
                }
                p.lineTo (plot.getRight(), midY);
                p.closeSubPath();
                const auto c = pass == 0 ? Colours::amber : Colours::copper;
                g.setGradientFill (juce::ColourGradient (c.withAlpha (0.9f), plot.getX(), midY, c.withAlpha (0.1f), plot.getRight(), midY, false));
                g.fillPath (p);
            }
        }

        // bottom caption bar
        auto cap = bounds.withTop (bounds.getBottom() - 92.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), cap.getX(), cap.getY(),
                                                 juce::Colours::black.withAlpha (0.85f), cap.getX(), cap.getBottom(), false));
        g.fillRect (cap);

        const juce::String name = user ? display.name : info.name;
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
        g.drawFittedText (name, nameArea.withTrimmedBottom (2), juce::Justification::centred, 1);

        juce::String sub;
        if (user)
            sub = "User impulse response  /  RT60 " + juce::String (display.rt60, 2) + " s";
        else
        {
            const float sz = size;
            auto m = [sz] (float v) { return juce::String (v * sz, v * sz < 10.0f ? 1 : 0); };
            sub = info.category + "  /  " + (info.heightM > 0.0f ? m (info.widthM) + " x " + m (info.depthM) + " x " + m (info.heightM) + " m"
                                                                  : m (info.widthM) + " x " + m (info.depthM) + " m")
                + "  (" + juce::String (display.rt60, 1) + " s)";
        }
        g.setColour (Colours::text.withAlpha (0.75f));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (sub, nameArea.withY (nameArea.getBottom() - 2).withHeight (20), juce::Justification::centred);

        g.setColour (Colours::text.withAlpha (0.45f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("1.1", bounds.withTop (bounds.getBottom() - 22.0f).withTrimmedLeft (bounds.getWidth() - 40.0f), juce::Justification::centred);

        if (dragOver)
        {
            g.setColour (Colours::bg.withAlpha (0.75f));
            g.fillRect (bounds);
            juce::Path outline, dashed;
            outline.addRoundedRectangle (bounds.reduced (8.0f), 10.0f);
            const float dashes[] = { 8.0f, 6.0f };
            juce::PathStrokeType (2.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (Colours::amber);
            g.fillPath (dashed);
            g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
            g.drawText ("Drop impulse response to load", bounds, juce::Justification::centred);
        }
    }

    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (bounds.reduced (1.0f), 14.0f, 3.0f);
    g.setColour (Colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 14.0f, 1.0f);
}

void SpaceView::mouseUp (const juce::MouseEvent& e)
{
    if (! nameArea.contains (e.getPosition()))
        return;

    // Browse menu grouped by category
    juce::PopupMenu menu;
    juce::StringArray cats;
    for (int i = 0; i < brew::getNumSpaces(); ++i)
        cats.addIfNotAlreadyThere (brew::getSpaceInfo (i).category);

    for (auto& c : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < brew::getNumSpaces(); ++i)
        {
            const auto info = brew::getSpaceInfo (i);
            if (info.category == c)
                sub.addItem (i + 1, info.name + "   (" + juce::String (info.rt60, 1) + " s)", true,
                             i == proc.getCurrentSpace() && ! proc.hasUserIR());
        }
        menu.addSubMenu (c, sub);
    }
    menu.addSeparator();
    menu.addItem (1000, "Load impulse response file...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (nameArea)),
                        [this] (int result)
                        {
                            if (result == 1000) { if (onLoadIR) onLoadIR(); }
                            else if (result > 0 && onSelectSpace) onSelectSpace (result - 1);
                        });
}

//==============================================================================
StagePad::StagePad (BrewVerbProcessor& p)
    : xAttach (*p.apvts.getParameter (ParamID::posx), [this] (float v) { x = v; repaint(); }),
      dAttach (*p.apvts.getParameter (ParamID::posdist), [this] (float v) { d = v; repaint(); })
{
    xAttach.sendInitialUpdate();
    dAttach.sendInitialUpdate();
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<float> StagePad::stageArea() const
{
    return getLocalBounds().toFloat().reduced (26.0f, 22.0f).withTrimmedBottom (16.0f);
}

void StagePad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panel.darker (0.4f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    const auto st = stageArea();

    // room seen from above, with perspective lines to the corners
    g.setGradientFill (juce::ColourGradient (Colours::amber.withAlpha (0.10f), st.getCentreX(), st.getY(),
                                             Colours::amber.withAlpha (0.02f), st.getCentreX(), st.getBottom(), false));
    g.fillRect (st);
    g.setColour (Colours::dim.withAlpha (0.6f));
    g.drawRect (st, 1.0f);
    const auto inner = st.reduced (st.getWidth() * 0.18f, st.getHeight() * 0.18f);
    g.setColour (Colours::dim.withAlpha (0.25f));
    g.drawRect (inner, 1.0f);
    for (auto [a, b] : { std::pair { st.getTopLeft(), inner.getTopLeft() }, std::pair { st.getTopRight(), inner.getTopRight() },
                         std::pair { st.getBottomLeft(), inner.getBottomLeft() }, std::pair { st.getBottomRight(), inner.getBottomRight() } })
        g.drawLine ({ a, b }, 1.0f);

    // listener (mic pair) at the front edge
    const juce::Point<float> lis (st.getCentreX(), st.getBottom() + 8.0f);
    g.setColour (Colours::text.withAlpha (0.85f));
    for (float dx : { -7.0f, 7.0f })
        g.fillRoundedRectangle (lis.x + dx - 2.5f, lis.y - 6.0f, 5.0f, 11.0f, 2.5f);

    // source
    const float sx = juce::jmap (x, -100.0f, 100.0f, st.getX(), st.getRight());
    const float sy = juce::jmap (d, 0.0f, 100.0f, st.getBottom() - 6.0f, st.getY() + 6.0f);
    g.setColour (Colours::amber.withAlpha (0.18f));
    g.drawLine (lis.x, lis.y - 6.0f, sx, sy, 1.0f);
    for (float rad : { 16.0f, 11.0f })
    {
        g.setColour (Colours::amber.withAlpha (rad > 12.0f ? 0.18f : 0.35f));
        g.fillEllipse (sx - rad, sy - rad, rad * 2.0f, rad * 2.0f);
    }
    g.setColour (juce::Colours::white);
    g.fillEllipse (sx - 6.0f, sy - 6.0f, 12.0f, 12.0f);

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("STAGE", r.removeFromTop (20.0f), juce::Justification::centred);
    auto bottom = getLocalBounds().toFloat().removeFromBottom (18.0f).reduced (10.0f, 0.0f);
    g.drawText (juce::String ("SOURCE ") + (juce::roundToInt (x) == 0 ? "C" : (x < 0 ? "L" + juce::String (-juce::roundToInt (x)) : "R" + juce::String (juce::roundToInt (x)))),
                bottom, juce::Justification::centredLeft);
    g.drawText ("DIST " + juce::String (juce::roundToInt (d)) + "%", bottom, juce::Justification::centredRight);
}

void StagePad::setFromMouse (juce::Point<float> p)
{
    const auto st = stageArea();
    const float nx = juce::jlimit (-100.0f, 100.0f, juce::jmap (p.x, st.getX(), st.getRight(), -100.0f, 100.0f));
    const float nd = juce::jlimit (0.0f, 100.0f, juce::jmap (p.y, st.getBottom() - 6.0f, st.getY() + 6.0f, 0.0f, 100.0f));
    xAttach.setValueAsPartOfGesture (nx);
    dAttach.setValueAsPartOfGesture (nd);
}

void StagePad::mouseDown (const juce::MouseEvent& e)
{
    xAttach.beginGesture();
    dAttach.beginGesture();
    setFromMouse (e.position);
}

void StagePad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void StagePad::mouseUp (const juce::MouseEvent&)
{
    xAttach.endGesture();
    dAttach.endGesture();
}

void StagePad::mouseDoubleClick (const juce::MouseEvent&)
{
    xAttach.setValueAsCompleteGesture (0.0f);
    dAttach.setValueAsCompleteGesture (30.0f);
}

//==============================================================================
void IRView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::bg.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 8.0f);

    auto plot = r.reduced (12.0f, 22.0f).withTrimmedBottom (-8.0f);
    const float midY = plot.getCentreY(), half = plot.getHeight() * 0.5f;

    g.setFont (juce::FontOptions (10.0f));
    for (float db : { -24.0f, -48.0f })
    {
        const float off = (db + 72.0f) / 72.0f * half;
        g.setColour (Colours::panelEdge);
        g.drawHorizontalLine ((int) (midY - off), plot.getX(), plot.getRight());
        g.drawHorizontalLine ((int) (midY + off), plot.getX(), plot.getRight());
    }
    if (display.seconds > 0.0f)
    {
        const float step = display.seconds > 6.0f ? 1.0f : display.seconds > 2.0f ? 0.5f : display.seconds > 0.6f ? 0.2f : 0.05f;
        g.setColour (Colours::dim);
        for (float t = step; t < display.seconds; t += step)
        {
            const float x = plot.getX() + plot.getWidth() * t / display.seconds;
            g.drawText (juce::String (t, step < 0.1f ? 2 : 1) + "s", (int) x - 18, (int) plot.getBottom() - 2, 36, 12, juce::Justification::centred);
        }
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        const auto& env = pass == 0 ? display.envL : display.envR;
        if (env.empty()) continue;
        juce::Path p;
        p.startNewSubPath (plot.getX(), midY);
        for (size_t i = 0; i < env.size(); ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (env.size() - 1);
            const float off = juce::jlimit (0.0f, 1.0f, (env[i] + 72.0f) / 72.0f) * half;
            p.lineTo (x, pass == 0 ? midY - off : midY + off);
        }
        p.lineTo (plot.getRight(), midY);
        p.closeSubPath();
        const auto c = pass == 0 ? Colours::amber : Colours::copper;
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.85f), plot.getX(), midY, c.withAlpha (0.15f), plot.getRight(), midY, false));
        g.fillPath (p);
    }

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("IMPULSE RESPONSE   RT60 " + juce::String (display.rt60, 2) + " s   LENGTH " + juce::String (display.seconds, 2) + " s",
                r.reduced (12.0f, 4.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
}

//==============================================================================
BrewVerbEditor::BrewVerbEditor (BrewVerbProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      decay       (p.apvts, ParamID::decay,      "Decay", true),
      spaceView   (p),
      stagePad    (p),
      outGain     (p.apvts, ParamID::output,     "Output Gain"),
      spread      (p.apvts, ParamID::width,      "Spread"),
      dryWet      (p.apvts, ParamID::mix,        "Dry / Wet"),
      hBrightness (p.apvts, ParamID::brightness, "Brightness"),
      hBass       (p.apvts, ParamID::bass,       "Bass"),
      hTreble     (p.apvts, ParamID::treble,     "Treble"),
      hPredelay   (p.apvts, ParamID::predelay,   "Pre-delay"),
      hAttack     (p.apvts, ParamID::attack,     "Attack"),
      hEqOnA      (p.apvts, ParamID::eqon, hEqOn),
      cLowCut     (p.apvts, ParamID::lowcut,     "Low Cut"),
      cHighCut    (p.apvts, ParamID::highcut,    "High Cut"),
      cBass       (p.apvts, ParamID::bass,       "Bass"),
      cTreble     (p.apvts, ParamID::treble,     "Treble"),
      cBrightness (p.apvts, ParamID::brightness, "Brightness"),
      cEqOnA      (p.apvts, ParamID::eqon, cEqOn),
      tSize       (p.apvts, ParamID::size,       "Size"),
      tLength     (p.apvts, ParamID::length,     "Length"),
      tAttack     (p.apvts, ParamID::attack,     "Attack"),
      tPredelay   (p.apvts, ParamID::predelay,   "Pre-delay"),
      tReverseA   (p.apvts, ParamID::reverse, tReverse),
      lEarly      (p.apvts, ParamID::early,      "Early Ref."),
      lTail       (p.apvts, ParamID::tail,       "Tail"),
      lDistance   (p.apvts, ParamID::posdist,    "Distance"),
      lPosX       (p.apvts, ParamID::posx,       "Left / Right"),
      lDryWet     (p.apvts, ParamID::mix,        "Dry / Wet"),
      lOutput     (p.apvts, ParamID::output,     "Output Gain")
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (decay);
    addAndMakeVisible (spaceView);
    addAndMakeVisible (stagePad);
    for (auto* k : { &outGain, &spread, &dryWet })
        addAndMakeVisible (*k);

    spaceView.onSelectSpace = [this] (int i) { selectSpace (i); };
    spaceView.onBuiltIn = [this]
    {
        proc.clearUserIR();
        statusLabel.setText (brew::getSpaceDescription (proc.getCurrentSpace()), juce::dontSendNotification);
    };
    spaceView.onLoadIR = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose an impulse response",
                                                       juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      loadIR (fc.getResult());
                              });
    };

    // ---- Tabs ------------------------------------------------------------------
    const char* tabNames[] = { "HOME", "COLOR", "TIME", "LEVELS", "SNAPSHOTS" };
    for (int i = 0; i < numTabs; ++i)
    {
        auto* b = tabButtons.add (new juce::TextButton (tabNames[i]));
        b->getProperties().set ("tab", true);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (101);
        b->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (b);
        addChildComponent (pages[(size_t) i]);
    }

    auto& home = pages[Home];
    for (juce::Component* c : { (juce::Component*) &hBrightness, (juce::Component*) &hBass, (juce::Component*) &hTreble,
                                (juce::Component*) &hPredelay, (juce::Component*) &hAttack, (juce::Component*) &hEqOn })
        home.addAndMakeVisible (c);

    auto& color = pages[Color];
    for (juce::Component* c : { (juce::Component*) &cLowCut, (juce::Component*) &cHighCut, (juce::Component*) &cBass,
                                (juce::Component*) &cTreble, (juce::Component*) &cBrightness, (juce::Component*) &cEqOn })
        color.addAndMakeVisible (c);

    auto& time = pages[Time];
    for (juce::Component* c : { (juce::Component*) &irView, (juce::Component*) &tSize, (juce::Component*) &tLength,
                                (juce::Component*) &tAttack, (juce::Component*) &tPredelay, (juce::Component*) &tReverse })
        time.addAndMakeVisible (c);

    auto& levels = pages[Levels];
    for (auto* k : { &lEarly, &lTail, &lDistance, &lPosX, &lDryWet, &lOutput })
        levels.addAndMakeVisible (k);

    auto& snaps = pages[Snapshots];
    for (int i = 0; i < BrewVerbProcessor::numSnapshots; ++i)
    {
        auto* b = snapButtons.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
        b->getProperties().set ("fontSize", 22.0f);
        b->getProperties().set ("snap", true);
        b->onClick = [this, i]
        {
            if (storeButton.getToggleState())
            {
                proc.storeSnapshot (i);
                storeButton.setToggleState (false, juce::dontSendNotification);
                snapHint.setText ("Stored snapshot " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + ".", juce::dontSendNotification);
            }
            else if (proc.hasSnapshot (i))
            {
                proc.recallSnapshot (i);
                snapHint.setText ("Recalled snapshot " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + ".", juce::dontSendNotification);
            }
            else
                snapHint.setText ("Slot is empty. Turn on STORE, then click the slot to save the current settings.", juce::dontSendNotification);
        };
        snaps.addAndMakeVisible (b);
    }
    snaps.addAndMakeVisible (storeButton);
    snapHint.setText ("Click a slot to recall it. Turn on STORE, then click a slot to save the current settings there.",
                      juce::dontSendNotification);
    snapHint.setJustificationType (juce::Justification::centred);
    snapHint.setColour (juce::Label::textColourId, Colours::dim);
    snapHint.setFont (juce::FontOptions (13.0f));
    snaps.addAndMakeVisible (snapHint);

    statusLabel.setFont (juce::FontOptions (12.0f));
    statusLabel.setColour (juce::Label::textColourId, Colours::dim);
    statusLabel.setText (proc.hasUserIR() ? "User impulse response loaded. Drag a new file onto the picture to replace it."
                                          : brew::getSpaceDescription (proc.getCurrentSpace()),
                         juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    proc.addChangeListener (this);
    irView.setDisplay (proc.getDisplay());

    showTab (Home);
    setResizable (false, false);
    setSize (1060, 640);
    startTimerHz (15);
}

BrewVerbEditor::~BrewVerbEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void BrewVerbEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, numTabs - 1, index);
    for (int i = 0; i < numTabs; ++i)
    {
        tabButtons[i]->setToggleState (i == currentTab, juce::dontSendNotification);
        pages[(size_t) i].setVisible (i == currentTab);
    }
}

void BrewVerbEditor::selectSpace (int index)
{
    if (auto* p = proc.apvts.getParameter (ParamID::space))
    {
        if (proc.hasUserIR())
            proc.clearUserIR();
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
    statusLabel.setColour (juce::Label::textColourId, Colours::dim);
    statusLabel.setText (brew::getSpaceDescription (index), juce::dontSendNotification);
    spaceView.repaint();
}

void BrewVerbEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    irView.setDisplay (proc.getDisplay());
    spaceView.refresh();
}

bool BrewVerbEditor::tabIsModified (int tab) const
{
    static const std::vector<std::vector<const char*>> ids =
    {
        { ParamID::brightness, ParamID::bass, ParamID::treble, ParamID::predelay, ParamID::attack, ParamID::eqon },
        { ParamID::lowcut, ParamID::highcut, ParamID::bass, ParamID::treble, ParamID::brightness, ParamID::eqon },
        { ParamID::size, ParamID::length, ParamID::attack, ParamID::predelay, ParamID::reverse },
        { ParamID::early, ParamID::tail, ParamID::posdist, ParamID::posx, ParamID::mix, ParamID::output },
    };
    if (tab == Snapshots)
    {
        for (int i = 0; i < BrewVerbProcessor::numSnapshots; ++i)
            if (proc.hasSnapshot (i)) return true;
        return false;
    }
    for (auto* id : ids[(size_t) tab])
        if (auto* p = proc.apvts.getParameter (id))
            if (std::abs (p->getValue() - p->getDefaultValue()) > 0.002f)
                return true;
    return false;
}

void BrewVerbEditor::timerCallback()
{
    for (int i = 0; i < numTabs; ++i)
    {
        const bool mod = tabIsModified (i);
        if ((bool) tabButtons[i]->getProperties()["dot"] != mod)
        {
            tabButtons[i]->getProperties().set ("dot", mod);
            tabButtons[i]->repaint();
        }
    }
    for (int i = 0; i < snapButtons.size(); ++i)
    {
        const bool stored = proc.hasSnapshot (i), last = proc.getLastSnapshot() == i;
        if ((bool) snapButtons[i]->getProperties()["stored"] != stored || snapButtons[i]->getToggleState() != last)
        {
            snapButtons[i]->getProperties().set ("stored", stored);
            snapButtons[i]->setToggleState (last, juce::dontSendNotification);
            snapButtons[i]->repaint();
        }
    }

    // repaint the illustration when anything it depicts changes
    const auto key = juce::String (proc.getCurrentSpace()) + "|" + juce::String (proc.apvts.getRawParameterValue (ParamID::size)->load())
                   + "|" + juce::String (proc.apvts.getRawParameterValue (ParamID::posx)->load())
                   + "|" + juce::String (proc.apvts.getRawParameterValue (ParamID::posdist)->load())
                   + "|" + juce::String ((int) proc.hasUserIR());
    if (key != spaceView.getProperties()["key"].toString())
    {
        spaceView.getProperties().set ("key", key);
        spaceView.repaint();
    }
}

void BrewVerbEditor::loadIR (const juce::File& f)
{
    const auto err = proc.loadUserIR (f);
    statusLabel.setText (err.isNotEmpty() ? err : "Loaded " + f.getFileName() + "  (" + juce::String (f.getSize() / 1024) + " KB)",
                         juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, err.isNotEmpty() ? Colours::copper : Colours::dim);
    spaceView.repaint();
}

bool BrewVerbEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif")
            || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac"))
            return true;
    return false;
}

void BrewVerbEditor::filesDropped (const juce::StringArray& files, int, int)
{
    spaceView.setDragHighlight (false);
    for (auto& f : files)
        if (isInterestedInFileDrag ({ f }))
        {
            loadIR (juce::File (f));
            break;
        }
}

//==============================================================================
namespace
{
    const juce::Rectangle<int> topPanel    { 16, 16, 1028, 352 };
    const juce::Rectangle<int> bottomPanel { 16, 380, 1028, 220 };
}

void BrewVerbEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    for (auto r : { topPanel, bottomPanel })
    {
        g.setGradientFill (juce::ColourGradient (Colours::panelHi, (float) r.getX(), (float) r.getY(),
                                                 Colours::panel, (float) r.getX(), (float) r.getBottom(), false));
        g.fillRoundedRectangle (r.toFloat(), 18.0f);
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 18.0f, 1.0f);
    }

    // brand mark (original): a small amber "drop" + wordmark
    const float bx = 40.0f, by = 34.0f;
    juce::Path drop;
    drop.startNewSubPath (bx + 11.0f, by);
    drop.cubicTo (bx + 22.0f, by + 12.0f, bx + 22.0f, by + 26.0f, bx + 11.0f, by + 26.0f);
    drop.cubicTo (bx, by + 26.0f, bx, by + 12.0f, bx + 11.0f, by);
    g.setGradientFill (juce::ColourGradient (Colours::amber, bx, by, Colours::copper, bx, by + 26.0f, false));
    g.fillPath (drop);
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("brewverb", (int) bx + 30, (int) by - 4, 200, 26, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("CONVOLUTION SPACE DESIGNER", (int) bx + 31, (int) by + 20, 220, 14, juce::Justification::centredLeft);

    g.setColour (Colours::dim.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Sound Brewery Studio  /  open source, GPLv3", getLocalBounds().removeFromBottom (36).reduced (24, 0),
                juce::Justification::centredRight);
}

void BrewVerbEditor::paintOverChildren (juce::Graphics&) {}

void BrewVerbEditor::resized()
{
    // ---- top panel --------------------------------------------------------------
    decay.setBounds (40, 92, 236, 262);
    spaceView.setBounds (296, 30, 472, 324);

    stagePad.setBounds (790, 30, 236, 176);
    const int ky = 214, kw = 80;
    outGain.setBounds (784, ky, kw, 128);
    spread .setBounds (868, ky, kw, 128);
    dryWet .setBounds (952, ky, kw, 128);

    // ---- tabs -----------------------------------------------------------------------
    const int tabW = 116, tabH = 30;
    int tx = bottomPanel.getCentreX() - (tabW * numTabs) / 2;
    for (auto* b : tabButtons)
    {
        b->setBounds (tx, bottomPanel.getY() + 12, tabW, tabH);
        tx += tabW;
    }

    const auto pageArea = bottomPanel.withTrimmedTop (50).reduced (24, 8);
    for (auto& p : pages)
        p.setBounds (pageArea);

    auto rowOfKnobs = [] (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int w)
    {
        const int total = w * (int) items.size();
        int x = area.getCentreX() - total / 2;
        for (auto* c : items)
        {
            if (dynamic_cast<juce::ToggleButton*> (c) != nullptr)
                c->setBounds (juce::Rectangle<int> (x, area.getY(), w, area.getHeight()).withSizeKeepingCentre (w - 14, 32));
            else
                c->setBounds (x + 6, area.getY(), w - 12, area.getHeight());
            x += w;
        }
    };

    const auto local = pages[0].getLocalBounds().withTrimmedBottom (6);
    rowOfKnobs (local, { &hBrightness, &hBass, &hTreble, &hPredelay, &hAttack, &hEqOn }, 132);
    rowOfKnobs (local, { &cLowCut, &cHighCut, &cBass, &cTreble, &cBrightness, &cEqOn }, 132);
    {
        auto r = local;
        irView.setBounds (r.removeFromLeft (r.getWidth() * 44 / 100));
        r.removeFromLeft (10);
        rowOfKnobs (r, { &tSize, &tLength, &tAttack, &tPredelay, &tReverse }, r.getWidth() / 5);
    }
    rowOfKnobs (local, { &lEarly, &lTail, &lDistance, &lPosX, &lDryWet, &lOutput }, 132);
    {
        auto r = local;
        snapHint.setBounds (r.removeFromBottom (28));
        const int bw = 96, gap = 18;
        int x = r.getCentreX() - (bw * 5 + gap * 4) / 2;
        for (auto* b : snapButtons)
        {
            b->setBounds (x, r.getCentreY() - 34, bw, 68);
            x += bw + gap;
        }
        storeButton.setBounds (x, r.getCentreY() - 16, bw + 10, 32);
    }

    statusLabel.setBounds (20, getHeight() - 34, 640, 26);
}
