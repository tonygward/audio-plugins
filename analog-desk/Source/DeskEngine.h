#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>

// Songsmith Analog Desk engine (DESK-01): one channel of a classic SSL-style console strip. Which behaviours follow published
// descriptions of the SSL 4000 E channel, which are Songsmith design choices, and the test that checks each, are written down in
// analog-desk/docs/engine.md.
//
//   input trim -> drive -> high-pass and low-pass filters -> [dynamics] -> four-band equaliser -> [dynamics] -> output trim
//
// The dynamics (a gate or expander, then a compressor) sit before or after the equaliser, as the strip's own switch does. The EQ
// and the dynamics each have their own bypass. The compressor and the gate follow one detector for every channel, so the stereo
// image holds. No latency, no look-ahead. process() does not allocate, lock or log; prepare() and reset() are for the message thread.
namespace SongsmithDesk
{
    // --- Ranges ---------------------------------------------------------------------------------------------------------
    inline constexpr float minTrimDb = -20.0f, maxTrimDb = 20.0f;
    inline constexpr float minHighPassHz = 16.0f, maxHighPassHz = 350.0f;      // 0 = off
    inline constexpr float minLowPassHz = 3000.0f, maxLowPassHz = 22000.0f;    // 0 = off
    inline constexpr float eqRangeDb = 15.0f;                                  // every band cuts or boosts up to this
    inline constexpr float minLfHz = 30.0f, maxLfHz = 450.0f;
    inline constexpr float minLmfHz = 200.0f, maxLmfHz = 2500.0f;
    inline constexpr float minHmfHz = 600.0f, maxHmfHz = 7000.0f;
    inline constexpr float minHfHz = 1500.0f, maxHfHz = 16000.0f;
    inline constexpr float minQ = 0.5f, maxQ = 2.5f;
    inline constexpr float minThresholdDb = -40.0f, maxThresholdDb = 0.0f;
    inline constexpr float minRatio = 1.0f, maxRatio = 20.0f;
    inline constexpr float minAttackMs = 0.1f, maxAttackMs = 30.0f;
    inline constexpr float minReleaseSeconds = 0.1f, maxReleaseSeconds = 4.0f;
    inline constexpr float maxMakeupDb = 20.0f;
    inline constexpr float minGateThresholdDb = -70.0f, maxGateThresholdDb = -10.0f;
    inline constexpr float minGateRangeDb = 0.0f, maxGateRangeDb = 40.0f;
    inline constexpr float minGateHoldSeconds = 0.0f, maxGateHoldSeconds = 2.0f;

    struct Params
    {
        // Trim and drive
        float inputTrimDb = 0.0f, outputTrimDb = 0.0f;
        float drive = 0.0f;                    // 0 to 1; 0 leaves the sound exactly as it is

        // Filters (0 = off)
        float highPassHz = 0.0f, lowPassHz = 0.0f;

        // Equaliser
        bool eqOn = true;
        float lfHz = 100.0f, lfDb = 0.0f;  bool lfBell = false;
        float lmfHz = 600.0f, lmfDb = 0.0f, lmfQ = 1.0f;
        float hmfHz = 2500.0f, hmfDb = 0.0f, hmfQ = 1.0f;
        float hfHz = 8000.0f, hfDb = 0.0f;  bool hfBell = false;

        // Dynamics
        bool dynamicsOn = true;
        bool dynamicsAfterEq = false;          // the strip's "channel output" switch: the dynamics after the equaliser instead of before it
        float compThresholdDb = 0.0f;          // dBFS
        float compRatio = 1.0f;                // 1 = off
        float compAttackMs = 3.0f, compReleaseSeconds = 0.3f;
        bool compSoftKnee = true;
        float compMakeupDb = 0.0f;
        bool gateOn = false;
        float gateThresholdDb = -50.0f, gateRangeDb = 20.0f;
        bool gateExpander = false;             // false = gate (steep), true = expander (gentle)
        float gateAttackMs = 1.0f, gateHoldSeconds = 0.1f, gateReleaseSeconds = 0.3f;
    };

    // --- Songsmith design choices -----------------------------------------------------------------------------------------
    inline constexpr float softKneeDb = 6.0f;                 // the compressor's soft knee (a hard knee is 0)
    inline constexpr float detectorReleaseSeconds = 0.010f;   // how quickly the peak detectors let go (they never lag on the way up)
    inline constexpr float gateRatio = 10.0f, expanderRatio = 2.0f;   // how steeply the gate or expander turns the sound down below its threshold
    inline constexpr float gateHysteresisDb = 2.0f;           // the level must rise this far above the threshold to open it again

    // The steady gain reduction in dB (zero or positive) for a steady peak level, before any attack or release.
    inline float compressorReductionDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
    {
        const float r = juce::jmax (1.0f, ratio);
        const float slope = 1.0f - 1.0f / r;
        const float over = levelDb - thresholdDb;
        if (kneeDb <= 0.0f)
            return over > 0.0f ? over * slope : 0.0f;
        if (over <= -0.5f * kneeDb)
            return 0.0f;
        if (over >= 0.5f * kneeDb)
            return over * slope;
        const float x = over + 0.5f * kneeDb;
        return slope * x * x / (2.0f * kneeDb);
    }

    // How far down the gate or expander turns the sound for a steady level (zero or positive).
    inline float gateReductionDb (float levelDb, float thresholdDb, float rangeDb, bool expander) noexcept
    {
        const float ratio = expander ? expanderRatio : gateRatio;
        const float below = thresholdDb - levelDb;
        return below > 0.0f ? juce::jmin (rangeDb, below * (ratio - 1.0f)) : 0.0f;
    }

    // The drive: a smooth, symmetric soft clip that leaves quiet sounds at their own level. 'amount' 0 is exactly transparent.
    inline float driveGain (float amount) noexcept { return 1.0f + 7.0f * amount * amount; }

    namespace detail
    {
        struct Coeffs { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; bool identity = true; };

        inline Coeffs normalised (double b0, double b1, double b2, double a0, double a1, double a2)
        {
            Coeffs c;
            c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
            c.identity = false;
            return c;
        }

        inline Coeffs peak (double fs, double hz, double q, double db)
        {
            if (std::abs (db) < 1.0e-4) return {};
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * juce::jmin (hz, fs * 0.49) / fs;
            const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
            return normalised (1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
        }

        inline Coeffs lowShelf (double fs, double hz, double db)
        {
            if (std::abs (db) < 1.0e-4) return {};
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * juce::jmin (hz, fs * 0.49) / fs;
            const double c = std::cos (w), alpha = std::sin (w) / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * alpha;   // shelf slope 1
            return normalised (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                               (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
        }

        inline Coeffs highShelf (double fs, double hz, double db)
        {
            if (std::abs (db) < 1.0e-4) return {};
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * juce::jmin (hz, fs * 0.49) / fs;
            const double c = std::cos (w), alpha = std::sin (w) / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * alpha;
            return normalised (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                               (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
        }

        // A second-order Butterworth-type high-pass or low-pass with the given Q.
        inline Coeffs highPass2 (double fs, double hz, double q)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * juce::jmin (hz, fs * 0.49) / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            return normalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
        }
        inline Coeffs lowPass2 (double fs, double hz, double q)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * juce::jmin (hz, fs * 0.49) / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            return normalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
        }
        // A first-order high-pass (with the second-order one, a third-order high-pass: 18 dB per octave).
        inline Coeffs highPass1 (double fs, double hz)
        {
            const double K = std::tan (juce::MathConstants<double>::pi * juce::jmin (hz, fs * 0.49) / fs);
            Coeffs c;
            c.b0 = 1.0 / (1.0 + K); c.b1 = -c.b0; c.b2 = 0.0; c.a1 = (K - 1.0) / (1.0 + K); c.a2 = 0.0; c.identity = false;
            return c;
        }

        inline std::complex<double> response (const Coeffs& c, double fs, double hz)
        {
            if (c.identity) return { 1.0, 0.0 };
            const double w = 2.0 * juce::MathConstants<double>::pi * hz / fs;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
            return (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
        }

        class Biquad
        {
        public:
            void set (const Coeffs& c) noexcept { coeffs = c; }
            void reset() noexcept { z1 = z2 = 0.0; }
            bool isIdentity() const noexcept { return coeffs.identity; }
            double process (double x) noexcept
            {
                if (coeffs.identity) return x;
                const double y = coeffs.b0 * x + z1;
                z1 = coeffs.b1 * x - coeffs.a1 * y + z2;
                z2 = coeffs.b2 * x - coeffs.a2 * y;
                return y;
            }
        private:
            Coeffs coeffs;
            double z1 = 0.0, z2 = 0.0;
        };

        // log(cosh(x)), without overflow: the antiderivative of tanh.
        inline double logCosh (double x) noexcept
        {
            const double a = std::abs (x);
            return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453;
        }
    }

    class Engine
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            detectorCoef = coefficient (detectorReleaseSeconds);
            inputGain.reset (sampleRate, 0.02);
            outputGain.reset (sampleRate, 0.02);
            driveSmooth.reset (sampleRate, 0.02);
            makeup.reset (sampleRate, 0.02);
            updateCoefficients();
            reset();
        }

        void reset()
        {
            for (auto& ch : channels)
                for (auto* f : { &ch.hp1, &ch.hp2, &ch.lp, &ch.lf, &ch.lmf, &ch.hmf, &ch.hf })
                    f->reset();
            for (auto& d : drivePrev) d = 0.0;
            compPeak = gatePeak = 0.0f;
            compReduction = gateReduction = 0.0f;
            holdRemaining = 0;
            gateOpen = true;
            maxCompReduction = 0.0f;
            inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.inputTrimDb));
            outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputTrimDb));
            driveSmooth.setCurrentAndTargetValue (params.drive);
            makeup.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.compMakeupDb));
        }

        void setParams (const Params& p) noexcept
        {
            params = clamped (p);
            inputGain.setTargetValue (juce::Decibels::decibelsToGain (params.inputTrimDb));
            outputGain.setTargetValue (juce::Decibels::decibelsToGain (params.outputTrimDb));
            driveSmooth.setTargetValue (params.drive);
            makeup.setTargetValue (juce::Decibels::decibelsToGain (params.compMakeupDb));
            updateCoefficients();
        }

        const Params& getParams() const noexcept { return params; }

        // The response of the filters and the equaliser alone (the part of the strip that is linear), in decibels.
        double responseDb (double hz) const noexcept
        {
            std::complex<double> h { 1.0, 0.0 };
            for (auto* c : { &hp1c, &hp2c, &lpc })
                h *= detail::response (*c, sampleRate, hz);
            if (params.eqOn)
                for (auto* c : { &lfc, &lmfc, &hmfc, &hfc })
                    h *= detail::response (*c, sampleRate, hz);
            return 20.0 * std::log10 (std::max (std::abs (h), 1.0e-12));
        }

        void process (float* const* data, int numChannels, int numSamples) noexcept
        {
            numChannels = juce::jlimit (0, maxChannels, numChannels);
            const bool dynamicsActive = params.dynamicsOn;
            for (int i = 0; i < numSamples; ++i)
            {
                const float inGain = inputGain.getNextValue(), outGain = outputGain.getNextValue(), mk = makeup.getNextValue();
                const float driveNow = driveSmooth.getNextValue();
                double x[maxChannels] {};
                for (int c = 0; c < numChannels; ++c)
                {
                    double v = (double) data[c][i];
                    if (! std::isfinite (v)) v = 0.0;
                    v *= (double) inGain;
                    if (driveNow > 0.0f)
                        v = saturate (v, driveGain (driveNow), c);
                    else
                        drivePrev[(size_t) c] = v;
                    auto& ch = channels[(size_t) c];
                    v = ch.hp1.process (v);
                    v = ch.hp2.process (v);
                    v = ch.lp.process (v);
                    x[c] = v;
                }

                if (dynamicsActive && ! params.dynamicsAfterEq)
                    dynamics (x, numChannels, mk);
                if (params.eqOn)
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto& ch = channels[(size_t) c];
                        x[c] = ch.hf.process (ch.hmf.process (ch.lmf.process (ch.lf.process (x[c]))));
                    }
                if (dynamicsActive && params.dynamicsAfterEq)
                    dynamics (x, numChannels, mk);

                for (int c = 0; c < numChannels; ++c)
                    data[c][i] = (float) (x[c] * (double) outGain);
            }
        }

        // The compressor's current gain reduction in dB (positive), for the meter and the tests.
        float getGainReductionDb() const noexcept { return compReduction; }
        float getGateReductionDb() const noexcept { return gateReduction; }
        // The deepest compressor reduction since the last read.
        float readMaxGainReductionDb() noexcept
        {
            const float r = maxCompReduction;
            maxCompReduction = compReduction;
            return r;
        }

    private:
        struct Channel { detail::Biquad hp1, hp2, lp, lf, lmf, hmf, hf; };

        static Params clamped (Params p) noexcept
        {
            p.inputTrimDb = juce::jlimit (minTrimDb, maxTrimDb, p.inputTrimDb);
            p.outputTrimDb = juce::jlimit (minTrimDb, maxTrimDb, p.outputTrimDb);
            p.drive = juce::jlimit (0.0f, 1.0f, p.drive);
            p.highPassHz = p.highPassHz <= 0.0f ? 0.0f : juce::jlimit (minHighPassHz, maxHighPassHz, p.highPassHz);
            p.lowPassHz = p.lowPassHz <= 0.0f ? 0.0f : juce::jlimit (minLowPassHz, maxLowPassHz, p.lowPassHz);
            p.lfHz = juce::jlimit (minLfHz, maxLfHz, p.lfHz);     p.lfDb = juce::jlimit (-eqRangeDb, eqRangeDb, p.lfDb);
            p.lmfHz = juce::jlimit (minLmfHz, maxLmfHz, p.lmfHz); p.lmfDb = juce::jlimit (-eqRangeDb, eqRangeDb, p.lmfDb); p.lmfQ = juce::jlimit (minQ, maxQ, p.lmfQ);
            p.hmfHz = juce::jlimit (minHmfHz, maxHmfHz, p.hmfHz); p.hmfDb = juce::jlimit (-eqRangeDb, eqRangeDb, p.hmfDb); p.hmfQ = juce::jlimit (minQ, maxQ, p.hmfQ);
            p.hfHz = juce::jlimit (minHfHz, maxHfHz, p.hfHz);     p.hfDb = juce::jlimit (-eqRangeDb, eqRangeDb, p.hfDb);
            p.compThresholdDb = juce::jlimit (minThresholdDb, maxThresholdDb, p.compThresholdDb);
            p.compRatio = juce::jlimit (minRatio, maxRatio, p.compRatio);
            p.compAttackMs = juce::jlimit (minAttackMs, maxAttackMs, p.compAttackMs);
            p.compReleaseSeconds = juce::jlimit (minReleaseSeconds, maxReleaseSeconds, p.compReleaseSeconds);
            p.compMakeupDb = juce::jlimit (0.0f, maxMakeupDb, p.compMakeupDb);
            p.gateThresholdDb = juce::jlimit (minGateThresholdDb, maxGateThresholdDb, p.gateThresholdDb);
            p.gateRangeDb = juce::jlimit (minGateRangeDb, maxGateRangeDb, p.gateRangeDb);
            p.gateAttackMs = juce::jlimit (minAttackMs, maxAttackMs, p.gateAttackMs);
            p.gateHoldSeconds = juce::jlimit (minGateHoldSeconds, maxGateHoldSeconds, p.gateHoldSeconds);
            p.gateReleaseSeconds = juce::jlimit (minReleaseSeconds, maxReleaseSeconds, p.gateReleaseSeconds);
            return p;
        }

        float coefficient (float seconds) const noexcept
        {
            return 1.0f - std::exp (-1.0f / (float) (sampleRate * (double) juce::jmax (1.0e-5f, seconds)));
        }

        void updateCoefficients() noexcept
        {
            using namespace detail;
            hp1c = params.highPassHz > 0.0f ? highPass1 (sampleRate, params.highPassHz) : Coeffs();
            hp2c = params.highPassHz > 0.0f ? highPass2 (sampleRate, params.highPassHz, 1.0) : Coeffs();
            lpc = params.lowPassHz > 0.0f ? lowPass2 (sampleRate, params.lowPassHz, 0.70710678118) : Coeffs();
            lfc = params.lfBell ? peak (sampleRate, params.lfHz, 1.0, params.lfDb) : lowShelf (sampleRate, params.lfHz, params.lfDb);
            lmfc = peak (sampleRate, params.lmfHz, params.lmfQ, params.lmfDb);
            hmfc = peak (sampleRate, params.hmfHz, params.hmfQ, params.hmfDb);
            hfc = params.hfBell ? peak (sampleRate, params.hfHz, 1.0, params.hfDb) : highShelf (sampleRate, params.hfHz, params.hfDb);
            for (auto& ch : channels)
            {
                ch.hp1.set (hp1c); ch.hp2.set (hp2c); ch.lp.set (lpc);
                ch.lf.set (lfc); ch.lmf.set (lmfc); ch.hmf.set (hmfc); ch.hf.set (hfc);
            }
            compAttackCoef = coefficient (params.compAttackMs * 0.001f);
            compReleaseCoef = coefficient (params.compReleaseSeconds);
            gateAttackCoef = coefficient (params.gateAttackMs * 0.001f);
            gateReleaseCoef = coefficient (params.gateReleaseSeconds);
            holdSamples = (int) std::lround (params.gateHoldSeconds * sampleRate);
        }

        // y = tanh (g x) / g, with a first-order antiderivative correction so the clip does not fold into the audible range.
        double saturate (double x, float gain, int channel) noexcept
        {
            const double g = (double) gain;
            const double previous = drivePrev[(size_t) channel];
            drivePrev[(size_t) channel] = x;
            const double dx = x - previous;
            if (std::abs (dx) < 1.0e-9)
                return std::tanh (g * 0.5 * (x + previous)) / g;
            return (detail::logCosh (g * x) - detail::logCosh (g * previous)) / (g * g * dx);
        }

        // The gate or expander, then the compressor, both following the loudest channel.
        void dynamics (double* x, int numChannels, float makeupGain) noexcept
        {
            double loudest = 0.0;
            for (int c = 0; c < numChannels; ++c)
                loudest = std::max (loudest, std::abs (x[c]));
            const float level = (float) loudest;

            if (params.gateOn)
            {
                if (level > gatePeak) gatePeak = level; else gatePeak += (level - gatePeak) * detectorCoef;
                const float db = gatePeak > 1.0e-9f ? 20.0f * std::log10 (gatePeak) : -180.0f;
                const float threshold = params.gateThresholdDb;
                if (db > threshold + (gateOpen ? 0.0f : gateHysteresisDb))
                {
                    gateOpen = true;
                    holdRemaining = holdSamples;
                }
                else if (db < threshold && gateOpen)
                {
                    if (holdRemaining > 0) --holdRemaining; else gateOpen = false;
                }
                const float target = gateOpen ? 0.0f : gateReductionDb (db, threshold, params.gateRangeDb, params.gateExpander);
                gateReduction += (target - gateReduction) * (target > gateReduction ? gateReleaseCoef : gateAttackCoef);
                if (gateReduction < 1.0e-6f) gateReduction = 0.0f;
                const double g = gateReduction > 0.0f ? std::exp (-(double) gateReduction * 0.115129255) : 1.0;
                for (int c = 0; c < numChannels; ++c)
                    x[c] *= g;
            }
            else
                gateReduction = 0.0f;

            double compLoudest = 0.0;
            for (int c = 0; c < numChannels; ++c)
                compLoudest = std::max (compLoudest, std::abs (x[c]));
            const float cl = (float) compLoudest;
            if (cl > compPeak) compPeak = cl; else compPeak += (cl - compPeak) * detectorCoef;
            if (compPeak < 1.0e-12f) compPeak = 0.0f;
            const float cdb = compPeak > 1.0e-9f ? 20.0f * std::log10 (compPeak) : -180.0f;
            const float target = compressorReductionDb (cdb, params.compThresholdDb, params.compRatio, params.compSoftKnee ? softKneeDb : 0.0f);
            compReduction += (target - compReduction) * (target > compReduction ? compAttackCoef : compReleaseCoef);
            if (compReduction < 1.0e-9f) compReduction = 0.0f;
            maxCompReduction = juce::jmax (maxCompReduction, compReduction);
            const double g = (compReduction > 0.0f ? std::exp (-(double) compReduction * 0.115129255) : 1.0) * (double) makeupGain;
            for (int c = 0; c < numChannels; ++c)
                x[c] *= g;
        }

        Params params;
        double sampleRate = 44100.0;
        std::array<Channel, (size_t) maxChannels> channels;
        detail::Coeffs hp1c, hp2c, lpc, lfc, lmfc, hmfc, hfc;
        std::array<double, (size_t) maxChannels> drivePrev {};
        float detectorCoef = 0.0f, compAttackCoef = 0.0f, compReleaseCoef = 0.0f, gateAttackCoef = 0.0f, gateReleaseCoef = 0.0f;
        float compPeak = 0.0f, gatePeak = 0.0f, compReduction = 0.0f, gateReduction = 0.0f, maxCompReduction = 0.0f;
        int holdSamples = 0, holdRemaining = 0;
        bool gateOpen = true;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain { 1.0f }, outputGain { 1.0f }, makeup { 1.0f };
        juce::SmoothedValue<float> driveSmooth { 0.0f };
    };
}
