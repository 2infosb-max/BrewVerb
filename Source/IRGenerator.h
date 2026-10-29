/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).

    IRGenerator: builds stereo impulse responses for the built-in spaces
    (fully synthesized, no third-party IR recordings are used) and reshapes
    user-loaded IR files with the same Size / Decay / Brightness / Attack /
    Levels / Position / Length / Reverse controls.
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace brew
{
    struct ShapeSettings
    {
        int    space      = 3;      // index into getSpaceNames()
        float  size       = 1.0f;   // 0.5 .. 2.0  (room dimension scale)
        float  decay      = 1.0f;   // 0.25 .. 3.0 (RT60 multiplier)
        float  damping    = 0.3f;   // 0 .. 1      (high-frequency absorption = 1 - brightness)
        float  attack     = 0.0f;   // 0 .. 1      (slow fade-in of the reverb onset)
        float  earlyDb    = 0.0f;   // -24 .. +12  (early reflection level)
        float  tailDb     = 0.0f;   // -24 .. +12  (diffuse tail level)
        float  posX       = 0.0f;   // -1 .. +1    (source left / right on the stage)
        float  posDist    = 0.3f;   // 0 .. 1      (source near / far from the listener)
        float  lengthSec  = 12.0f;  // hard cap on IR length (with fade)
        bool   reverse    = false;
        double sampleRate = 48000.0;
    };

    enum class Scene { Booth, Living, Room, Studio, Office, Bathroom, Car, Corridor,
                       Chamber, Stairwell, Plate, Hall, Church, Warehouse, Cathedral, Courtyard };

    struct SpaceInfo
    {
        juce::String name, description, category;
        Scene scene;
        float widthM, depthM, heightM;   // physical size at Size = 100 %
        float rt60;                      // mid-band RT60 at Decay = 100 %
    };

    const juce::StringArray& getSpaceNames();
    juce::String getSpaceDescription (int index);
    SpaceInfo getSpaceInfo (int index);
    int getNumSpaces();

    /** Builds a stereo IR for one of the built-in spaces. */
    juce::AudioBuffer<float> generateSpace (const ShapeSettings& s);

    /** Reshapes a user IR (any channel count / sample rate) into a stereo IR. */
    juce::AudioBuffer<float> shapeUserIR (const juce::AudioBuffer<float>& source,
                                          double sourceRate,
                                          const ShapeSettings& s);

    /** Estimates RT60 (seconds) of an IR from its Schroeder decay curve. */
    float estimateRT60 (const juce::AudioBuffer<float>& ir, double sampleRate);
}
