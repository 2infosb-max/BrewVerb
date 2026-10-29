/*
    BrewVerb — open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Parameters that change the impulse response (rebuilt on a background thread)
    const char* const irParams[] = { ParamID::space, ParamID::size, ParamID::decay, ParamID::brightness,
                                     ParamID::attack, ParamID::early, ParamID::tail, ParamID::posx,
                                     ParamID::posdist, ParamID::length, ParamID::reverse };

    bool isIRParam (const juce::String& id)
    {
        for (auto* p : irParams)
            if (id == p) return true;
        return false;
    }
}

//==============================================================================
BrewVerbProcessor::BrewVerbProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrewVerb", createLayout())
{
    pSpace    = apvts.getRawParameterValue (ParamID::space);
    pSize     = apvts.getRawParameterValue (ParamID::size);
    pDecay    = apvts.getRawParameterValue (ParamID::decay);
    pBrightness = apvts.getRawParameterValue (ParamID::brightness);
    pAttack   = apvts.getRawParameterValue (ParamID::attack);
    pTail     = apvts.getRawParameterValue (ParamID::tail);
    pPosX     = apvts.getRawParameterValue (ParamID::posx);
    pPosDist  = apvts.getRawParameterValue (ParamID::posdist);
    pBass     = apvts.getRawParameterValue (ParamID::bass);
    pTreble   = apvts.getRawParameterValue (ParamID::treble);
    pEqOn     = apvts.getRawParameterValue (ParamID::eqon);
    pEarly    = apvts.getRawParameterValue (ParamID::early);
    pLength   = apvts.getRawParameterValue (ParamID::length);
    pReverse  = apvts.getRawParameterValue (ParamID::reverse);
    pPredelay = apvts.getRawParameterValue (ParamID::predelay);
    pLowcut   = apvts.getRawParameterValue (ParamID::lowcut);
    pHighcut  = apvts.getRawParameterValue (ParamID::highcut);
    pWidth    = apvts.getRawParameterValue (ParamID::width);
    pMix      = apvts.getRawParameterValue (ParamID::mix);
    pOutput   = apvts.getRawParameterValue (ParamID::output);

    for (auto* id : irParams)
        apvts.addParameterListener (id, this);

    for (int ch = 0; ch < 2; ++ch)
    {
        bassF[ch].coefficients = bassCoef;
        trebleF[ch].coefficients = trebleCoef;
    }

    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    startTimer (40);
}

BrewVerbProcessor::~BrewVerbProcessor()
{
    stopTimer();
    for (auto* id : irParams)
        apvts.removeParameterListener (id, this);

    ++buildGeneration;
    builder.removeAllJobs (true, 5000);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BrewVerbProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v)) + " %"; };
    auto ms  = [] (float v, int) { return String (v, 1) + " ms"; };
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (juce::roundToInt (v)) + " Hz"; };
    auto db  = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; };
    auto sec = [] (float v, int) { return String (v, 2) + " s"; };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::space, 1 }, "Space",
                                                       brew::getSpaceNames(), 3));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::size, 1 }, "Size",
        NormalisableRange<float> (50.0f, 200.0f, 1.0f), 100.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::decay, 1 }, "Decay",
        NormalisableRange<float> (25.0f, 300.0f, 1.0f, 0.6f), 100.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::brightness, 1 }, "Brightness",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 70.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::attack, 1 }, "Attack",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::tail, 1 }, "Tail Level",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::posx, 1 }, "Source Left/Right",
        NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int i = juce::roundToInt (v);
            return i == 0 ? String ("C") : (i < 0 ? "L" + String (-i) : "R" + String (i));
        })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::posdist, 1 }, "Source Distance",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 30.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::bass, 1 }, "Bass",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::treble, 1 }, "Treble",
        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::eqon, 1 }, "EQ On", true));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::early, 1 }, "Early Reflections",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::length, 1 }, "Length",
        NormalisableRange<float> (0.1f, 12.0f, 0.01f, 0.4f), 12.0f, AudioParameterFloatAttributes().withStringFromValueFunction (sec)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::reverse, 1 }, "Reverse", false));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::predelay, 1 }, "Pre-delay",
        NormalisableRange<float> (0.0f, 250.0f, 0.1f, 0.5f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (ms)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::lowcut, 1 }, "Low Cut",
        NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.4f), 80.0f, AudioParameterFloatAttributes().withStringFromValueFunction (hz)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::highcut, 1 }, "High Cut",
        NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f), 12000.0f, AudioParameterFloatAttributes().withStringFromValueFunction (hz)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::width, 1 }, "Spread",
        NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::mix, 1 }, "Dry/Wet",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output Gain",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    return layout;
}

//==============================================================================
bool BrewVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();

    if (out == juce::AudioChannelSet::mono())
        return in == juce::AudioChannelSet::mono();

    return false;
}

void BrewVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    // Build the IR for this sample rate synchronously so the very first
    // processed block already has the right space (load before prepare).
    if (! juce::approximatelyEqual (sampleRate, currentRate))
    {
        currentRate = sampleRate;
        launchRebuild (true);
    }
    installPendingIR();

    convolution.prepare (spec);
    preDelay.prepare (spec);
    preDelay.setMaximumDelayInSamples ((int) (0.3 * sampleRate) + 8);
    lowCut.prepare (spec);
    highCut.prepare (spec);
    wetBuffer.setSize (2, juce::jmax (1, samplesPerBlock), false, true, false);
    for (int ch = 0; ch < 2; ++ch) { bassF[ch].reset(); trebleF[ch].reset(); }
    lastBass = lastTreble = 1000.0f;   // force coefficient update
    updateShelves();

    for (auto* s : { &dryGain, &wetGain, &outGain, &widthSm })
        s->reset (sampleRate, 0.03);
    preDelaySm.reset (sampleRate, 0.08);

    const float mix = pMix->load() * 0.01f;
    dryGain.setCurrentAndTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetGain.setCurrentAndTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    widthSm.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    preDelaySm.setCurrentAndTargetValue (pPredelay->load() * 0.001f * (float) sampleRate);
}

void BrewVerbProcessor::installPendingIR()
{
    // Wait-free: if the builder is mid hand-off we simply pick it up next block.
    const juce::SpinLock::ScopedTryLockType lock (pendingLock);
    if (lock.isLocked() && hasPendingIR)
    {
        convolution.loadImpulseResponse (std::move (pendingIR), pendingRate,
                                         juce::dsp::Convolution::Stereo::yes,
                                         juce::dsp::Convolution::Trim::no,
                                         juce::dsp::Convolution::Normalise::no);   // already energy-normalised
        hasPendingIR = false;
    }
}

void BrewVerbProcessor::releaseResources()
{
    convolution.reset();
    preDelay.reset();
}

void BrewVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (n == 0 || numOut == 0) return;

    // Mono in -> stereo out: duplicate the input so dry is centred
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int ch = juce::jmax (numIn, numIn == 1 ? 2 : numIn); ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    // ---- Build wet signal --------------------------------------------------
    wetBuffer.setSize (2, n, false, false, true);
    wetBuffer.copyFrom (0, 0, buffer, 0, 0, n);
    wetBuffer.copyFrom (1, 0, buffer, juce::jmin (1, numOut - 1), 0, n);

    // Pre-delay
    preDelaySm.setTargetValue (pPredelay->load() * 0.001f * (float) getSampleRate());
    {
        auto* l = wetBuffer.getWritePointer (0);
        auto* r = wetBuffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            preDelay.setDelay (preDelaySm.getNextValue());
            preDelay.pushSample (0, l[i]);
            preDelay.pushSample (1, r[i]);
            l[i] = preDelay.popSample (0);
            r[i] = preDelay.popSample (1);
        }
    }

    installPendingIR();

    juce::dsp::AudioBlock<float> wetBlock (wetBuffer);
    auto block = wetBlock.getSubBlock (0, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    convolution.process (ctx);

    if (pEqOn->load() > 0.5f)
    {
        const float nyq = (float) getSampleRate() * 0.45f;
        lowCut.setCutoffFrequency (juce::jmin (pLowcut->load(), nyq));
        highCut.setCutoffFrequency (juce::jmin (pHighcut->load(), nyq));
        lowCut.process (ctx);
        highCut.process (ctx);

        updateShelves();
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = wetBuffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                d[i] = trebleF[ch].processSample (bassF[ch].processSample (d[i]));
        }
    }

    // ---- Width, mix, output -------------------------------------------------
    const float mix = pMix->load() * 0.01f;
    dryGain.setTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetGain.setTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    widthSm.setTargetValue (pWidth->load() * 0.01f);

    const auto* wl = wetBuffer.getReadPointer (0);
    const auto* wr = wetBuffer.getReadPointer (1);
    auto* outL = buffer.getWritePointer (0);
    auto* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float w  = widthSm.getNextValue();
        const float dg = dryGain.getNextValue();
        const float wg = wetGain.getNextValue();
        const float og = outGain.getNextValue();

        const float mid  = 0.5f * (wl[i] + wr[i]);
        const float side = 0.5f * (wl[i] - wr[i]) * w;

        if (outR != nullptr)
        {
            outL[i] = og * (dg * outL[i] + wg * (mid + side));
            outR[i] = og * (dg * outR[i] + wg * (mid - side));
        }
        else
        {
            outL[i] = og * (dg * outL[i] + wg * mid);
        }
    }
}

void BrewVerbProcessor::updateShelves()
{
    // Coefficients are written in place (no allocation on the audio thread)
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const float b = pBass->load(), t = pTreble->load();
    if (! juce::approximatelyEqual (b, lastBass))
    {
        *bassCoef = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (sr, 220.0, 0.7071f,
                                                                           juce::Decibels::decibelsToGain (b));
        lastBass = b;
    }
    if (! juce::approximatelyEqual (t, lastTreble))
    {
        *trebleCoef = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sr, 4500.0, 0.7071f,
                                                                              juce::Decibels::decibelsToGain (t));
        lastTreble = t;
    }
}

//==============================================================================
void BrewVerbProcessor::parameterChanged (const juce::String& id, float)
{
    if (isIRParam (id))
        requestRebuild();
}

void BrewVerbProcessor::requestRebuild()
{
    lastChangeMs = juce::Time::getMillisecondCounter();
    rebuildPending = true;
}

void BrewVerbProcessor::timerCallback()
{
    // Debounce: wait until knobs have been still for ~120 ms
    if (rebuildPending && juce::Time::getMillisecondCounter() - lastChangeMs.load() > 120)
    {
        rebuildPending = false;
        launchRebuild();
    }
}

void BrewVerbProcessor::launchRebuild (bool synchronous)
{
    const double sr = currentRate > 0.0 ? currentRate : 48000.0;

    brew::ShapeSettings s;
    s.space      = juce::roundToInt (pSpace->load());
    s.size       = pSize->load() * 0.01f;
    s.decay      = pDecay->load() * 0.01f;
    s.damping    = 1.0f - pBrightness->load() * 0.01f;
    s.attack     = pAttack->load() * 0.01f;
    s.earlyDb    = pEarly->load();
    s.tailDb     = pTail->load();
    s.posX       = pPosX->load() * 0.01f;
    s.posDist    = pPosDist->load() * 0.01f;
    s.lengthSec  = pLength->load();
    s.reverse    = pReverse->load() > 0.5f;
    s.sampleRate = sr;

    std::shared_ptr<juce::AudioBuffer<float>> src;
    double srcRate = 48000.0;
    juce::String name;
    {
        const juce::ScopedLock sl (irLock);
        src = userIR;
        srcRate = userIRRate;
        name = src != nullptr ? userIRFile.getFileNameWithoutExtension()
                              : brew::getSpaceNames()[s.space];
    }

    const int gen = ++buildGeneration;

    auto job = [this, s, src, srcRate, name, gen, sr]
    {
        if (gen != buildGeneration.load()) return;

        auto ir = src != nullptr ? brew::shapeUserIR (*src, srcRate, s)
                                 : brew::generateSpace (s);

        if (gen != buildGeneration.load()) return;

        // Energy-normalise so 100% wet sits ~6 dB under the dry signal for
        // broadband material, whatever the space or user IR.
        double energy = 0.0;
        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            for (int i = 0; i < ir.getNumSamples(); ++i)
                energy += (double) ir.getSample (ch, i) * ir.getSample (ch, i);
        energy /= juce::jmax (1, ir.getNumChannels());
        if (energy > 1.0e-12)
            ir.applyGain ((float) (0.5 / std::sqrt (energy)));

        updateDisplay (ir, sr, name);
        tailSeconds = (double) ir.getNumSamples() / sr + 0.25;

        {
            const juce::SpinLock::ScopedLockType lock (pendingLock);
            pendingIR = std::move (ir);
            pendingRate = sr;
            hasPendingIR = true;
        }
        sendChangeMessage();
    };

    if (synchronous)
        job();
    else
        builder.addJob (std::move (job));
}

void BrewVerbProcessor::updateDisplay (const juce::AudioBuffer<float>& ir, double sr, const juce::String& name)
{
    constexpr int points = 600;
    Display d;
    d.envL.resize (points);
    d.envR.resize (points);
    d.seconds = (float) ((double) ir.getNumSamples() / sr);
    d.rt60 = brew::estimateRT60 (ir, sr);
    d.name = name;

    const int n = ir.getNumSamples();
    float peak = 1.0e-9f;
    for (int ch = 0; ch < 2; ++ch)
        peak = juce::jmax (peak, ir.getMagnitude (ch, 0, n));

    for (int p = 0; p < points; ++p)
    {
        const int start = (int) ((juce::int64) p * n / points);
        const int len = juce::jmax (1, (int) ((juce::int64) (p + 1) * n / points) - start);
        d.envL[(size_t) p] = juce::Decibels::gainToDecibels (ir.getMagnitude (0, start, len) / peak, -72.0f);
        d.envR[(size_t) p] = juce::Decibels::gainToDecibels (ir.getMagnitude (1, start, len) / peak, -72.0f);
    }

    const juce::ScopedLock sl (displayLock);
    display = std::move (d);
}

BrewVerbProcessor::Display BrewVerbProcessor::getDisplay() const
{
    const juce::ScopedLock sl (displayLock);
    return display;
}

//==============================================================================
juce::String BrewVerbProcessor::loadUserIR (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
        return "Could not read \"" + file.getFileName() + "\". Use WAV, AIFF or FLAC.";

    const auto maxSamples = (juce::int64) (30.0 * reader->sampleRate);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxSamples);
    if (len < 16)
        return "That file is too short to be an impulse response.";

    auto buf = std::make_shared<juce::AudioBuffer<float>> ((int) juce::jmin (4u, reader->numChannels), len);
    reader->read (buf.get(), 0, len, 0, true, true);

    {
        const juce::ScopedLock sl (irLock);
        userIR = buf;
        userIRRate = reader->sampleRate;
        userIRFile = file;
    }

    lastChangeMs = 0;
    rebuildPending = true;
    return {};
}

void BrewVerbProcessor::clearUserIR()
{
    {
        const juce::ScopedLock sl (irLock);
        userIR.reset();
        userIRFile = juce::File();
    }
    lastChangeMs = 0;
    rebuildPending = true;
}

bool BrewVerbProcessor::hasUserIR() const
{
    const juce::ScopedLock sl (irLock);
    return userIR != nullptr;
}

juce::String BrewVerbProcessor::getSourceName() const
{
    const juce::ScopedLock sl (irLock);
    return userIR != nullptr ? userIRFile.getFileNameWithoutExtension()
                             : brew::getSpaceNames()[juce::roundToInt (pSpace->load())];
}

//==============================================================================
int BrewVerbProcessor::getCurrentProgram()
{
    return juce::roundToInt (pSpace->load());
}

void BrewVerbProcessor::setCurrentProgram (int index)
{
    if (auto* p = apvts.getParameter (ParamID::space))
    {
        clearUserIR();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    }
}

const juce::String BrewVerbProcessor::getProgramName (int index)
{
    return brew::getSpaceNames()[index];
}

void BrewVerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (irLock);
        state.setProperty ("userIR", userIR != nullptr ? userIRFile.getFullPathName() : juce::String(), nullptr);
    }
    state.removeChild (state.getChildWithName ("Snapshots"), nullptr);
    state.appendChild (snapshots.createCopy(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BrewVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid() || ! state.hasType (apvts.state.getType()))
            return;

        const juce::String irPath = state.getProperty ("userIR").toString();

        auto snaps = state.getChildWithName ("Snapshots");
        snapshots = snaps.isValid() ? snaps.createCopy() : juce::ValueTree ("Snapshots");
        state.removeChild (snaps, nullptr);
        apvts.replaceState (state);

        const juce::File f (irPath);
        if (irPath.isNotEmpty() && f.existsAsFile())
            loadUserIR (f);
        else
            clearUserIR();
    }
}

//==============================================================================
void BrewVerbProcessor::storeSnapshot (int slot)
{
    juce::ValueTree snap ("Snap");
    snap.setProperty ("slot", slot, nullptr);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            snap.setProperty (rp->getParameterID(), rp->getValue(), nullptr);
    {
        const juce::ScopedLock sl (irLock);
        snap.setProperty ("userIR", userIR != nullptr ? userIRFile.getFullPathName() : juce::String(), nullptr);
    }

    for (int i = snapshots.getNumChildren(); --i >= 0;)
        if ((int) snapshots.getChild (i).getProperty ("slot") == slot)
            snapshots.removeChild (i, nullptr);
    snapshots.appendChild (snap, nullptr);
    lastSnapshot = slot;
}

void BrewVerbProcessor::recallSnapshot (int slot)
{
    for (const auto& snap : snapshots)
    {
        if ((int) snap.getProperty ("slot") != slot) continue;

        const juce::String irPath = snap.getProperty ("userIR").toString();
        if (irPath.isNotEmpty() && juce::File (irPath).existsAsFile())
        {
            if (! hasUserIR() || getSourceName() != juce::File (irPath).getFileNameWithoutExtension())
                loadUserIR (juce::File (irPath));
        }
        else if (hasUserIR())
            clearUserIR();

        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (snap.hasProperty (rp->getParameterID()))
                {
                    rp->beginChangeGesture();
                    rp->setValueNotifyingHost ((float) snap.getProperty (rp->getParameterID()));
                    rp->endChangeGesture();
                }
        lastSnapshot = slot;
        return;
    }
}

bool BrewVerbProcessor::hasSnapshot (int slot) const
{
    for (const auto& snap : snapshots)
        if ((int) snap.getProperty ("slot") == slot)
            return true;
    return false;
}

//==============================================================================
juce::AudioProcessorEditor* BrewVerbProcessor::createEditor()
{
    return new BrewVerbEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrewVerbProcessor();
}
