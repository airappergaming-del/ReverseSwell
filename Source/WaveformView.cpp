#include "WaveformView.h"
#include "SwellLookAndFeel.h"
#include "SwellRenderer.h"

using namespace juce;

namespace
{
    const StringArray audioExtensions { ".wav", ".wave", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a", ".caf", ".bwf" };
    constexpr float handleGrab = 7.0f;
}

WaveformView::WaveformView (Mode m) : mode (m)
{
    setOpaque (false);
}

//==============================================================================
void WaveformView::setAudio (std::shared_ptr<const AudioBuffer<float>> newAudio, double newRate, bool resetZoom)
{
    const int oldLength = numSamples();
    audio      = std::move (newAudio);
    sampleRate = newRate > 0.0 ? newRate : 44100.0;
    buildPeaks();

    if (resetZoom || oldLength != numSamples())
    {
        viewStart  = 0.0;
        viewLength = jmax (1.0, (double) numSamples());
    }

    clampView();
    repaint();
}

void WaveformView::clearAudio()
{
    audio.reset();
    peakMin.clear();
    peakMax.clear();
    playhead = -1.0;
    repaint();
}

void WaveformView::setSelection (int start, int end)
{
    if (dragMode != DragMode::none)
        return;   // don't fight the user's drag

    if (start != selStart || end != selEnd)
    {
        selStart = start;
        selEnd   = end;
        repaint();
    }
}

void WaveformView::setRegion (int swellStart, int hit)
{
    regionStart = swellStart;
    regionHit   = hit;
    repaint();
}

void WaveformView::setPlayhead (double samplePosition)
{
    if (std::abs (samplePosition - playhead) > 0.5)
    {
        playhead = samplePosition;
        repaint();
    }
}

void WaveformView::setBusy (bool isBusy)
{
    if (busy != isBusy)
    {
        busy = isBusy;
        repaint();
    }
}

//==============================================================================
void WaveformView::buildPeaks()
{
    peakMin.clear();
    peakMax.clear();

    if (! hasAudio())
        return;

    const int n     = numSamples();
    const int bins  = (n + binSize - 1) / binSize;
    const int chans = audio->getNumChannels();
    peakMin.assign ((size_t) bins, 0.0f);
    peakMax.assign ((size_t) bins, 0.0f);

    for (int b = 0; b < bins; ++b)
    {
        const int s0 = b * binSize;
        const int s1 = jmin (n, s0 + binSize);
        float mn = 0.0f, mx = 0.0f;

        for (int ch = 0; ch < chans; ++ch)
        {
            const auto range = FloatVectorOperations::findMinAndMax (audio->getReadPointer (ch, s0), s1 - s0);
            mn = jmin (mn, range.getStart());
            mx = jmax (mx, range.getEnd());
        }

        peakMin[(size_t) b] = mn;
        peakMax[(size_t) b] = mx;
    }
}

void WaveformView::getMinMax (int start, int end, float& mn, float& mx) const
{
    mn = 0.0f;
    mx = 0.0f;

    if (end - start >= binSize * 2)
    {
        const int b0 = start / binSize;
        const int b1 = jmin ((int) peakMin.size() - 1, (end - 1) / binSize);
        for (int b = b0; b <= b1; ++b)
        {
            mn = jmin (mn, peakMin[(size_t) b]);
            mx = jmax (mx, peakMax[(size_t) b]);
        }
        return;
    }

    for (int ch = 0; ch < audio->getNumChannels(); ++ch)
    {
        const auto range = FloatVectorOperations::findMinAndMax (audio->getReadPointer (ch, start), end - start);
        mn = jmin (mn, range.getStart());
        mx = jmax (mx, range.getEnd());
    }
}

//==============================================================================
Rectangle<float> WaveformView::waveArea() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 10.0f).withTrimmedTop (16.0f);
}

double WaveformView::xToSample (float x) const
{
    const auto wa = waveArea();
    return viewStart + (double) (x - wa.getX()) / (double) jmax (1.0f, wa.getWidth()) * viewLength;
}

float WaveformView::sampleToX (double s) const
{
    const auto wa = waveArea();
    return wa.getX() + (float) ((s - viewStart) / viewLength) * wa.getWidth();
}

int WaveformView::clampSample (double s) const
{
    return jlimit (0, jmax (0, numSamples() - 1), (int) std::round (s));
}

double WaveformView::minViewLength() const
{
    return jmax (32.0, sampleRate * 0.005);
}

void WaveformView::clampView()
{
    const double n = jmax (1.0, (double) numSamples());
    viewLength = jlimit (jmin (minViewLength(), n), n, viewLength);
    viewStart  = jlimit (0.0, n - viewLength, viewStart);
}

String WaveformView::formatTime (double samples) const
{
    return String (samples / sampleRate, 2) + " s";
}

//==============================================================================
void WaveformView::paintFrame (Graphics& g, Rectangle<float> b)
{
    if (dragHover)
    {
        // soft purple glow while a file is hovering
        for (int i = 3; i >= 0; --i)
        {
            g.setColour (Palette::accent.withAlpha (0.10f + 0.05f * (float) (3 - i)));
            g.drawRoundedRectangle (b.reduced (1.0f), 8.0f, 2.0f + (float) i * 3.0f);
        }
        g.setColour (Palette::highlight);
        g.drawRoundedRectangle (b.reduced (1.0f), 8.0f, 1.5f);
    }
    else
    {
        g.setColour (Palette::border);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }
}

void WaveformView::paintEmpty (Graphics& g, Rectangle<float> b)
{
    if (mode == Mode::input)
    {
        Path outline;
        outline.addRoundedRectangle (b.reduced (10.0f), 8.0f);
        const float dashes[] = { 6.0f, 5.0f };
        Path dashed;
        PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (Palette::accent.withAlpha (dragHover ? 1.0f : 0.6f));
        g.fillPath (dashed);

        // small "drop" arrow icon
        const auto c = b.getCentre().translated (0.0f, -26.0f);
        Path arrow;
        arrow.startNewSubPath (c.x, c.y - 10.0f);
        arrow.lineTo (c.x, c.y + 8.0f);
        arrow.startNewSubPath (c.x - 7.0f, c.y + 1.0f);
        arrow.lineTo (c.x, c.y + 8.0f);
        arrow.lineTo (c.x + 7.0f, c.y + 1.0f);
        arrow.startNewSubPath (c.x - 12.0f, c.y + 14.0f);
        arrow.lineTo (c.x + 12.0f, c.y + 14.0f);
        g.setColour (Palette::highlight);
        g.strokePath (arrow, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

        g.setColour (Palette::text);
        g.setFont (SwellLookAndFeel::font (18.0f, true));
        g.drawText ("Drop audio here", b.withTrimmedTop (b.getHeight() * 0.5f - 6.0f).removeFromTop (24.0f),
                    Justification::centred);
        g.setColour (Palette::textDim);
        g.setFont (SwellLookAndFeel::font (12.0f));
        g.drawText ("WAV  /  AIFF  /  FLAC  /  MP3   -   or click Load",
                    b.withTrimmedTop (b.getHeight() * 0.5f + 20.0f).removeFromTop (18.0f), Justification::centred);
    }
    else
    {
        g.setColour (Palette::textDim);
        g.setFont (SwellLookAndFeel::font (14.0f));
        g.drawText (busy ? "Rendering..." : "Your swell will appear here", b, Justification::centred);
    }
}

void WaveformView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (b.reduced (0.5f), 8.0f);

    if (! hasAudio())
    {
        paintEmpty (g, b);
        paintFrame (g, b);
        return;
    }

    const auto wa   = waveArea();
    const int n     = numSamples();
    const float mid = wa.getCentreY();
    const float halfH = wa.getHeight() * 0.5f * 0.94f;

    g.saveState();
    g.reduceClipRegion (wa.toNearestInt());

    // selection fill (input)
    if (mode == Mode::input && selEnd > selStart)
    {
        const float x0 = sampleToX (selStart), x1 = sampleToX (selEnd);
        g.setColour (Palette::deep.withAlpha (0.55f));
        g.fillRect (Rectangle<float>::leftTopRightBottom (x0, wa.getY(), x1, wa.getBottom()));
    }

    // centre line
    g.setColour (Palette::border);
    g.fillRect (wa.getX(), mid - 0.5f, wa.getWidth(), 1.0f);

    // waveform columns
    const double spp = viewLength / (double) jmax (1.0f, wa.getWidth());
    const int xStart = (int) std::floor (wa.getX());
    const int xEnd   = (int) std::ceil (wa.getRight());

    for (int x = xStart; x < xEnd; ++x)
    {
        const double sA = viewStart + (double) ((float) x - wa.getX()) * spp;
        int a = (int) sA;
        int e = jmax (a + 1, (int) (sA + spp));
        if (a >= n) break;
        if (e <= 0) continue;
        a = jmax (0, a);
        e = jmin (e, n);

        float mn, mx;
        getMinMax (a, e, mn, mx);

        Colour c;
        if (mode == Mode::input)
            c = (a >= selStart && a < selEnd) ? Palette::highlight : Palette::wave;
        else
            c = (a >= regionStart && a < regionHit) ? Palette::accent : Palette::wave;

        g.setColour (c);
        const float top = mid - mx * halfH;
        const float bottom = mid - mn * halfH;
        g.fillRect ((float) x, top, 1.0f, jmax (1.0f, bottom - top));
    }

    // handles (input)
    if (mode == Mode::input && selEnd > selStart)
    {
        for (const int s : { selStart, selEnd })
        {
            const float hx = sampleToX (s);
            g.setColour (Palette::highlight);
            g.fillRect (hx - 1.0f, wa.getY(), 2.0f, wa.getHeight());
            g.fillRoundedRectangle (hx - 5.0f, wa.getY(), 10.0f, 12.0f, 3.0f);
        }
    }

    // hit marker (output)
    if (mode == Mode::output && regionHit >= 0)
    {
        const float hx = sampleToX (regionHit);
        Path line;
        line.startNewSubPath (hx, wa.getY());
        line.lineTo (hx, wa.getBottom());
        const float dashes[] = { 3.0f, 3.0f };
        Path dashed;
        PathStrokeType (1.0f).createDashedStroke (dashed, line, dashes, 2);
        g.setColour (Palette::highlight.withAlpha (0.8f));
        g.fillPath (dashed);
    }

    // playhead
    if (playhead >= 0.0)
    {
        const float px = sampleToX (playhead);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.fillRect (px - 0.75f, wa.getY(), 1.5f, wa.getHeight());
    }

    g.restoreState();

    // top info strip
    const auto strip = getLocalBounds().toFloat().reduced (12.0f, 6.0f).removeFromTop (16.0f);
    g.setFont (SwellLookAndFeel::font (11.0f));
    g.setColour (Palette::textDim);
    g.drawText (formatTime (viewStart), strip, Justification::centredLeft);

    if (mode == Mode::input)
    {
        g.drawText (formatTime (viewStart + viewLength), strip, Justification::centredRight);
        if (selEnd > selStart)
        {
            g.setColour (Palette::highlight);
            g.drawText ("Selection  " + formatTime (selStart) + "  ->  " + formatTime (selEnd)
                          + "   (" + formatTime (selEnd - selStart) + ")",
                        strip, Justification::centred);
        }
    }
    else
    {
        g.setColour (Palette::highlight);
        g.drawText ("Swell " + formatTime (jmax (0, regionHit - regionStart)) + "    Total " + formatTime (n),
                    strip, Justification::centred);

        // "drag me" hint: grip dots + label, top right
        auto hint = strip.withTrimmedLeft (strip.getWidth() - 110.0f);
        g.setColour (Palette::text.withAlpha (isMouseOver() ? 1.0f : 0.7f));
        g.drawText (busy ? "RENDERING..." : "DRAG TO DAW", hint.withTrimmedLeft (16.0f), Justification::centredRight);
        g.setColour (Palette::accent);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col)
                g.fillEllipse (hint.getX() + 2.0f + (float) col * 5.0f, hint.getCentreY() - 6.0f + (float) row * 5.0f, 2.6f, 2.6f);
    }

    paintFrame (g, b);
}

//==============================================================================
void WaveformView::mouseDown (const MouseEvent& e)
{
    dragOutStarted = false;
    if (! hasAudio() || mode == Mode::output)
        return;

    prevSelStart = selStart;
    prevSelEnd   = selEnd;

    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        dragMode = DragMode::pan;
        panAnchorStart = viewStart;
        return;
    }

    const float xs = sampleToX (selStart), xe = sampleToX (selEnd);

    if (selEnd > selStart && std::abs (e.position.x - xs) < handleGrab)
        dragMode = DragMode::startHandle;
    else if (selEnd > selStart && std::abs (e.position.x - xe) < handleGrab)
        dragMode = DragMode::endHandle;
    else
    {
        dragMode   = DragMode::newSelection;
        dragAnchor = clampSample (xToSample (e.position.x));
        selStart = selEnd = dragAnchor;
    }

    repaint();
}

void WaveformView::mouseDrag (const MouseEvent& e)
{
    if (! hasAudio())
        return;

    if (mode == Mode::output)
    {
        if (! dragOutStarted && e.getDistanceFromDragStart() > 6 && onDragOut != nullptr)
        {
            dragOutStarted = true;
            onDragOut();
        }
        return;
    }

    const int s = clampSample (xToSample (e.position.x));

    switch (dragMode)
    {
        case DragMode::newSelection:
            selStart = jmin (dragAnchor, s);
            selEnd   = jmax (dragAnchor, s);
            break;
        case DragMode::startHandle:
            selStart = jmin (s, selEnd - 1);
            break;
        case DragMode::endHandle:
            selEnd = jmax (s, selStart + 1);
            break;
        case DragMode::pan:
            viewStart = panAnchorStart - (double) e.getDistanceFromDragStartX() / (double) waveArea().getWidth() * viewLength;
            clampView();
            break;
        case DragMode::none:
            break;
    }

    repaint();
}

void WaveformView::mouseUp (const MouseEvent&)
{
    if (mode == Mode::output || ! hasAudio() || dragMode == DragMode::none || dragMode == DragMode::pan)
    {
        dragMode = DragMode::none;
        return;
    }

    const int n = numSamples();

    // A plain click moves the start of the selection there and keeps its length
    if (dragMode == DragMode::newSelection && selEnd - selStart < 2)
    {
        const int len = jmax (1, prevSelEnd - prevSelStart);
        selStart = dragAnchor;
        selEnd   = jmin (n, selStart + len);
    }

    if (snap)
    {
        const int radius = (int) (sampleRate * 0.01);
        selStart = SwellRenderer::findNearestZeroCrossing (*audio, selStart, radius);
        selEnd   = SwellRenderer::findNearestZeroCrossing (*audio, selEnd, radius);
    }

    selStart = jlimit (0, n - 1, selStart);
    selEnd   = jlimit (selStart + 1, n, selEnd);

    dragMode = DragMode::none;
    repaint();

    if (onSelectionChanged != nullptr)
        onSelectionChanged (selStart, selEnd);
}

void WaveformView::mouseMove (const MouseEvent& e)
{
    if (! hasAudio())
    {
        setMouseCursor (MouseCursor::NormalCursor);
        return;
    }

    if (mode == Mode::output)
    {
        setMouseCursor (MouseCursor::DraggingHandCursor);
        repaint();
        return;
    }

    const bool nearHandle = selEnd > selStart
                         && (std::abs (e.position.x - sampleToX (selStart)) < handleGrab
                             || std::abs (e.position.x - sampleToX (selEnd)) < handleGrab);
    setMouseCursor (nearHandle ? MouseCursor::LeftRightResizeCursor : MouseCursor::IBeamCursor);
}

void WaveformView::mouseDoubleClick (const MouseEvent&)
{
    viewStart  = 0.0;
    viewLength = (double) numSamples();
    clampView();
    repaint();
}

void WaveformView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (! hasAudio())
        return;

    const auto wa = waveArea();
    const bool horizontal = std::abs (wheel.deltaX) > std::abs (wheel.deltaY);

    if (horizontal || e.mods.isShiftDown())
    {
        const float d = horizontal ? wheel.deltaX : wheel.deltaY;
        viewStart -= (double) d * viewLength * 0.5;
    }
    else
    {
        const double anchor = xToSample (e.position.x);
        const double rel    = (double) ((e.position.x - wa.getX()) / jmax (1.0f, wa.getWidth()));
        const double factor = std::pow (2.0, (double) -wheel.deltaY * (wheel.isReversed ? -2.0 : 2.0));
        viewLength *= factor;
        clampView();
        viewStart = anchor - rel * viewLength;
    }

    clampView();
    repaint();
}

//==============================================================================
bool WaveformView::isInterestedInFileDrag (const StringArray& files)
{
    if (mode != Mode::input)
        return false;

    for (auto& f : files)
        for (auto& ext : audioExtensions)
            if (f.endsWithIgnoreCase (ext))
                return true;

    return false;
}

void WaveformView::fileDragEnter (const StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void WaveformView::fileDragExit (const StringArray&)
{
    dragHover = false;
    repaint();
}

void WaveformView::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    repaint();

    for (auto& f : files)
        for (auto& ext : audioExtensions)
            if (f.endsWithIgnoreCase (ext))
            {
                if (onFileDropped != nullptr)
                    onFileDropped (File (f));
                return;
            }
}
