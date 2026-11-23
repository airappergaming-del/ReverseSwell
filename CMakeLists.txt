cmake_minimum_required(VERSION 3.22)

project(ReverseSwell VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary on macOS (Apple Silicon + Intel)
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it.
#   cmake -B build -DJUCE_DIR=/path/to/JUCE
# ---------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(REVERSE_SWELL_COPY_AFTER_BUILD "Copy the VST3 into the system plug-in folder after building" OFF)

juce_add_plugin(ReverseSwell
    PRODUCT_NAME              "Reverse Swell"
    COMPANY_NAME              "Reverse Swell"
    BUNDLE_ID                 com.reverseswell.plugin
    PLUGIN_MANUFACTURER_CODE  Rvsw
    PLUGIN_CODE               Rswl
    FORMATS                   VST3 Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   ${REVERSE_SWELL_COPY_AFTER_BUILD}
    VST3_CATEGORIES           Fx Reverb)

target_sources(ReverseSwell PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WaveformView.cpp
    Source/SwellRenderer.cpp
    Source/SwellLookAndFeel.cpp)

target_compile_definitions(ReverseSwell PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(ReverseSwell
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
