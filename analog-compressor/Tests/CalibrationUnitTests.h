#pragma once

// ACOMP-09 unit tests. Included once by AnalogCompressorTests.cpp after CalibrationTests.h.
#include "CalibrationTests.h"

struct CalibrationTests final : juce::UnitTest
{
    CalibrationTests() : juce::UnitTest ("Set Levels calibration", "FET") {}

    using Status = FET::Calibration::Status;
    static constexpr double fs = 48000.0;

    static std::unique_ptr<SongsmithAnalogCompressorProcessor> make()
    {
        auto p = std::make_unique<SongsmithAnalogCompressorProcessor>();
        p->prepareToPlay (fs, 512);
        return p;
    }

    // Level-matched bypass comparison: the same sound through the plugin with the proposal applied and bypassed.
    void expectLevelMatched (const CalibrationTestSupport::Signal& in, const FET::Calibration::Result& r,
                             int ratio, double toleranceDb, const juce::String& what)
    {
        using namespace CalibrationTestSupport;
        auto p = make();
        set (*p, ID::ratio, (float) ratio);
        set (*p, ID::input, r.inputDb);
        set (*p, ID::output, r.outputDb);
        p->prepareToPlay (fs, 512);   // the smoothers start at the new settings

        std::vector<float> gr;
        const auto wet = play (*p, in, &gr);
        set (*p, ID::bypass, 1.0f);
        p->prepareToPlay (fs, 512);
        const auto bypassed = play (*p, in);

        expectWithinAbsoluteError (rmsDb (wet), rmsDb (bypassed), toleranceDb, what + ": level matches the bypassed sound");
        expectWithinAbsoluteError (rmsDb (bypassed), rmsDb (in), 0.01, what + ": bypass is the dry sound");
        expectWithinAbsoluteError ((double) percentile90 (gr), (double) r.reductionDb, 1.0, what + ": measured reduction is what was proposed");
    }

    void runTest() override
    {
        using namespace CalibrationTestSupport;
        using R = FET::Calibration::Result;

        beginTest ("a sustained source gets about 3 dB of reduction and a matched level");
        {
            const auto in = sustained (fs, 6.0, 0.1f);
            auto p = make();
            const R r = calibrate (*p, in);
            expect (r.status == Status::good, "good");
            expectGreaterThan (r.reductionDb, 1.5f);
            expectLessThan (r.reductionDb, 4.5f);
            expectGreaterThan (r.inputDb, 0.0f, "a -20 dBFS source needs drive");
            expectGreaterThan (r.outputDb, -36.0f);
            expectLessThan (r.outputDb, 24.0f);
            expectLevelMatched (in, r, 0, 1.5, "sustained");
        }

        beginTest ("a transient source is judged on its loud hits, not its quiet gaps");
        {
            const auto in = transients (fs, 6.0, 0.5f);
            auto p = make();
            const R r = calibrate (*p, in);
            expect (r.usable(), "usable");
            expectGreaterThan (r.reductionDb, 1.5f);
            expectLessThan (r.reductionDb, 4.5f);
            expectLessThan (r.averageReductionDb, r.reductionDb, "the average is lower than the peaks");
            expectLevelMatched (in, r, 0, 2.5, "transients");
        }

        beginTest ("ratio, Ignore Below and the other settings are accounted for");
        {
            const auto in = sustained (fs, 6.0, 0.1f);
            auto four = make();
            const R r4 = calibrate (*four, in);
            auto twenty = make();
            set (*twenty, ID::ratio, 3.0f);
            const R r20 = calibrate (*twenty, in);
            expect (r4.inputDb != r20.inputDb || r4.outputDb != r20.outputDb, "20:1 does not propose what 4:1 does");

            Signal bass (in.size());
            for (size_t i = 0; i < bass.size(); ++i)
                bass[i] = (float) (0.3 * std::sin (juce::MathConstants<double>::twoPi * 50.0 * (double) i / fs)
                                   + 0.05 * std::sin (juce::MathConstants<double>::twoPi * 1500.0 * (double) i / fs));
            auto open = make();
            const R rOpen = calibrate (*open, bass);
            auto filtered = make();
            set (*filtered, ID::ignoreBelowHz, 200.0f);
            const R rFiltered = calibrate (*filtered, bass);
            expectGreaterThan (rFiltered.inputDb, rOpen.inputDb, "ignoring bass lets Input go higher for the same reduction");
        }

        beginTest ("silence, short captures and unreachable targets are reported, not guessed");
        {
            auto p = make();
            expect (calibrate (*p, Signal ((size_t) (fs * 5.0), 0.0f)).status == Status::silent, "digital silence");
            auto q = make();
            expect (calibrate (*q, sustained (fs, 1.0, 0.1f)).status == Status::tooShort, "one second is too short");
            auto quiet = make();
            const R r = calibrate (*quiet, sustained (fs, 5.0, 0.0005f));
            expect (r.status == Status::inputAtLimit, "very quiet: Input reaches its limit");
            expectWithinAbsoluteError (r.inputDb, FET::inputMaxDb, 0.01f);
            expectLessThan (r.reductionDb, 2.0f, "and the shortfall is reported");
            expect (r.usable(), "the musician may still apply it");
        }

        beginTest ("clipping risk and already-clipped sound are flagged");
        {
            auto p = make();
            Signal hot = sustained (fs, 5.0, 1.0f);
            for (auto& s : hot)
                s = juce::jlimit (-1.0f, 1.0f, 2.5f * s);
            const R r = calibrate (*p, hot);
            expect (r.alreadyClipping, "the captured sound reaches full scale");
            expect (r.clipRisk == (r.matchedPeakDb > FET::Calibration::clipPeakDb), "clip risk follows the matched peak");
        }

        beginTest ("listening copies sound and never touches a parameter; it stops by itself when full");
        {
            auto p = make();
            const float in0 = getParam (*p, ID::input), out0 = getParam (*p, ID::output);
            p->calibrationCapture.start();
            play (*p, sustained (fs, 2.0, 0.1f));
            expectEquals (p->calibrationCapture.getSamples(), (int) (fs * 2.0));
            expectEquals (getParam (*p, ID::input), in0);
            expectEquals (getParam (*p, ID::output), out0);
            play (*p, sustained (fs, 14.0, 0.1f));
            expect (! p->calibrationCapture.isActive(), "full");
            expectEquals (p->calibrationCapture.getSamples(), (int) (FET::Calibration::maxCaptureSeconds * fs));
            p->calibrationCapture.start();
            play (*p, sustained (fs, 1.0, 0.1f));
            p->calibrationCapture.stop();
            const int kept = p->calibrationCapture.getSamples();
            play (*p, sustained (fs, 1.0, 0.1f));
            expectEquals (p->calibrationCapture.getSamples(), kept, "stopped means stopped");
        }

        beginTest ("Input and Output stay fixed during playback while listening");
        {
            auto p = make();
            set (*p, ID::input, 7.0f);
            p->calibrationCapture.start();
            play (*p, sustained (fs, 4.0, 0.1f));
            expectEquals (getParam (*p, ID::input), 7.0f);
        }
    }
};
