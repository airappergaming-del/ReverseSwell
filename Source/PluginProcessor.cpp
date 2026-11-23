#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int debounceMs = 150;
}

//==============================================================================
ReverseSwellProcessor::ReverseSwellProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ReverseSwellState", createLayout())
{
    formatManager.registerBasicFormats();

    for (auto* id : ParamIDs::all)
        apvts.addParameterListener (id, this);

    renderThread.startThread();
}

ReverseSwellProcessor::~ReverseSwellProcessor()
{
    for (auto* id : ParamIDs::all)
        apvts.removeParameterListener (id, this);

    renderThread.signalThreadShouldExit();
    renderThread.notify();
    renderThread.stopThread (4000);
}

juce::AudioProcessorValueTreeState::ParameterLayout ReverseSwellProcessor::createLayout()
{
    using namespace juce;

    auto secs = [] (float v, int) { return String (v, 2) + " s"; };
    auto pct  = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto ms   = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto db   = [] (float v, int) { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; };

    NormalisableRange<float> swellRange (0.5f, 8.0f, 0.01f);
    swellRange.setSkewForCentre (2.5f);
    NormalisableRange<float> decayRange (0.5f, 20.0f, 0.01f);
    decayRange.setSkewForCentre (5.0f);
    NormalisableRange<float> preRange (0.0f, 200.0f, 1.0f);
    NormalisableRange<float> dbRange (-60.0f, 12.0f, 0.1f);
    dbRange.setSkewForCentre (-12.0f);

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::swellLength, 1 }, "Swell Length",
                    swellRange, 3.0f, AudioParameterFloatAttributes().withStringFromValueFunction (secs)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::roomSize, 1 }, "Room Size",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::decay, 1 }, "Decay",
                    decayRange, 6.0f, AudioParameterFloatAttributes().withStringFromValueFunction (secs)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::damping, 1 }, "Damping",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::preDelay, 1 }, "Pre-delay",
                    preRange, 10.0f, AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::width, 1 }, "Width",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::swellCurve, 1 }, "Swell Curve",
                    StringArray { "Linear", "Exponential", "Log" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::swellLevel, 1 }, "Swell Level",
                    dbRange, 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::dryLevel, 1 }, "Dry Level",
                    dbRange, 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::includeDry, 1 }, "Include Dry", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::normalize, 1 }, "Normalize", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::selectionOnly, 1 }, "Reverb Selection Only", true));

    return layout;
}

//==============================================================================
void ReverseSwellProcessor::prepareToPlay (double sampleRate, int)
{
    hostSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

bool ReverseSwellProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void ReverseSwellProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Input audio passes through untouched; the preview is added on top.
    for (const auto meta : midi)
    {
        if (meta.getMessage().isNoteOn())
        {
            playPosition   = 0.0;
            previewPlaying = true;
        }
    }

    if (restartRequested.exchange (false))
        playPosition = 0.0;

    if (! previewPlaying.load())
    {
        playheadSample = -1.0;
        return;
    }

    const juce::SpinLock::ScopedTryLockType lock (resultLock);
    if (! lock.isLocked())
        return;

    const auto* r = result.get();
    if (r == nullptr || r->audio.getNumSamples() < 2)
    {
        previewPlaying = false;
        playheadSample = -1.0;
        return;
    }

    const double step  = r->sampleRate / hostSampleRate;
    const int len      = r->audio.getNumSamples();
    const float* srcL  = r->audio.getReadPointer (0);
    const float* srcR  = r->audio.getReadPointer (juce::jmin (1, r->audio.getNumChannels() - 1));
    double pos = playPosition;

    for (int i = 0; i < numSamples; ++i)
    {
        const int idx = (int) pos;
        if (idx >= len - 1)
        {
            previewPlaying = false;
            break;
        }

        const float frac = (float) (pos - (double) idx);
        const float l = srcL[idx] + frac * (srcL[idx + 1] - srcL[idx]);
        const float rr = srcR[idx] + frac * (srcR[idx + 1] - srcR[idx]);

        if (numOut == 1)
            buffer.addSample (0, i, 0.5f * (l + rr));
        else
        {
            buffer.addSample (0, i, l);
            buffer.addSample (1, i, rr);
        }

        pos += step;
    }

    playPosition   = pos;
    playheadSample = previewPlaying.load() ? pos : -1.0;
}

//==============================================================================
juce::AudioProcessorEditor* ReverseSwellProcessor::createEditor()
{
    return new ReverseSwellEditor (*this);
}

void ReverseSwellProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("filePath",  getSourceFile().getFullPathName(), nullptr);
    state.setProperty ("selStart",  selectionStart.load(), nullptr);
    state.setProperty ("selEnd",    selectionEnd.load(), nullptr);
    state.setProperty ("snapToZero", snapToZero.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ReverseSwellProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);

    snapToZero = (bool) state.getProperty ("snapToZero", true);

    const juce::File file (state.getProperty ("filePath").toString());
    if (file.existsAsFile())
    {
        juce::String error;
        if (loadFile (file, error, false))
            setSelection ((int) state.getProperty ("selStart", 0), (int) state.getProperty ("selEnd", 0));
    }
}

//==============================================================================
bool ReverseSwellProcessor::loadFile (const juce::File& file, juce::String& error, bool resetSelection)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Couldn't read " + file.getFileName();
        return false;
    }

    const auto maxSamples = (juce::int64) (reader->sampleRate * 60.0 * 20.0);
    if (reader->lengthInSamples <= 0)
    {
        error = "That file is empty";
        return false;
    }
    if (reader->lengthInSamples > maxSamples)
    {
        error = "File too long (max 20 minutes)";
        return false;
    }

    const int numChannels = juce::jlimit (1, 2, (int) reader->numChannels);
    const int numSamples  = (int) reader->lengthInSamples;

    auto buffer = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);
    reader->read (buffer.get(), 0, numSamples, 0, true, numChannels > 1);

    {
        const juce::ScopedLock sl (sourceLock);
        source                 = buffer;
        sourceRate             = reader->sampleRate;
        sourceOriginalChannels = (int) reader->numChannels;
        sourceFile             = file;
    }

    ++sourceVersion;

    if (resetSelection)
    {
        // Guess the first hit: first sample above 25% of the peak, nudged back to a zero crossing.
        const float peak = buffer->getMagnitude (0, numSamples);
        int start = 0;

        if (peak > 0.0f)
        {
            const float threshold = peak * 0.25f;
            for (int i = 0; i < numSamples && start == 0; ++i)
                for (int ch = 0; ch < numChannels; ++ch)
                    if (std::abs (buffer->getSample (ch, i)) >= threshold)
                    {
                        start = i;
                        break;
                    }

            start = juce::jmax (0, start - (int) (reader->sampleRate * 0.005));
            start = SwellRenderer::findNearestZeroCrossing (*buffer, start, (int) (reader->sampleRate * 0.005));
        }

        const int end = juce::jmin (numSamples, start + (int) reader->sampleRate);
        setSelection (start, end);
    }
    else
    {
        requestRender();
    }

    sendChangeMessage();
    return true;
}

std::shared_ptr<const juce::AudioBuffer<float>> ReverseSwellProcessor::getSource() const
{
    const juce::ScopedLock sl (sourceLock);
    return source;
}

double ReverseSwellProcessor::getSourceSampleRate() const
{
    const juce::ScopedLock sl (sourceLock);
    return sourceRate;
}

juce::File ReverseSwellProcessor::getSourceFile() const
{
    const juce::ScopedLock sl (sourceLock);
    return sourceFile;
}

juce::String ReverseSwellProcessor::getSourceDescription() const
{
    const juce::ScopedLock sl (sourceLock);
    if (source == nullptr)
        return "No file loaded";

    const auto seconds = (double) source->getNumSamples() / sourceRate;
    const auto chans   = sourceOriginalChannels == 1 ? juce::String ("Mono")
                       : sourceOriginalChannels == 2 ? juce::String ("Stereo")
                                                     : juce::String (sourceOriginalChannels) + " ch (first 2 used)";

    return sourceFile.getFileName() + "   " + juce::String (seconds, 2) + " s   "
         + juce::String (sourceRate / 1000.0, 1) + " kHz   " + chans;
}

void ReverseSwellProcessor::setSelection (int start, int end)
{
    int total = 0;
    if (auto s = getSource())
        total = s->getNumSamples();

    if (total > 0)
    {
        start = juce::jlimit (0, total - 1, start);
        end   = juce::jlimit (start + 1, total, end);
    }

    selectionStart = start;
    selectionEnd   = end;
    requestRender();
}

std::shared_ptr<const SwellResult> ReverseSwellProcessor::getResult() const
{
    const juce::SpinLock::ScopedLockType sl (resultLock);
    return result;
}

void ReverseSwellProcessor::startPreview()
{
    restartRequested = true;
    previewPlaying   = true;
}

void ReverseSwellProcessor::stopPreview()
{
    previewPlaying = false;
}

//==============================================================================
bool ReverseSwellProcessor::writeWav (const SwellResult& r, const juce::File& file)
{
    file.deleteFile();   // FileOutputStream appends, so start fresh

    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), r.sampleRate,
                                                                          (unsigned int) r.audio.getNumChannels(),
                                                                          24, {}, 0));
    if (writer == nullptr)
        return false;

    stream.release();   // the writer owns the stream now
    return writer->writeFromAudioSampleBuffer (r.audio, 0, r.audio.getNumSamples());
}

bool ReverseSwellProcessor::writeResultToFile (const juce::File& file) const
{
    auto r = getResult();
    return r != nullptr && writeWav (*r, file);
}

juce::File ReverseSwellProcessor::writeTempFileForDrag()
{
    auto r = getResult();
    if (r == nullptr)
        return {};

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ReverseSwell");
    dir.createDirectory();

    auto base = getSourceFile().getFileNameWithoutExtension();
    if (base.isEmpty())
        base = "audio";

    // A new name per render, so DAWs that cache by path never get a stale file
    auto file = dir.getChildFile (base + "_swell_" + juce::String (r->id) + ".wav");

    if (! file.existsAsFile() && ! writeWav (*r, file))
        return {};

    return file;
}

//==============================================================================
void ReverseSwellProcessor::parameterChanged (const juce::String&, float)
{
    requestRender();
}

void ReverseSwellProcessor::requestRender()
{
    ++renderRequests;
    rendering = true;
    renderThread.notify();
}

SwellSettings ReverseSwellProcessor::snapshotSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    SwellSettings s;
    s.swellLengthSec = get (ParamIDs::swellLength);
    s.roomSize       = get (ParamIDs::roomSize);
    s.decaySec       = get (ParamIDs::decay);
    s.damping        = get (ParamIDs::damping);
    s.preDelayMs     = get (ParamIDs::preDelay);
    s.width          = get (ParamIDs::width);
    s.curve          = juce::roundToInt (get (ParamIDs::swellCurve));
    s.swellGainDb    = get (ParamIDs::swellLevel);
    s.dryGainDb      = get (ParamIDs::dryLevel);
    s.includeDry     = get (ParamIDs::includeDry) > 0.5f;
    s.normalize      = get (ParamIDs::normalize) > 0.5f;
    s.selectionOnly  = get (ParamIDs::selectionOnly) > 0.5f;
    return s;
}

void ReverseSwellProcessor::renderLoop()
{
    while (! renderThread.threadShouldExit())
    {
        renderThread.wait (-1);
        if (renderThread.threadShouldExit())
            return;

        // Debounce: wait until requests stop arriving for a moment
        for (;;)
        {
            const int before = renderRequests.load();
            renderThread.wait (debounceMs);
            if (renderThread.threadShouldExit())
                return;
            if (renderRequests.load() == before)
                break;
        }

        performRender();
    }
}

void ReverseSwellProcessor::performRender()
{
    const int requestAtStart = renderRequests.load();

    std::shared_ptr<const juce::AudioBuffer<float>> src;
    double sr;
    {
        const juce::ScopedLock sl (sourceLock);
        src = source;
        sr  = sourceRate;
    }

    if (src == nullptr || src->getNumSamples() == 0)
    {
        rendering = false;
        return;
    }

    auto rendered = SwellRenderer::render (*src, sr, selectionStart.load(), selectionEnd.load(), snapshotSettings(),
                                           [this, requestAtStart]
                                           {
                                               return renderThread.threadShouldExit()
                                                   || renderRequests.load() != requestAtStart;
                                           });

    if (rendered == nullptr)
        return;   // cancelled: a newer request is already queued

    rendered->id = ++renderCounter;
    std::shared_ptr<const SwellResult> newResult (std::move (rendered));

    {
        const juce::SpinLock::ScopedLockType sl (resultLock);
        std::swap (result, newResult);
    }
    newResult.reset();   // old result freed here, never on the audio thread

    if (renderRequests.load() == requestAtStart)
        rendering = false;

    sendChangeMessage();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverseSwellProcessor();
}
