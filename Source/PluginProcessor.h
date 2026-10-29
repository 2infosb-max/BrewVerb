/*
    BrewVerb — open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "IRGenerator.h"

namespace ParamID
{
    inline constexpr const char* space    = "space";
    inline constexpr const char* size     = "size";
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* brightness = "brightness";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* tail     = "tail";
    inline constexpr const char* posx     = "posx";
    inline constexpr const char* posdist  = "posdist";
    inline constexpr const char* bass     = "bass";
    inline constexpr const char* treble   = "treble";
    inline constexpr const char* eqon     = "eqon";
    inline constexpr const char* early    = "early";
    inline constexpr const char* length   = "length";
    inline constexpr const char* reverse  = "reverse";
    inline constexpr const char* predelay = "predelay";
    inline constexpr const char* lowcut   = "lowcut";
    inline constexpr const char* highcut  = "highcut";
    inline constexpr const char* width    = "width";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* output   = "output";
}

class BrewVerbProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::Timer
{
public:
    BrewVerbProcessor();
    ~BrewVerbProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BrewVerb"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return tailSeconds.load(); }

    int getNumPrograms() override { return brew::getNumSpaces(); }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    /** Load a WAV/AIFF/FLAC impulse response. Returns an error message or empty on success. */
    juce::String loadUserIR (const juce::File& file);
    void clearUserIR();
    bool hasUserIR() const;
    juce::String getSourceName() const;

    struct Display
    {
        std::vector<float> envL, envR;   // dB envelope, ~600 points
        float seconds = 0.0f;
        float rt60 = 0.0f;
        juce::String name;
    };
    Display getDisplay() const;

    // ---- Snapshots (A-D): store / recall every parameter + loaded IR -------
    static constexpr int numSnapshots = 4;
    void storeSnapshot (int slot);
    void recallSnapshot (int slot);
    bool hasSnapshot (int slot) const;
    int  getLastSnapshot() const { return lastSnapshot; }

    int getCurrentSpace() const { return juce::roundToInt (pSpace->load()); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float newValue) override;
    void timerCallback() override;
    void requestRebuild();
    void launchRebuild (bool synchronous = false);
    void installPendingIR();
    void updateDisplay (const juce::AudioBuffer<float>& ir, double sr, const juce::String& name);

    // ---- DSP ---------------------------------------------------------------
    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 256 } };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 48000 };
    juce::dsp::StateVariableTPTFilter<float> lowCut, highCut;
    juce::dsp::IIR::Coefficients<float>::Ptr bassCoef  { new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0) },
                                             trebleCoef { new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0) };
    juce::dsp::IIR::Filter<float> bassF[2], trebleF[2];
    float lastBass = 1000.0f, lastTreble = 1000.0f;
    void updateShelves();
    juce::AudioBuffer<float> wetBuffer;
    juce::SmoothedValue<float> dryGain, wetGain, outGain, widthSm, preDelaySm;
    double currentRate = 0.0;

    // ---- IR state ------------------------------------------------------------
    mutable juce::CriticalSection irLock;
    std::shared_ptr<juce::AudioBuffer<float>> userIR;
    double userIRRate = 48000.0;
    juce::File userIRFile;

    juce::SpinLock pendingLock;               // builder thread -> audio thread hand-off
    juce::AudioBuffer<float> pendingIR;
    double pendingRate = 48000.0;
    bool hasPendingIR = false;

    std::atomic<bool> rebuildPending { false };
    std::atomic<juce::uint32> lastChangeMs { 0 };
    std::atomic<int> buildGeneration { 0 };
    std::atomic<double> tailSeconds { 2.0 };
    juce::ThreadPool builder { 1 };

    juce::ValueTree snapshots { "Snapshots" };
    int lastSnapshot = -1;

    mutable juce::CriticalSection displayLock;
    Display display;

    std::atomic<float>* pSpace = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pBrightness = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pTail = nullptr;
    std::atomic<float>* pPosX = nullptr;
    std::atomic<float>* pPosDist = nullptr;
    std::atomic<float>* pBass = nullptr;
    std::atomic<float>* pTreble = nullptr;
    std::atomic<float>* pEqOn = nullptr;
    std::atomic<float>* pEarly = nullptr;
    std::atomic<float>* pLength = nullptr;
    std::atomic<float>* pReverse = nullptr;
    std::atomic<float>* pPredelay = nullptr;
    std::atomic<float>* pLowcut = nullptr;
    std::atomic<float>* pHighcut = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrewVerbProcessor)
};
