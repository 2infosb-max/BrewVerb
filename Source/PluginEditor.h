/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).
*/
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace brewui
{
    namespace Colours
    {
        const juce::Colour bg        { 0xff141210 };
        const juce::Colour panel     { 0xff1e1b17 };
        const juce::Colour panelHi   { 0xff26221d };
        const juce::Colour panelEdge { 0xff332e26 };
        const juce::Colour text      { 0xffece4d6 };
        const juce::Colour dim       { 0xff8f8678 };
        const juce::Colour amber     { 0xffe8a33d };
        const juce::Colour copper    { 0xffc5683a };
        const juce::Colour track     { 0xff3a342b };
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float start, float end, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (13.0f, juce::Font::bold); }
        juce::Font getPopupMenuFont() override { return juce::FontOptions (15.0f); }
    };

    /** Rotary knob with label. Big = the main Decay knob. */
    struct Knob : public juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, bool big = false);
        void resized() override;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label  label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
        bool big;
    };

    /** The centre display: an original illustration of the selected space. */
    class SpaceView : public juce::Component
    {
    public:
        explicit SpaceView (BrewVerbProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;
        void setDragHighlight (bool b) { dragOver = b; repaint(); }
        void refresh() { display = proc.getDisplay(); repaint(); }

        std::function<void()> onLoadIR, onBuiltIn;
        std::function<void (int)> onSelectSpace;

    private:
        BrewVerbProcessor& proc;
        BrewVerbProcessor::Display display;
        juce::TextButton prev { "<" }, next { ">" }, load { "LOAD IR" }, builtIn { "BUILT-IN" };
        juce::Rectangle<int> nameArea;
        bool dragOver = false;
    };

    /** Top-down stage: drag the source (speaker) around in front of the listener. */
    class StagePad : public juce::Component
    {
    public:
        explicit StagePad (BrewVerbProcessor&);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
    private:
        juce::Rectangle<float> stageArea() const;
        void setFromMouse (juce::Point<float>);
        juce::ParameterAttachment xAttach, dAttach;
        float x = 0.0f, d = 30.0f;
    };

    /** IR energy envelope (TIME tab). */
    class IRView : public juce::Component
    {
    public:
        void setDisplay (BrewVerbProcessor::Display d) { display = std::move (d); repaint(); }
        void paint (juce::Graphics&) override;
    private:
        BrewVerbProcessor::Display display;
    };
}

class BrewVerbEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit BrewVerbEditor (BrewVerbProcessor&);
    ~BrewVerbEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { spaceView.setDragHighlight (true); }
    void fileDragExit (const juce::StringArray&) override { spaceView.setDragHighlight (false); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    /** For tests / screenshots. */
    void showTab (int index);
    void refreshIndicators() { timerCallback(); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void loadIR (const juce::File&);
    void selectSpace (int index);
    bool tabIsModified (int tab) const;

    using Knob = brewui::Knob;
    using BtnAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    BrewVerbProcessor& proc;
    brewui::LookAndFeel lnf;

    Knob decay;
    brewui::SpaceView spaceView;
    brewui::StagePad stagePad;
    Knob outGain, spread, dryWet;

    // ---- Tabs ----------------------------------------------------------------
    enum Tab { Home, Color, Time, Levels, Snapshots, numTabs };
    juce::OwnedArray<juce::TextButton> tabButtons;
    std::array<juce::Component, numTabs> pages;
    int currentTab = Home;

    // HOME
    Knob hBrightness, hBass, hTreble, hPredelay, hAttack;
    juce::ToggleButton hEqOn { "EQ ON" };  BtnAttach hEqOnA;
    // COLOR
    Knob cLowCut, cHighCut, cBass, cTreble, cBrightness;
    juce::ToggleButton cEqOn { "EQ ON" };  BtnAttach cEqOnA;
    // TIME
    brewui::IRView irView;
    Knob tSize, tLength, tAttack, tPredelay;
    juce::ToggleButton tReverse { "REVERSE" };  BtnAttach tReverseA;
    // LEVELS
    Knob lEarly, lTail, lDistance, lPosX, lDryWet, lOutput;
    // SNAPSHOTS
    juce::OwnedArray<juce::TextButton> snapButtons;
    juce::ToggleButton storeButton { "STORE" };
    juce::Label snapHint;

    juce::Label statusLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrewVerbEditor)
};
