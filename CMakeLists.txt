cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) on macOS
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(BrewVerb VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE ---------------------------------------------------------------
# Use a local copy if JUCE_DIR is given, otherwise download JUCE 8.0.4
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Formats --------------------------------------------------------------
# AAX = Pro Tools, AU = Logic, VST3 = Nuendo/Reaper/Cubase etc., VST = legacy VST2 hosts.
# JUCE 8 bundles the AAX SDK (GPLv3 option), so no extra download is needed.
if(APPLE)
    set(BREWVERB_FORMATS AAX AU VST3 Standalone)
elseif(WIN32)
    set(BREWVERB_FORMATS AAX VST3 Standalone)
else()
    set(BREWVERB_FORMATS VST3 Standalone)
endif()

# VST2 needs Steinberg's VST2 SDK, which is no longer distributed and may not be
# shared. If you hold a copy, place it in ./vst2sdk (the folder that contains
# "pluginterfaces/vst2.x/aeffect.h") or pass -DBREWVERB_VST2_SDK=<path>.
set(BREWVERB_VST2_SDK "${CMAKE_CURRENT_SOURCE_DIR}/vst2sdk" CACHE PATH "Path to the Steinberg VST2 SDK (optional)")
set(_vst2_root "")
foreach(_cand "${BREWVERB_VST2_SDK}" "${BREWVERB_VST2_SDK}/VST2_SDK" "${BREWVERB_VST2_SDK}/vstsdk2.4")
    if(EXISTS "${_cand}/pluginterfaces/vst2.x/aeffect.h" AND EXISTS "${_cand}/pluginterfaces/vst2.x/aeffectx.h")
        set(_vst2_root "${_cand}")
        break()
    endif()
endforeach()
if(_vst2_root)
    juce_set_vst2_sdk_path("${_vst2_root}")
    list(APPEND BREWVERB_FORMATS VST)
    message(STATUS "BrewVerb: VST2 SDK found at ${_vst2_root} - building VST as well")
else()
    message(STATUS "BrewVerb: no VST2 SDK found - building without VST (AU / VST3 / AAX unaffected)")
endif()
message(STATUS "BrewVerb formats: ${BREWVERB_FORMATS}")

juce_add_plugin(BrewVerb
    COMPANY_NAME "Sound Brewery"
    COMPANY_WEBSITE "https://cinebrewery.in"
    BUNDLE_ID "com.soundbrewery.brewverb"
    PLUGIN_MANUFACTURER_CODE Sbrw
    PLUGIN_CODE Bvrb
    PRODUCT_NAME "BrewVerb"
    DESCRIPTION "Open-source convolution reverb and space designer"
    FORMATS ${BREWVERB_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Reverb
    VST2_CATEGORY kPlugCategRoomFx
    AAX_CATEGORY Reverb
    HARDENED_RUNTIME_ENABLED TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BrewVerb PRIVATE
    Source/IRGenerator.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpaceArt.cpp)

target_include_directories(BrewVerb PRIVATE Source)

target_compile_definitions(BrewVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BrewVerb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Offline test tool (renders every space to WAV; not shipped) ---------
option(BREWVERB_BUILD_TESTS "Build the offline IR render test" OFF)
if(BREWVERB_BUILD_TESTS)
    juce_add_console_app(BrewVerbIRTest PRODUCT_NAME "BrewVerbIRTest")
    target_sources(BrewVerbIRTest PRIVATE tests/RenderIRs.cpp Source/IRGenerator.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/SpaceArt.cpp)
    target_include_directories(BrewVerbIRTest PRIVATE Source)
    target_compile_definitions(BrewVerbIRTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BrewVerbIRTest PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
