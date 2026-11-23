#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <memory>

/** Everything the offline renderer needs, snapshotted from the parameters. */
struct SwellSettings
{
    double swellLengthSec = 3.0;   // how far before the hit the swell begins
    double decaySec       = 6.0;   // RT60 of the reverb
    double preDelayMs     = 10.0;
    float  roomSize       = 0.8f;  // 0..1
    float  damping        = 0.4f;  // 0..1
    float  width          = 1.0f;  // 0..1
    int    curve          = 1;     // 0 = linear, 1 = exponential, 2 = log
    float  swellGainDb    = 0.0f;
    float  dryGainDb      = 0.0f;
    bool   includeDry     = true;
    bool   normalize      = true;
    bool   selectionOnly  = true;  // false = reverb from the selection start to the end of the file
};

/** The rendered output. Always stereo. */
struct SwellResult
{
    juce::AudioBuffer<float> audio;
    double sampleRate  = 44100.0;
    int    swellStart  = 0;   // first sample of the swell in `audio`
    int    hitPosition = 0;   // where the selection start (the hit) lands in `audio`
    int    id          = 0;   // increments with every render
};

class SwellRenderer
{
public:
    /** Renders a reverse-reverb swell. Returns nullptr if cancelled.
        shouldCancel is polled regularly so a newer request can abort an old render. */
    static std::unique_ptr<SwellResult> render (const juce::AudioBuffer<float>& source,
                                                double sampleRate,
                                                int selectionStart,
                                                int selectionEnd,
                                                const SwellSettings& settings,
                                                const std::function<bool()>& shouldCancel);

    /** Nearest upward or downward zero crossing of the channel sum within +/- radius samples. */
    static int findNearestZeroCrossing (const juce::AudioBuffer<float>& buffer, int position, int radius);

    /** Shapes the fade-in. x in 0..1. */
    static float curveValue (int curve, float x);
};
