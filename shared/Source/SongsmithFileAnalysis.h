#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include "SongsmithKWeighting.h"

// REC-01: offline whole-file analysis for the standalone Loudness Analyser.
//
// Everything here runs on a user-selected file, on a background thread, never in an audio callback.
// It reads the file once, in blocks, and never writes, resamples or changes the original.
//
// Real measurements: file format, sample rate, stored bit depth, channels, duration, BS.1770-4
// integrated loudness (absolute gate -70 LUFS, relative gate -10 LU), EBU Tech 3342 loudness range
// (3 s windows, relative gate -20 LU), sample peak, and true peak (4x, or 2x from 96 kHz, Kaiser
// interpolation, a different design from the Maximiser's limiter detector).
// Not knowable from a file: the bit depth or sample rate it was originally recorded at.
namespace Songsmith::FileAnalysis
{
    struct Result
    {
        // File facts.
        bool opened = false;
        juce::String error;             // plain words, set when opened is false or reading failed
        juce::String formatName;        // "WAV", "AIFF" or "FLAC"
        double sampleRate = 0.0;
        int channels = 0;
        int storedBits = 0;             // bits per sample stored in the file; 0 if unknown
        bool floatingPoint = false;
        juce::int64 frames = 0;
        double durationSeconds = 0.0;

        // Where known: the lowest bits are all zero, so the values fit in fewer bits (integer files).
        int bitsInUse = 0;              // 0 when unknown or the file is silent

        // Loudness (each has a "has" flag: a missing value is never invented).
        bool hasIntegratedLoudness = false;
        double integratedLufs = 0.0;
        bool hasLoudnessRange = false;
        double loudnessRange = 0.0;     // dB (loudness units)

        // Peaks, in dBFS / dBTP; -infinity for silence.
        double samplePeakDb = -std::numeric_limits<double>::infinity();
        double truePeakDb   = -std::numeric_limits<double>::infinity();

        // Possible clipping.
        int clippedRuns = 0;            // runs of 3 or more samples at the file's ceiling
        int longestClippedRun = 0;
        juce::int64 samplesAboveFullScale = 0;   // floating point files only

        bool silent = false;            // every sample is exactly zero
        bool cancelled = false;
    };

    namespace detail
    {
        inline std::vector<float> designInterpolator (int factor, int tapsPerPhase, double beta)
        {
            const int length = factor * tapsPerPhase;
            const double centre = (double) length * 0.5, pi = 3.14159265358979323846;
            auto i0 = [] (double x)
            {
                double sum = 1.0, term = 1.0;
                for (int k = 1; k < 60; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
                return sum;
            };
            std::vector<float> h ((size_t) length);
            for (int m = 0; m < length; ++m)
            {
                const double t = ((double) m - centre) / (double) factor;
                const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
                const double r = ((double) m - centre) / centre;
                h[(size_t) m] = (float) (sinc * i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta));
            }
            for (int q = 0; q < factor; ++q)   // unity gain at DC in every phase
            {
                double sum = 0.0;
                for (int k = 0; k < tapsPerPhase; ++k) sum += h[(size_t) (k * factor + q)];
                for (int k = 0; k < tapsPerPhase; ++k) h[(size_t) (k * factor + q)] = (float) (h[(size_t) (k * factor + q)] / sum);
            }
            return h;
        }

        // Streaming true-peak reader for one channel. 'peak' includes the filter's tail once flush() is called.
        class TruePeak
        {
        public:
            static constexpr int taps = 24, ring = 32;
            void prepare (double fs)
            {
                factor = fs >= 96000.0 ? 2 : 4;
                coeffs = designInterpolator (factor, taps, 8.0);
                hist.fill (0.0f); pos = 0; peak = 0.0f;
            }
            void push (float x) noexcept
            {
                pos = (pos + 1) & (ring - 1);
                hist[(size_t) pos] = x;
                // Phase q of the newest sample estimates the point q/factor after the sample 'taps/2' ago.
                for (int q = 0; q < factor; ++q)
                {
                    float acc = 0.0f;
                    for (int k = 0; k < taps; ++k)
                        acc += coeffs[(size_t) (k * factor + q)] * hist[(size_t) ((pos - k) & (ring - 1))];
                    peak = std::max (peak, std::abs (acc));
                }
            }
            void flush() noexcept { for (int i = 0; i < taps; ++i) push (0.0f); }
            float peakValue() const noexcept { return peak; }
        private:
            int factor = 4, pos = 0;
            float peak = 0.0f;
            std::vector<float> coeffs;
            std::array<float, ring> hist {};
        };

        inline double lufsFromMeanSquare (double ms) { return ms > 0.0 ? -0.691 + 10.0 * std::log10 (ms) : -std::numeric_limits<double>::infinity(); }

        // Gated mean of window loudness values. Returns false when nothing passes the absolute gate.
        inline bool gatedLoudness (const std::vector<double>& meanSquares, double relativeGateLu, double& outLufs, std::vector<double>* kept = nullptr)
        {
            auto average = [&] (double floorLufs, double& ms)
            {
                double sum = 0.0; size_t n = 0;
                for (auto v : meanSquares)
                    if (lufsFromMeanSquare (v) > floorLufs) { sum += v; ++n; }
                if (n == 0) return false;
                ms = sum / (double) n;
                return true;
            };
            double ms = 0.0;
            if (! average (-70.0, ms)) return false;
            const double gate = lufsFromMeanSquare (ms) + relativeGateLu;
            double ms2 = 0.0;
            if (! average (gate, ms2)) return false;
            outLufs = lufsFromMeanSquare (ms2);
            if (kept != nullptr)
                for (auto v : meanSquares)
                    if (lufsFromMeanSquare (v) > -70.0 && lufsFromMeanSquare (v) > gate) kept->push_back (lufsFromMeanSquare (v));
            return true;
        }
    }

    // BS.1770 channel weights: left, right and centre 1.0; the two surround channels of a six-channel
    // file 1.41; the low-frequency channel (fourth of six) is left out. Other layouts: all 1.0.
    inline double channelWeight (int channel, int numChannels)
    {
        if (numChannels == 6) return channel == 3 ? 0.0 : (channel >= 4 ? 1.41253754 : 1.0);
        return 1.0;
    }

    inline bool isSupportedExtension (const juce::File& f)
    {
        const auto e = f.getFileExtension().toLowerCase();
        return e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac";
    }

    // Analyses already-open audio. 'read' supplies the samples; used by analyseFile and by tests.
    // Offline only: call from a background thread. 'cancel' may be null.
    inline Result analyseReader (juce::AudioFormatReader& reader, const std::atomic<bool>* cancel = nullptr)
    {
        Result r;
        r.opened = true;
        r.formatName = reader.getFormatName().containsIgnoreCase ("flac") ? "FLAC"
                     : reader.getFormatName().containsIgnoreCase ("aiff") ? "AIFF" : "WAV";
        r.sampleRate = reader.sampleRate;
        r.channels = (int) reader.numChannels;
        r.storedBits = (int) reader.bitsPerSample;
        r.floatingPoint = reader.usesFloatingPointData;
        r.frames = reader.lengthInSamples;
        r.durationSeconds = r.sampleRate > 0.0 ? (double) r.frames / r.sampleRate : 0.0;

        if (r.channels < 1 || r.sampleRate < 8000.0 || r.frames < 1)
        {
            r.opened = false;
            r.error = "This file has no audio that can be read.";
            return r;
        }

        const int nch = r.channels;
        std::vector<Songsmith::KWeighting::Filter> filters ((size_t) nch);
        std::vector<detail::TruePeak> peaks ((size_t) nch);
        for (auto& f : filters) f.prepare (r.sampleRate);
        for (auto& p : peaks) p.prepare (r.sampleRate);

        const int subLen = (int) std::lround (r.sampleRate * 0.1);   // 100 ms steps
        std::vector<double> subSums;                                 // weighted mean square per 100 ms
        double acc = 0.0; int accCount = 0;

        juce::AudioBuffer<float> buf (nch, 16384);
        std::vector<int*> intPtrs ((size_t) nch);
        std::vector<std::vector<int>> ints ((size_t) nch, std::vector<int> (16384));
        for (int c = 0; c < nch; ++c) intPtrs[(size_t) c] = ints[(size_t) c].data();

        unsigned int orBits = 0;
        std::vector<int> run ((size_t) nch, 0);
        const double intCeiling = 1.0 - 1.0 / std::pow (2.0, juce::jmax (8, r.storedBits) - 1);
        const double clipLevel = r.floatingPoint ? 1.0 : intCeiling;
        double samplePeak = 0.0;

        for (juce::int64 pos = 0; pos < r.frames;)
        {
            if (cancel != nullptr && cancel->load()) { r.cancelled = true; return r; }
            const int n = (int) std::min<juce::int64> (16384, r.frames - pos);
            // Integer files arrive as left-aligned 32-bit integers; floating point files arrive as the
            // bit pattern of each float. Either way every channel is read, whatever the layout.
            const bool ok = reader.readSamples (intPtrs.data(), nch, 0, pos, n);
            if (ok)
                for (int c = 0; c < nch; ++c)
                {
                    auto* d = buf.getWritePointer (c);
                    for (int i = 0; i < n; ++i)
                    {
                        const int v = ints[(size_t) c][(size_t) i];
                        if (r.floatingPoint)
                            std::memcpy (&d[i], &v, sizeof (float));
                        else
                        {
                            orBits |= (unsigned int) v;
                            d[i] = (float) ((double) v / 2147483648.0);
                        }
                    }
                }
            if (! ok) { r.opened = false; r.error = "This file could not be read all the way through. It may be damaged or cut short."; return r; }

            for (int i = 0; i < n; ++i)
            {
                double weighted = 0.0;
                for (int c = 0; c < nch; ++c)
                {
                    const float x = buf.getReadPointer (c)[i];
                    const double ax = std::abs ((double) x);
                    if (! std::isfinite (ax)) { r.opened = false; r.error = "This file contains values that are not valid audio."; return r; }
                    samplePeak = std::max (samplePeak, ax);
                    if (r.floatingPoint && ax > 1.0) ++r.samplesAboveFullScale;
                    if (ax >= clipLevel) { if (++run[(size_t) c] == 3) ++r.clippedRuns; r.longestClippedRun = std::max (r.longestClippedRun, run[(size_t) c]); }
                    else run[(size_t) c] = 0;
                    peaks[(size_t) c].push (x);
                    const double w = channelWeight (c, nch);
                    const double y = filters[(size_t) c].process ((double) x);
                    weighted += w * y * y;
                }
                acc += weighted;
                if (++accCount == subLen) { subSums.push_back (acc / subLen); acc = 0.0; accCount = 0; }
            }
            pos += n;
        }
        for (auto& p : peaks) p.flush();

        r.silent = samplePeak == 0.0;
        if (! r.silent)
        {
            r.samplePeakDb = 20.0 * std::log10 (samplePeak);
            float tp = 0.0f;
            for (auto& p : peaks) tp = std::max (tp, p.peakValue());
            r.truePeakDb = 20.0 * std::log10 (std::max ((double) tp, samplePeak));
            if (! r.floatingPoint && orBits != 0)
            {
                int trailing = 0;
                while (((orBits >> trailing) & 1u) == 0u) ++trailing;
                r.bitsInUse = 32 - trailing;
            }
        }

        // Integrated: 400 ms windows every 100 ms (four steps); needs at least one full window.
        std::vector<double> momentary, shortTerm;
        for (size_t i = 4; i <= subSums.size(); ++i)
        {
            double s = 0.0; for (size_t k = i - 4; k < i; ++k) s += subSums[k];
            momentary.push_back (s / 4.0);
        }
        for (size_t i = 30; i <= subSums.size(); ++i)
        {
            double s = 0.0; for (size_t k = i - 30; k < i; ++k) s += subSums[k];
            shortTerm.push_back (s / 30.0);
        }
        double lufs = 0.0;
        if (detail::gatedLoudness (momentary, -10.0, lufs)) { r.hasIntegratedLoudness = true; r.integratedLufs = lufs; }

        std::vector<double> kept;
        double ignored = 0.0;
        if (detail::gatedLoudness (shortTerm, -20.0, ignored, &kept) && kept.size() >= 2)
        {
            std::sort (kept.begin(), kept.end());
            auto pct = [&] (double p) { return kept[(size_t) std::lround (p * (double) (kept.size() - 1))]; };
            r.hasLoudnessRange = true;
            r.loudnessRange = pct (0.95) - pct (0.10);
        }
        return r;
    }

    // Opens and analyses a WAV, AIFF or FLAC file. Anything else, or anything damaged, comes back
    // with opened = false and a plain-words reason.
    inline Result analyseFile (const juce::File& file, const std::atomic<bool>* cancel = nullptr)
    {
        Result r;
        if (! file.existsAsFile()) { r.error = "That file could not be found."; return r; }
        if (! isSupportedExtension (file)) { r.error = "Only WAV, AIFF and FLAC files can be analysed."; return r; }

        juce::AudioFormatManager manager;
        manager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (file));
        if (reader == nullptr) { r.error = "This file could not be opened. It may be damaged, or not the kind of file its name suggests."; return r; }
        return analyseReader (*reader, cancel);
    }
}
