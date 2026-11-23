#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SwellRenderer.h"
#include "Params.h"

class ReverseSwellProcessor : public juce::AudioProcessor,
                              public juce::ChangeBroadcaster,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    ReverseSwellProcessor();
    ~ReverseSwellProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return true; }

    const juce::String getName() const override           { return "Reverse Swell"; }
    bool acceptsMidi() const override                     { return true; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 0.0; }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    // Editor API (message thread)
    juce::AudioProcessorValueTreeState& getState()        { return apvts; }
    juce::AudioFormatManager& getFormatManager()          { return formatManager; }

    bool loadFile (const juce::File& file, juce::String& error, bool resetSelection = true);
    std::shared_ptr<const juce::AudioBuffer<float>> getSource() const;
    double getSourceSampleRate() const;
    juce::File getSourceFile() const;
    juce::String getSourceDescription() const;
    int getSourceVersion() const                          { return sourceVersion.load(); }

    void setSelection (int start, int end);
    int getSelectionStart() const                         { return selectionStart.load(); }
    int getSelectionEnd() const                           { return selectionEnd.load(); }

    void setSnapToZero (bool shouldSnap)                  { snapToZero = shouldSnap; }
    bool getSnapToZero() const                            { return snapToZero.load(); }

    std::shared_ptr<const SwellResult> getResult() const;
    bool isRendering() const                              { return rendering.load(); }

    void startPreview();
    void stopPreview();
    bool isPreviewing() const                             { return previewPlaying.load(); }
    double getPlayheadSample() const                      { return playheadSample.load(); }

    bool writeResultToFile (const juce::File& file) const;
    juce::File writeTempFileForDrag();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;

    void requestRender();
    void renderLoop();
    void performRender();
    SwellSettings snapshotSettings() const;
    static bool writeWav (const SwellResult&, const juce::File&);

    class RenderThread : public juce::Thread
    {
    public:
        explicit RenderThread (ReverseSwellProcessor& p) : juce::Thread ("Reverse Swell Render"), owner (p) {}
        void run() override { owner.renderLoop(); }
    private:
        ReverseSwellProcessor& owner;
    };

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

    // Source audio (immutable once loaded; replaced wholesale)
    mutable juce::CriticalSection sourceLock;
    std::shared_ptr<const juce::AudioBuffer<float>> source;
    double sourceRate = 44100.0;
    int sourceOriginalChannels = 0;
    juce::File sourceFile;
    std::atomic<int> sourceVersion { 0 };

    std::atomic<int> selectionStart { 0 }, selectionEnd { 0 };
    std::atomic<bool> snapToZero { true };

    // Rendered result: audio thread only ever try-locks and reads a raw pointer
    mutable juce::SpinLock resultLock;
    std::shared_ptr<const SwellResult> result;
    int renderCounter = 0;

    // Playback state
    std::atomic<bool> previewPlaying { false }, restartRequested { false };
    std::atomic<double> playheadSample { -1.0 };
    double playPosition = 0.0;
    double hostSampleRate = 44100.0;

    // Background rendering
    std::atomic<int> renderRequests { 0 };
    std::atomic<bool> rendering { false };
    RenderThread renderThread { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseSwellProcessor)
};
