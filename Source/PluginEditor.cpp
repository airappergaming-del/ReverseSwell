#include "PluginEditor.h"

using namespace juce;

namespace
{
    struct KnobSpec { const char* id; const char* name; };

    constexpr KnobSpec knobSpecs[] = {
        { ParamIDs::swellLength, "Swell Length" },
        { ParamIDs::roomSize,    "Room Size"    },
        { ParamIDs::decay,       "Decay"        },
        { ParamIDs::damping,     "Damping"      },
        { ParamIDs::preDelay,    "Pre-delay"    },
        { ParamIDs::width,       "Width"        },
        { ParamIDs::swellLevel,  "Swell Level"  },
        { ParamIDs::dryLevel,    "Dry Level"    },
    };
}

//==============================================================================
ReverseSwellEditor::ReverseSwellEditor (ReverseSwellProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lookAndFeel);

    // Header
    loadButton.getProperties().set ("primary", false);
    exportButton.getProperties().set ("primary", true);
    loadButton.onClick   = [this] { openLoadDialog(); };
    exportButton.onClick = [this] { openExportDialog(); };
    playButton.onClick   = [this]
    {
        if (proc.isPreviewing()) proc.stopPreview();
        else                     proc.startPreview();
    };
    for (auto* b : { &loadButton, &playButton, &exportButton })
        addAndMakeVisible (b);

    fileLabel.setFont (SwellLookAndFeel::font (13.0f));
    fileLabel.setColour (Label::textColourId, Palette::textDim);
    fileLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (fileLabel);

    statusLabel.setFont (SwellLookAndFeel::font (12.0f));
    statusLabel.setColour (Label::textColourId, Palette::highlight);
    statusLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (statusLabel);

    // Waveforms
    inputView.onFileDropped      = [this] (const File& f) { loadFile (f); };
    inputView.onSelectionChanged = [this] (int s, int e) { proc.setSelection (s, e); };
    inputView.setSnapToZero (proc.getSnapToZero());
    outputView.onDragOut         = [this] { startDragOut(); };
    addAndMakeVisible (inputView);
    addAndMakeVisible (outputView);

    // Knobs
    auto& state = proc.getState();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        addAndMakeVisible (k.slider);   // add first so the text box is created by our LookAndFeel
        addAndMakeVisible (k.label);
        k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 72, 16);
        k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        k.attachment = std::make_unique<SliderAttachment> (state, knobSpecs[i].id, k.slider);

        if (auto* param = state.getParameter (knobSpecs[i].id))
            k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        k.label.setText (knobSpecs[i].name, dontSendNotification);
        k.label.setFont (SwellLookAndFeel::font (12.0f, true));
        k.label.setColour (Label::textColourId, Palette::text);
        k.label.setJustificationType (Justification::centred);
    }

    // Curve + toggles
    curveLabel.setText ("Curve", dontSendNotification);
    curveLabel.setFont (SwellLookAndFeel::font (12.0f, true));
    curveLabel.setColour (Label::textColourId, Palette::text);
    addAndMakeVisible (curveLabel);

    curveBox.addItemList ({ "Linear", "Exponential", "Log" }, 1);
    curveAttachment = std::make_unique<ComboBoxAttachment> (state, ParamIDs::swellCurve, curveBox);
    addAndMakeVisible (curveBox);

    includeDryAttachment    = std::make_unique<ButtonAttachment> (state, ParamIDs::includeDry, includeDryToggle);
    normalizeAttachment     = std::make_unique<ButtonAttachment> (state, ParamIDs::normalize, normalizeToggle);
    selectionOnlyAttachment = std::make_unique<ButtonAttachment> (state, ParamIDs::selectionOnly, selectionOnlyToggle);

    includeDryToggle.setTooltip ("Off = export only the swell");
    selectionOnlyToggle.setTooltip ("On = reverb only the selection. Off = reverb from the selection to the end of the file");
    snapToggle.setTooltip ("Snap selection edges to zero crossings");

    snapToggle.setToggleState (proc.getSnapToZero(), dontSendNotification);
    snapToggle.onClick = [this]
    {
        proc.setSnapToZero (snapToggle.getToggleState());
        inputView.setSnapToZero (snapToggle.getToggleState());
    };

    for (auto* t : { &includeDryToggle, &normalizeToggle, &selectionOnlyToggle, &snapToggle })
        addAndMakeVisible (t);

    proc.addChangeListener (this);
    refreshFromProcessor();

    setResizable (true, true);
    setResizeLimits (760, 500, 1800, 1200);
    setSize (900, 560);

    startTimerHz (30);
}

ReverseSwellEditor::~ReverseSwellEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void ReverseSwellEditor::paint (Graphics& g)
{
    g.fillAll (Palette::background);

    // Title
    auto t = titleArea.toFloat();
    g.setFont (SwellLookAndFeel::font (20.0f, true));
    const String first = "REVERSE ";
    const float firstW = GlyphArrangement::getStringWidth (g.getCurrentFont(), first);
    g.setColour (Palette::text);
    g.drawText (first, t, Justification::centredLeft);
    g.setColour (Palette::accent);
    g.drawText ("SWELL", t.withTrimmedLeft (firstW), Justification::centredLeft);

    // Captions
    auto caption = [&g] (Rectangle<int> area, const String& head, const String& tail)
    {
        g.setFont (SwellLookAndFeel::font (11.0f, true));
        g.setColour (Palette::text);
        const float w = GlyphArrangement::getStringWidth (g.getCurrentFont(), head);
        g.drawText (head, area, Justification::centredLeft);
        g.setFont (SwellLookAndFeel::font (11.0f));
        g.setColour (Palette::textDim);
        g.drawText (tail, area.withTrimmedLeft ((int) w + 10), Justification::centredLeft);
    };

    caption (inputCaption, "INPUT", "click-drag to select the hit   /   wheel to zoom   /   alt-drag to pan   /   double-click to reset");
    caption (outputCaption, "OUTPUT", "purple = swell   /   drag the waveform onto a DAW track");

    // Controls panel
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (controlsPanel.toFloat(), 8.0f);
    g.setColour (Palette::border);
    g.drawRoundedRectangle (controlsPanel.toFloat().reduced (0.5f), 8.0f, 1.0f);
}

void ReverseSwellEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    // Header
    auto header = r.removeFromTop (36);
    exportButton.setBounds (header.removeFromRight (112).reduced (0, 2));
    header.removeFromRight (8);
    playButton.setBounds (header.removeFromRight (80).reduced (0, 2));
    header.removeFromRight (8);
    loadButton.setBounds (header.removeFromRight (80).reduced (0, 2));
    header.removeFromRight (12);
    titleArea = header.removeFromLeft (180);
    statusLabel.setBounds (header.removeFromRight (120));
    fileLabel.setBounds (header);

    r.removeFromTop (12);

    // Controls
    controlsPanel = r.removeFromBottom (150);
    r.removeFromBottom (12);

    // Waveforms
    auto inputArea = r.removeFromTop ((int) ((float) r.getHeight() * 0.54f));
    inputCaption = inputArea.removeFromTop (20);
    inputView.setBounds (inputArea);

    r.removeFromTop (10);
    outputCaption = r.removeFromTop (20);
    outputView.setBounds (r);

    // Knob row + side column
    auto c = controlsPanel.reduced (14, 12);
    auto side = c.removeFromRight (250);
    c.removeFromRight (12);

    const int kw = c.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto area = c.removeFromLeft (kw);
        k.label.setBounds (area.removeFromTop (16));
        k.slider.setBounds (area);
    }

    auto curveRow = side.removeFromTop (30);
    curveLabel.setBounds (curveRow.removeFromLeft (52));
    curveBox.setBounds (curveRow);
    side.removeFromTop (10);

    const int rowH = side.getHeight() / 2;
    auto row1 = side.removeFromTop (rowH);
    auto row2 = side;
    includeDryToggle.setBounds (row1.removeFromLeft (row1.getWidth() / 2));
    normalizeToggle.setBounds (row1);
    selectionOnlyToggle.setBounds (row2.removeFromLeft (row2.getWidth() / 2));
    snapToggle.setBounds (row2);
}

//==============================================================================
void ReverseSwellEditor::changeListenerCallback (ChangeBroadcaster*)
{
    refreshFromProcessor();
}

void ReverseSwellEditor::refreshFromProcessor()
{
    if (proc.getSourceVersion() != shownSourceVersion)
    {
        shownSourceVersion = proc.getSourceVersion();
        if (auto src = proc.getSource())
            inputView.setAudio (src, proc.getSourceSampleRate(), true);
        else
            inputView.clearAudio();

        fileLabel.setText (proc.getSourceDescription(), dontSendNotification);
        snapToggle.setToggleState (proc.getSnapToZero(), dontSendNotification);
        inputView.setSnapToZero (proc.getSnapToZero());
    }

    inputView.setSelection (proc.getSelectionStart(), proc.getSelectionEnd());

    auto res = proc.getResult();
    const int id = res != nullptr ? res->id : 0;

    if (id != shownResultId)
    {
        shownResultId = id;
        if (res != nullptr)
        {
            // aliasing shared_ptr: keeps the whole result alive while the view shows its buffer
            outputView.setAudio (std::shared_ptr<const AudioBuffer<float>> (res, &res->audio), res->sampleRate, true);
            outputView.setRegion (res->swellStart, res->hitPosition);
        }
        else
        {
            outputView.clearAudio();
        }
    }

    exportButton.setEnabled (res != nullptr);
    playButton.setEnabled (res != nullptr);
}

void ReverseSwellEditor::timerCallback()
{
    outputView.setPlayhead (proc.getPlayheadSample());
    playButton.setButtonText (proc.isPreviewing() ? "Stop" : "Play");
    outputView.setBusy (proc.isRendering());

    String status;
    if (proc.isRendering())
        status = "Rendering...";
    else if (Time::getMillisecondCounter() < statusUntil)
        status = statusMessage;

    if (statusLabel.getText() != status)
        statusLabel.setText (status, dontSendNotification);
}

void ReverseSwellEditor::showStatus (const String& message)
{
    statusMessage = message;
    statusUntil = Time::getMillisecondCounter() + 4000;
}

//==============================================================================
void ReverseSwellEditor::loadFile (const File& file)
{
    String error;
    if (proc.loadFile (file, error))
        showStatus ("Loaded");
    else
        showStatus (error);
}

void ReverseSwellEditor::openLoadDialog()
{
    chooser = std::make_unique<FileChooser> ("Load audio file", proc.getSourceFile(),
                                             proc.getFormatManager().getWildcardForAllFormats());

    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                                  loadFile (f);
                          });
}

void ReverseSwellEditor::openExportDialog()
{
    if (proc.getResult() == nullptr)
        return;

    auto src = proc.getSourceFile();
    auto initial = src.existsAsFile()
                     ? src.getSiblingFile (src.getFileNameWithoutExtension() + "_swell.wav")
                     : File::getSpecialLocation (File::userDesktopDirectory).getChildFile ("swell.wav");

    chooser = std::make_unique<FileChooser> ("Export swell as WAV", initial, "*.wav");

    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                            | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == File())
                                  return;

                              f = f.withFileExtension ("wav");
                              showStatus (proc.writeResultToFile (f) ? "Exported " + f.getFileName()
                                                                     : String ("Export failed"));
                          });
}

void ReverseSwellEditor::startDragOut()
{
    const auto file = proc.writeTempFileForDrag();
    if (! file.existsAsFile())
    {
        showStatus ("Couldn't write temp file");
        return;
    }

    DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, &outputView);
}
