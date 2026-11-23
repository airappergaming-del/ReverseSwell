#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <memory>
#include <vector>

/** Waveform display used for both the input (drop target + selection) and
    the output (coloured swell + drag source). */
class WaveformView : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    enum class Mode { input, output };

    explicit WaveformView (Mode);

    void setAudio (std::shared_ptr<const juce::AudioBuffer<float>> audio, double sampleRate, bool resetZoom);
    void clearAudio();
    bool hasAudio() const noexcept { return audio != nullptr && audio->getNumSamples() > 0; }

    void setSelection (int start, int end);          // input mode
    void setRegion (int swellStart, int hit);        // output mode
    void setPlayhead (double samplePosition);        // < 0 hides it
    void setSnapToZero (bool shouldSnap)             { snap = shouldSnap; }
    void setBusy (bool isBusy);

    std::function<void (const juce::File&)> onFileDropped;
    std::function<void (int, int)> onSelectionChanged;
    std::function<void()> onDragOut;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    juce::Rectangle<float> waveArea() const;
    double xToSample (float x) const;
    float sampleToX (double sample) const;
    int clampSample (double s) const;
    int numSamples() const noexcept { return audio != nullptr ? audio->getNumSamples() : 0; }
    double minViewLength() const;
    void clampView();
    void buildPeaks();
    void getMinMax (int start, int end, float& mn, float& mx) const;
    juce::String formatTime (double samples) const;

    void paintEmpty (juce::Graphics&, juce::Rectangle<float>);
    void paintFrame (juce::Graphics&, juce::Rectangle<float>);

    Mode mode;
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 44100.0;

    static constexpr int binSize = 128;
    std::vector<float> peakMin, peakMax;

    double viewStart = 0.0, viewLength = 1.0;
    int selStart = 0, selEnd = 0, prevSelStart = 0, prevSelEnd = 0;
    int regionStart = -1, regionHit = -1;
    double playhead = -1.0;
    bool snap = true, dragHover = false, busy = false, dragOutStarted = false;

    enum class DragMode { none, newSelection, startHandle, endHandle, pan };
    DragMode dragMode = DragMode::none;
    int dragAnchor = 0;
    double panAnchorStart = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
