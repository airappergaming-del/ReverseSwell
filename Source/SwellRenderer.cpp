#include "SwellRenderer.h"
#include <cmath>
#include <vector>

namespace
{
    struct DelayLine
    {
        std::vector<float> buffer { 0.0f };
        int pos = 0;

        void setLength (int length)
        {
            buffer.assign ((size_t) juce::jmax (1, length), 0.0f);
            pos = 0;
        }

        float read() const noexcept         { return buffer[(size_t) pos]; }

        void write (float v) noexcept
        {
            buffer[(size_t) pos] = v;
            if (++pos >= (int) buffer.size())
                pos = 0;
        }

        float process (float in) noexcept
        {
            const auto out = read();
            write (in);
            return out;
        }
    };

    /** Schroeder allpass used as an input diffuser. */
    struct Allpass
    {
        DelayLine delay;
        float g = 0.7f;

        float process (float x) noexcept
        {
            const auto z = delay.read();
            const auto w = x + g * z;
            const auto y = z - g * w;
            delay.write (w);
            return y;
        }
    };

    /** 8-line feedback delay network with a true RT60 decay control,
        in-loop damping, input diffusion, pre-delay and stereo width. */
    class FdnReverb
    {
    public:
        static constexpr int N = 8;

        void prepare (double sr, const SwellSettings& s)
        {
            static constexpr double baseMs[N] = { 31.3, 37.9, 41.7, 47.3, 53.1, 59.9, 67.3, 73.7 };
            const double sizeScale = 0.35 + 1.4 * (double) s.roomSize;
            const double rt60      = juce::jmax (0.1, s.decaySec);

            for (int i = 0; i < N; ++i)
            {
                const int len = juce::jmax (8, (int) std::round (baseMs[i] * 0.001 * sr * sizeScale));
                lines[i].setLength (len);
                gains[i]   = (float) std::pow (10.0, -3.0 * (double) len / (rt60 * sr));
                lowpass[i] = 0.0f;
            }

            damp  = juce::jlimit (0.0f, 0.92f, s.damping * 0.9f);
            width = juce::jlimit (0.0f, 1.0f, s.width);

            preDelayLength = (int) std::round (s.preDelayMs * 0.001 * sr);
            if (preDelayLength > 0)
            {
                preL.setLength (preDelayLength);
                preR.setLength (preDelayLength);
            }

            static constexpr double apMsL[4] = { 4.77, 3.59, 12.73, 9.31 };
            static constexpr double apMsR[4] = { 5.13, 3.81, 12.11, 8.93 };

            for (int k = 0; k < 4; ++k)
            {
                apL[k].delay.setLength ((int) std::round (apMsL[k] * 0.001 * sr));
                apR[k].delay.setLength ((int) std::round (apMsR[k] * 0.001 * sr));
            }
        }

        void process (float inL, float inR, float& outL, float& outR) noexcept
        {
            if (preDelayLength > 0)
            {
                inL = preL.process (inL);
                inR = preR.process (inR);
            }

            for (int k = 0; k < 4; ++k)
            {
                inL = apL[k].process (inL);
                inR = apR[k].process (inR);
            }

            float taps[N], feedback[N];

            for (int i = 0; i < N; ++i)
            {
                taps[i]     = lines[i].read();
                lowpass[i]  = taps[i] * (1.0f - damp) + lowpass[i] * damp;
                feedback[i] = lowpass[i] * gains[i];
            }

            hadamard (feedback);

            for (int i = 0; i < N; ++i)
                lines[i].write (feedback[i] + ((i & 1) ? inR : inL) * 0.5f);

            static constexpr float signL[N] = { 1, -1,  1,  1, -1,  1, -1, -1 };
            static constexpr float signR[N] = { 1,  1, -1,  1,  1, -1, -1,  1 };

            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                l += taps[i] * signL[i];
                r += taps[i] * signR[i];
            }

            const float mid  = 0.5f * (l + r);
            const float side = 0.5f * (l - r) * width;
            outL = mid + side;
            outR = mid - side;
        }

    private:
        static void hadamard (float* x) noexcept
        {
            for (int h = 1; h < N; h *= 2)
                for (int i = 0; i < N; i += h * 2)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = x[j], b = x[j + h];
                        x[j]     = a + b;
                        x[j + h] = a - b;
                    }

            const float scale = 1.0f / std::sqrt ((float) N);
            for (int i = 0; i < N; ++i)
                x[i] *= scale;
        }

        DelayLine lines[N];
        float gains[N] {}, lowpass[N] {};
        float damp = 0.0f, width = 1.0f;
        int preDelayLength = 0;
        DelayLine preL, preR;
        Allpass apL[4], apR[4];
    };
}

//==============================================================================
float SwellRenderer::curveValue (int curve, float x)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    const float k = 4.0f;

    switch (curve)
    {
        case 1:  return (std::exp (k * x) - 1.0f) / (std::exp (k) - 1.0f);    // exponential: slow start, fast rise
        case 2:  return std::log1p (x * (std::exp (k) - 1.0f)) / k;           // log: fast start, slow rise
        default: return x;                                                    // linear
    }
}

int SwellRenderer::findNearestZeroCrossing (const juce::AudioBuffer<float>& buffer, int position, int radius)
{
    const int n = buffer.getNumSamples();
    if (n < 2)
        return position;

    position = juce::jlimit (0, n - 1, position);

    auto mix = [&] (int i)
    {
        float v = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            v += buffer.getSample (ch, i);
        return v;
    };

    for (int d = 0; d <= radius; ++d)
    {
        for (int sign : { -1, 1 })
        {
            const int i = position + sign * d;
            if (i >= 1 && i < n)
            {
                const float a = mix (i - 1), b = mix (i);
                if ((a <= 0.0f && b > 0.0f) || (a >= 0.0f && b < 0.0f))
                    return i;
            }
            if (d == 0)
                break;
        }
    }

    return position;
}

//==============================================================================
std::unique_ptr<SwellResult> SwellRenderer::render (const juce::AudioBuffer<float>& src,
                                                    double sr,
                                                    int selStart,
                                                    int selEnd,
                                                    const SwellSettings& s,
                                                    const std::function<bool()>& shouldCancel)
{
    juce::ScopedNoDenormals noDenormals;

    const int total = src.getNumSamples();
    if (total <= 0 || sr <= 0.0 || src.getNumChannels() == 0)
        return nullptr;

    selStart = juce::jlimit (0, total - 1, selStart);
    if (selEnd <= selStart || selEnd > total)
        selEnd = total;

    const int srcEnd   = s.selectionOnly ? selEnd : total;
    const int srcLen   = juce::jmax (1, srcEnd - selStart);
    const int tailLen  = juce::jmax (1, (int) std::round (s.swellLengthSec * sr));
    const int fadeLen  = juce::jmax (1, juce::jmin (srcLen, (int) std::round (0.03 * sr)));  // crossfade into the dry hit
    const int swellLen = tailLen + fadeLen;
    const int wetLen   = srcLen + tailLen;           // reversed source followed by silence for the tail
    const int keepFrom = wetLen - swellLen;          // we only keep the part that becomes the pre-hit swell

    const float* inL = src.getReadPointer (0);
    const float* inR = src.getReadPointer (src.getNumChannels() > 1 ? 1 : 0);

    juce::AudioBuffer<float> swell (2, swellLen);
    swell.clear();

    // 1) reverse the source, 2) run it through the reverb 100% wet,
    // 3) write the output reversed again so the tail turns into a rising swell
    FdnReverb reverb;
    reverb.prepare (sr, s);

    float* swL = swell.getWritePointer (0);
    float* swR = swell.getWritePointer (1);

    for (int t = 0; t < wetLen; ++t)
    {
        if ((t & 16383) == 0 && shouldCancel && shouldCancel())
            return nullptr;

        float xl = 0.0f, xr = 0.0f;
        if (t < srcLen)
        {
            const int idx = selStart + (srcLen - 1 - t);
            xl = inL[idx];
            xr = inR[idx];
        }

        float yl, yr;
        reverb.process (xl, xr, yl, yr);

        if (t >= keepFrom)
        {
            const int k = wetLen - 1 - t;   // reverse back
            swL[k] = yl;
            swR[k] = yr;
        }
    }

    // Fade-in curve over the swell, short cosine fade-out right after the hit
    for (int k = 0; k < swellLen; ++k)
    {
        float env;
        if (k < tailLen)
            env = curveValue (s.curve, (float) k / (float) tailLen);
        else
            env = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (float) (k - tailLen) / (float) fadeLen));

        swL[k] *= env;
        swR[k] *= env;
    }

    // Level-match the swell to the source peak, then apply Swell Level
    float srcPeak = 0.0f;
    for (int ch = 0; ch < src.getNumChannels(); ++ch)
        srcPeak = juce::jmax (srcPeak, src.getMagnitude (ch, selStart, srcLen));

    const float swellPeak = swell.getMagnitude (0, swellLen);
    const float swellGain = juce::Decibels::decibelsToGain (s.swellGainDb, -59.95f);

    if (swellPeak > 1.0e-9f)
        swell.applyGain (juce::jmax (srcPeak, 1.0e-3f) / swellPeak * swellGain);

    auto result = std::make_unique<SwellResult>();
    result->sampleRate = sr;

    if (s.includeDry)
    {
        // Line the swell up so its end lands exactly on the hit (selection start)
        const int offset   = juce::jmax (0, tailLen - selStart);
        const int swellPos = offset + selStart - tailLen;
        const int outLen   = juce::jmax (offset + total, swellPos + swellLen);

        result->audio.setSize (2, outLen);
        result->audio.clear();

        const float dryGain = juce::Decibels::decibelsToGain (s.dryGainDb, -59.95f);
        if (dryGain > 0.0f)
            for (int ch = 0; ch < 2; ++ch)
                result->audio.addFrom (ch, offset, src, juce::jmin (ch, src.getNumChannels() - 1), 0, total, dryGain);

        for (int ch = 0; ch < 2; ++ch)
            result->audio.addFrom (ch, swellPos, swell, ch, 0, swellLen);

        result->swellStart  = swellPos;
        result->hitPosition = offset + selStart;
    }
    else
    {
        result->audio.makeCopyOf (swell);
        result->swellStart  = 0;
        result->hitPosition = tailLen;
    }

    if (s.normalize)
    {
        const float peak = result->audio.getMagnitude (0, result->audio.getNumSamples());
        if (peak > 1.0e-6f)
            result->audio.applyGain (0.966f / peak);   // -0.3 dBFS
    }

    return result;
}
