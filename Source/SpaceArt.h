/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).

    SpaceArt: original vector illustrations of each built-in space, drawn in
    code (no photographs). The source speaker and listener mics are placed on
    the floor according to the stage position.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "IRGenerator.h"

namespace brewui
{
    void drawSpaceArt (juce::Graphics& g, juce::Rectangle<float> area, const brew::SpaceInfo& info,
                       float sizeScale, float posX, float posDist);
}
