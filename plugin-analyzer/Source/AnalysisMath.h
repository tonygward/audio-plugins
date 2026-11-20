#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

// ANALYZER-01: the measurement itself. No audio-thread code lives here: the Reference plays a known test program, the Result
// captures what arrives, and this turns the capture into a frequency response (gain in decibels against frequency).
//
// The method, in plain terms:
//  - The test program is two identical logarithmic sine sweeps from 20 Hz to 20 kHz (an exponential sweep, as used for
//    measuring loudspeakers and rooms), each followed by a second of silence, after a short silent lead-in.
//  - The Result records everything that arrives. The delay between what was sent and what arrived is found by
//    cross-correlating the capture with the first sweep, so the plugin's latency and the host's delay do not matter.
//  - For each sweep, the gain at each frequency is the size of the received spectrum divided by the size of the sent spectrum.
//    Only the size (the magnitude) is reported, so delay cannot be mistaken for anything else, and phase is not reported.
//  - The two sweeps are compared. If they disagree the plugin is changing with time or level (a compressor, saturation, a
//    modulated effect), and the curve is flagged rather than presented as a static equaliser curve.
//  - The result is smoothed to a sixth of an octave on a logarithmic grid, 20 Hz to 20 kHz.
//
// Limits, stated honestly: this measures the magnitude of a linear, time-invariant response. A nonlinear or level-dependent
// device is measured at the test level only. A tail longer than the one-second gap is cut off.
namespace PluginAnalyzer
{
    struct SweepSpec
    {
        double lowHz = 20.0, highHz = 20000.0;
        double sweepSeconds = 4.0;
        double gapSeconds = 1.0;       // silence after each sweep, which holds the plugin's tail
        double leadInSeconds = 0.25;   // silence before the first sweep: it absorbs a block or two of start-up difference
        double tailSeconds = 1.5;      // extra capture after the program: room for latency
        double fadeSeconds = 0.05;     // each sweep fades in and out so it starts and ends without a click
        int repeats = 2;               // the sweeps compared to see whether the processing changes over time
    };

    inline int samplesFor (double seconds, double fs) { return (int) std::lround (seconds * fs); }
    inline int sweepSamples (const SweepSpec& s, double fs)       { return samplesFor (s.sweepSeconds, fs); }
    inline int repetitionSamples (const SweepSpec& s, double fs)  { return sweepSamples (s, fs) + samplesFor (s.gapSeconds, fs); }
    inline int programSamples (const SweepSpec& s, double fs)     { return samplesFor (s.leadInSeconds, fs) + s.repeats * repetitionSamples (s, fs); }
    inline int captureSamples (const SweepSpec& s, double fs)     { return programSamples (s, fs) + samplesFor (s.tailSeconds, fs); }

    // The top of the sweep stays below the Nyquist limit at low sample rates.
    inline double topFrequency (const SweepSpec& s, double fs) { return std::min (s.highHz, 0.45 * fs); }

    // One exponential sweep at unit amplitude: f(t) = lowHz * exp(t / L), L = T / ln(high / low).
    inline std::vector<float> makeSweep (const SweepSpec& s, double fs)
    {
        const int n = sweepSamples (s, fs);
        const double f1 = s.lowHz, f2 = topFrequency (s, fs), T = s.sweepSeconds;
        const double L = T / std::log (f2 / f1), twoPi = 6.283185307179586476925286766559;
        const int fade = std::max (1, samplesFor (s.fadeSeconds, fs));
        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            double v = std::sin (twoPi * f1 * L * (std::exp (t / L) - 1.0));
            if (i < fade)            v *= 0.5 - 0.5 * std::cos (3.14159265358979323846 * (double) i / (double) fade);
            else if (i >= n - fade)  v *= 0.5 - 0.5 * std::cos (3.14159265358979323846 * (double) (n - 1 - i) / (double) fade);
            out[(size_t) i] = (float) v;
        }
        return out;
    }

    // The whole test program the Reference plays: the lead-in, then each sweep followed by its gap.
    inline std::vector<float> makeProgram (const SweepSpec& s, double fs, float amplitude)
    {
        const auto sweep = makeSweep (s, fs);
        std::vector<float> out ((size_t) programSamples (s, fs), 0.0f);
        const int lead = samplesFor (s.leadInSeconds, fs), rep = repetitionSamples (s, fs);
        for (int r = 0; r < s.repeats; ++r)
            for (size_t i = 0; i < sweep.size(); ++i)
                out[(size_t) (lead + r * rep) + i] = amplitude * sweep[i];
        return out;
    }

    enum Flag : unsigned
    {
        flagNone = 0,
        flagNoSignal = 1u << 0,        // nothing, or almost nothing, reached the Result
        flagLowSignal = 1u << 1,       // it arrived, but very quietly, so the noise floor may show
        flagClipped = 1u << 2,         // the Result's input reached full scale
        flagTimeVarying = 1u << 3,     // the two sweeps disagree: the processing changes with time or level
        flagNotAligned = 1u << 4,      // the capture could not be lined up with the test program
        flagNoValidCurve = 1u << 5     // too few frequencies could be measured
    };

    struct Measurement
    {
        std::vector<float> frequencyHz;    // the log grid, 20 Hz to 20 kHz
        std::vector<float> magnitudeDb;    // gain in dB at each frequency (meaningful where valid is non-zero)
        std::vector<std::uint8_t> valid;   // 1 where the value was measured
        double sampleRate = 0.0;
        double delaySamples = 0.0;         // between what was sent and what arrived
        float capturePeak = 0.0f;          // the highest sample in the capture, linear
        float testLevelDb = 0.0f;          // the level of the test sweep that was sent
        float repeatDifferenceDb = 0.0f;   // RMS difference between the two sweeps, 30 Hz to 18 kHz
        float alignmentQuality = 0.0f;     // how sharp the correlation peak was (higher is better)
        unsigned flags = flagNone;
        int sweeps = 0;

        bool isValidCurve() const noexcept { return (flags & (flagNoSignal | flagNotAligned | flagNoValidCurve)) == 0 && ! frequencyHz.empty(); }
    };

    inline constexpr int gridPoints = 600;
    inline constexpr double gridLowHz = 20.0, gridHighHz = 20000.0;
    inline double gridFrequency (int i) { return gridLowHz * std::pow (gridHighHz / gridLowHz, (double) i / (double) (gridPoints - 1)); }

    namespace detail
    {
        using Complex = std::complex<float>;

        inline int nextPowerOfTwo (int n) { int p = 1; while (p < n) p <<= 1; return p; }

        // Zero-padded forward FFT of 'length' samples starting at 'start' (samples outside the capture count as silence).
        inline void fftOf (const float* data, int dataLength, int start, int length, int fftSize, std::vector<Complex>& out)
        {
            std::vector<Complex> in ((size_t) fftSize, Complex (0.0f, 0.0f));
            for (int i = 0; i < length; ++i)
            {
                const int k = start + i;
                if (k >= 0 && k < dataLength)
                    in[(size_t) i] = Complex (data[k], 0.0f);
            }
            out.assign ((size_t) fftSize, Complex());
            juce::dsp::FFT fft (juce::roundToInt (std::log2 ((double) fftSize)));
            fft.perform (in.data(), out.data(), false);
        }

        // The delay of 'capture' behind 'reference' (both starting where the reference starts), in samples, and how sharp the
        // correlation peak is: its height over the typical size of everything else.
        inline double estimateDelay (const std::vector<float>& capture, int captureLength, const std::vector<float>& reference, int maxLag,
                                     float& quality)
        {
            const int refLen = (int) reference.size();
            const int use = std::min (captureLength, refLen + maxLag);
            const int fftSize = nextPowerOfTwo (use + refLen);
            std::vector<Complex> a, b;
            fftOf (capture.data(), captureLength, 0, use, fftSize, a);
            fftOf (reference.data(), refLen, 0, refLen, fftSize, b);
            for (int i = 0; i < fftSize; ++i)
                a[(size_t) i] *= std::conj (b[(size_t) i]);
            std::vector<Complex> r ((size_t) fftSize);
            juce::dsp::FFT fft (juce::roundToInt (std::log2 ((double) fftSize)));
            fft.perform (a.data(), r.data(), true);

            int best = 0;
            float bestValue = 0.0f;
            double sumSquares = 0.0;
            const int searchEnd = std::min (maxLag, fftSize / 2);
            for (int lag = 0; lag < searchEnd; ++lag)
            {
                const float v = std::abs (r[(size_t) lag]);
                sumSquares += (double) v * (double) v;
                if (v > bestValue) { bestValue = v; best = lag; }
            }
            const double rms = std::sqrt (sumSquares / (double) std::max (1, searchEnd));
            quality = rms > 0.0 ? (float) ((double) bestValue / rms) : 0.0f;

            // Parabolic interpolation around the peak, for a fraction of a sample.
            double frac = 0.0;
            if (best > 0 && best + 1 < searchEnd)
            {
                const double y0 = std::abs (r[(size_t) best - 1]), y1 = std::abs (r[(size_t) best]), y2 = std::abs (r[(size_t) best + 1]);
                const double d = y0 - 2.0 * y1 + y2;
                if (std::abs (d) > 1.0e-12)
                    frac = juce::jlimit (-0.5, 0.5, 0.5 * (y0 - y2) / d);
            }
            return (double) best + frac;
        }
    }

    // Turns one capture (the Result's input, one channel) into a measurement. 'testLevelDb' is the level of the sweep that was sent.
    // Offline: call it on a background thread, never on the audio thread.
    inline Measurement analyse (const std::vector<float>& capture, int captureLength, const SweepSpec& spec, double fs, float testLevelDb)
    {
        Measurement m;
        m.sampleRate = fs;
        m.testLevelDb = testLevelDb;
        m.sweeps = spec.repeats;
        m.frequencyHz.resize (gridPoints);
        m.magnitudeDb.assign (gridPoints, 0.0f);
        m.valid.assign (gridPoints, 0);
        for (int i = 0; i < gridPoints; ++i)
            m.frequencyHz[(size_t) i] = (float) gridFrequency (i);

        float peak = 0.0f;
        for (int i = 0; i < captureLength; ++i)
            peak = std::max (peak, std::abs (capture[(size_t) i]));
        m.capturePeak = peak;
        if (peak < 1.0e-5f)       { m.flags |= flagNoSignal; return m; }
        if (peak < 1.0e-3f)       m.flags |= flagLowSignal;
        if (peak >= 0.999f)       m.flags |= flagClipped;

        const double amplitude = std::pow (10.0, (double) testLevelDb / 20.0);
        const auto sweep = makeSweep (spec, fs);
        const int lead = samplesFor (spec.leadInSeconds, fs), rep = repetitionSamples (spec, fs), sweepLen = (int) sweep.size();

        // 1. Line the capture up with the program: the delay is read from the first sweep.
        std::vector<float> first (sweep.size() + (size_t) lead, 0.0f);
        for (size_t i = 0; i < sweep.size(); ++i)
            first[(size_t) lead + i] = (float) amplitude * sweep[i];
        float quality = 0.0f;
        const int maxLag = std::min (captureLength, lead + samplesFor (spec.gapSeconds + spec.tailSeconds, fs));
        const double delay = detail::estimateDelay (capture, captureLength, first, maxLag, quality);
        m.delaySamples = delay - 0.0;   // 'first' already carries the lead-in, so this is the delay itself
        m.alignmentQuality = quality;
        if (quality < 6.0f)
        {
            m.flags |= flagNotAligned;
            return m;
        }

        // 2. For each sweep, the size of the received spectrum over the size of the sent spectrum.
        const int pre = samplesFor (0.1, fs);
        const int segment = pre + rep;   // from just before this sweep to just before the next one
        const int fftSize = detail::nextPowerOfTwo (segment);
        const double binHz = fs / (double) fftSize;

        std::vector<detail::Complex> sent;
        {
            std::vector<float> scaled (sweep.size());
            for (size_t i = 0; i < sweep.size(); ++i)
                scaled[i] = (float) amplitude * sweep[i];
            detail::fftOf (scaled.data(), sweepLen, 0, sweepLen, fftSize, sent);
        }
        double sentMax = 0.0;
        for (int k = 1; k < fftSize / 2; ++k)
            sentMax = std::max (sentMax, (double) std::norm (sent[(size_t) k]));
        const double sentFloor = sentMax * 1.0e-6;   // 60 dB below the strongest bin: below that the division means nothing

        std::vector<std::vector<double>> power ((size_t) spec.repeats, std::vector<double> ((size_t) gridPoints, 0.0));
        std::vector<std::uint8_t> repValid ((size_t) gridPoints, 1);
        const double topHz = topFrequency (spec, fs);
        const double halfWindow = std::pow (2.0, 1.0 / 12.0);   // a sixth of an octave wide

        for (int r = 0; r < spec.repeats; ++r)
        {
            const int start = (int) std::lround (delay) + lead + r * rep - pre;
            std::vector<detail::Complex> got;
            detail::fftOf (capture.data(), captureLength, start, segment, fftSize, got);

            for (int i = 0; i < gridPoints; ++i)
            {
                const double fc = gridFrequency (i);
                if (fc < spec.lowHz || fc > topHz * 0.98)
                {
                    repValid[(size_t) i] = 0;
                    continue;
                }
                const double lo = fc / halfWindow, hi = fc * halfWindow;
                const int k0 = std::max (1, (int) std::floor (lo / binHz)), k1 = std::min (fftSize / 2 - 1, (int) std::ceil (hi / binHz));
                double sum = 0.0;
                int count = 0;
                for (int k = k0; k <= k1; ++k)
                {
                    const double s2 = (double) std::norm (sent[(size_t) k]);
                    if (s2 <= sentFloor)
                        continue;
                    sum += (double) std::norm (got[(size_t) k]) / s2;
                    ++count;
                }
                if (count == 0)
                    repValid[(size_t) i] = 0;
                else
                    power[(size_t) r][(size_t) i] = sum / (double) count;
            }
        }

        // 3. Average the sweeps, and see how much they disagree.
        int validCount = 0;
        double diffSquares = 0.0;
        int diffCount = 0;
        for (int i = 0; i < gridPoints; ++i)
        {
            if (! repValid[(size_t) i])
                continue;
            double mean = 0.0;
            for (int r = 0; r < spec.repeats; ++r)
                mean += power[(size_t) r][(size_t) i];
            mean /= (double) spec.repeats;
            if (mean <= 0.0)
                continue;
            m.magnitudeDb[(size_t) i] = (float) (10.0 * std::log10 (mean));
            m.valid[(size_t) i] = 1;
            ++validCount;

            const double fc = gridFrequency (i);
            if (spec.repeats >= 2 && fc >= 30.0 && fc <= 18000.0 && power[0][(size_t) i] > 0.0 && power[1][(size_t) i] > 0.0)
            {
                const double d = 10.0 * std::log10 (power[1][(size_t) i] / power[0][(size_t) i]);
                diffSquares += d * d;
                ++diffCount;
            }
        }
        m.repeatDifferenceDb = diffCount > 0 ? (float) std::sqrt (diffSquares / (double) diffCount) : 0.0f;
        if (validCount < gridPoints / 2)
            m.flags |= flagNoValidCurve;
        if (m.repeatDifferenceDb > 0.5f)
            m.flags |= flagTimeVarying;
        return m;
    }

    // One line of plain words for each flag that is set, saying what happened and whether to do anything.
    inline juce::StringArray describeFlags (const Measurement& m)
    {
        juce::StringArray out;
        if (m.flags & flagNoSignal)
            out.add ("Nothing reached the Result. Check that the Reference is before the plugin, the Result is after it, both use the same Session, and the host is playing.");
        if (m.flags & flagNotAligned)
            out.add ("The sound that arrived could not be lined up with the test sweep, so no curve is shown. Check that nothing else is playing on this track and that the plugin is not muting the sound.");
        if (m.flags & flagLowSignal)
            out.add ("The test sweep arrived very quietly, so noise may show in the curve. Raise Test Level, or check the plugin's output level.");
        if (m.flags & flagClipped)
            out.add ("The sound reached the top of the scale at the Result. Lower Test Level, or turn the plugin's output down, and measure again.");
        if (m.flags & flagTimeVarying)
            out.add ("The two sweeps do not match (they differ by " + juce::String (m.repeatDifferenceDb, 1) + " dB). The plugin changes with time or level, "
                     "so this is not a fixed equaliser curve. Turn off any compressor, saturation or movement and measure again.");
        if (m.flags & flagNoValidCurve)
            out.add ("Too little of the frequency range could be measured to show a curve.");
        return out;
    }
}
