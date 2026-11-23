#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "WaveformView.h"
#include "SwellLookAndFeel.h"
#include <array>

class ReverseSwellEditor : public juce::AudioProcessorEditor,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit ReverseSwellEditor (ReverseSwellProcessor&);
    ~ReverseSwellEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void refreshFromProcessor();
    void loadFile (const juce::File&);
    void openLoadDialog();
    void openExportDialog();
    void startDragOut();
    void showStatus (const juce::String& message);

    ReverseSwellProcessor& proc;
    SwellLookAndFeel lookAndFeel;   // declared first so it outlives every child component

    juce::TextButton loadButton { "Load" }, playButton { "Play" }, exportButton { "Export WAV" };
    juce::Label fileLabel, statusLabel;

    WaveformView inputView  { WaveformView::Mode::input };
    WaveformView outputView { WaveformView::Mode::output };

    std::array<Knob, 8> knobs;

    juce::Label curveLabel;
    juce::ComboBox curveBox;
    std::unique_ptr<ComboBoxAttachment> curveAttachment;

    juce::ToggleButton includeDryToggle { "Include dry" }, normalizeToggle { "Normalize" },
                       selectionOnlyToggle { "Selection only" }, snapToggle { "Snap to zero" };
    std::unique_ptr<ButtonAttachment> includeDryAttachment, normalizeAttachment, selectionOnlyAttachment;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Rectangle<int> titleArea, inputCaption, outputCaption, controlsPanel;
    int shownSourceVersion = -1, shownResultId = -1;
    juce::String statusMessage;
    juce::uint32 statusUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseSwellEditor)
};
