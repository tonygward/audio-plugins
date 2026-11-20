#pragma once

// REC-01: checks for the whole-file Recording and Streaming Advisor, compiled into a test executable:
//   static SongsmithFileAdvisorTests sharedFileAdvisorTests { "Maximiser" };   // once per executable
//
// Reference signals are generated here, written to a temporary folder as real files, and read back
// through the same code the app uses. Expected loudness comes from the standard itself: a stereo
// sine with the same level on both channels at about 1 kHz reads the same number of LUFS as its
// level in dBFS (EBU Tech 3341, level test cases).

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/SongsmithFileAdvice.h"
#include "../Source/SongsmithWording.h"

class SongsmithFileAdvisorTests final : public juce::UnitTest
{
public:
    explicit SongsmithFileAdvisorTests (const juce::String& category) : juce::UnitTest ("Shared Songsmith File Advisor", category) {}

    void runTest() override
    {
        using namespace Songsmith;
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("songsmith-rec01", "", false);
        folder.createDirectory();

        beginTest ("44.1 kHz 16-bit: metadata, loudness reference and CD-quality advice");
        {
            const auto f = writeWav (folder, "cd.wav", 44100.0, 16, false, sine (44100.0, 10.0, 997.0, -23.0));
            const auto r = FileAnalysis::analyseFile (f);
            expect (r.opened, r.error);
            expectEquals (r.formatName, juce::String ("WAV"));
            expectEquals (r.sampleRate, 44100.0);
            expectEquals (r.storedBits, 16);
            expectEquals (r.channels, 2);
            expect (! r.floatingPoint);
            expectWithinAbsoluteError (r.durationSeconds, 10.0, 0.001);
            expect (r.hasIntegratedLoudness);
            expectWithinAbsoluteError (r.integratedLufs, -23.0, 0.1);
            expectWithinAbsoluteError (r.samplePeakDb, -23.0, 0.05);
            expectWithinAbsoluteError (r.truePeakDb, -23.0, 0.15);
            expect (r.clippedRuns == 0);
            const auto a = FileAdvice::adviseOn (r);
            expect (a.summary.contains ("CD quality") && a.summary.contains ("no need to record it again"), a.summary);
            expect (hasStep (a, FileAdvice::Kind::forNextRecording, "24-bit"));
            checkWording (a);
        }

        beginTest ("44.1 kHz 24-bit and 48 kHz 24-bit are good recording settings");
        {
            for (double fs : { 44100.0, 48000.0 })
            {
                const auto f = writeWav (folder, "hi" + juce::String ((int) fs) + ".wav", fs, 24, false, sine (fs, 10.0, 997.0, -23.0));
                const auto r = FileAnalysis::analyseFile (f);
                expect (r.opened, r.error);
                expectEquals (r.storedBits, 24);
                expectWithinAbsoluteError (r.integratedLufs, -23.0, 0.1);
                const auto a = FileAdvice::adviseOn (r);
                expect (a.summary.contains ("good recording settings"), a.summary);
                expect (! hasStep (a, FileAdvice::Kind::forNextRecording, "24-bit"), "no advice to do what was already done");
                checkWording (a);
            }
        }

        beginTest ("AIFF and FLAC are read, and the format name says which");
        {
            const auto aiff = FileAnalysis::analyseFile (writeAiff16 (folder, "x.aiff", sine (44100.0, 5.0, 997.0, -20.0)));
            expect (aiff.opened, aiff.error);
            expectEquals (aiff.formatName, juce::String ("AIFF"));
            expectEquals (aiff.storedBits, 16);
            expectWithinAbsoluteError (aiff.integratedLufs, -20.0, 0.1);

            const auto flacFile = folder.getChildFile ("x.flac");
            {
                juce::FlacAudioFormat flac;
                const auto data = sine (44100.0, 5.0, 997.0, -20.0);
                std::unique_ptr<juce::FileOutputStream> out (flacFile.createOutputStream());
                if (out != nullptr)
                {
                    std::unique_ptr<juce::AudioFormatWriter> w (flac.createWriterFor (out.get(), 44100.0, 2, 24, {}, 0));
                    if (w != nullptr)
                    {
                        out.release();   // the writer owns the stream now
                        w->writeFromAudioSampleBuffer (data, 0, data.getNumSamples());
                    }
                }
            }
            const auto fl = FileAnalysis::analyseFile (flacFile);
            expect (fl.opened, fl.error);
            expectEquals (fl.formatName, juce::String ("FLAC"));
            expectEquals (fl.storedBits, 24);
            expectWithinAbsoluteError (fl.integratedLufs, -20.0, 0.1);
        }

        beginTest ("32-bit floating point above full scale is reported without alarm");
        {
            auto data = sine (48000.0, 5.0, 997.0, 3.5);   // peaks at about +3.5 dBFS
            const auto r = FileAnalysis::analyseFile (writeWav (folder, "float.wav", 48000.0, 32, true, data));
            expect (r.opened, r.error);
            expect (r.floatingPoint);
            expectEquals (r.storedBits, 32);
            expect (r.samplesAboveFullScale > 0);
            expect (r.truePeakDb > 3.0);
            const auto a = FileAdvice::adviseOn (r);
            expect (a.summary.contains ("floating point"), a.summary);
            expect (hasStep (a, FileAdvice::Kind::noChangeNecessary, "not proof"));
            checkWording (a);
        }

        beginTest ("True peak finds a peak that falls between samples");
        {
            // A full-scale sine at a quarter of the sample rate, 45 degrees out of phase: every sample is
            // 0.7071, the true peak is 1.0 (0 dBTP).
            juce::AudioBuffer<float> b (2, 48000);
            for (int i = 0; i < b.getNumSamples(); ++i)
                for (int c = 0; c < 2; ++c)
                {
                    // A short fade at each end keeps the abrupt start from adding its own overshoot.
                    const double fade = juce::jmin (1.0, i / 4800.0, (b.getNumSamples() - 1 - i) / 4800.0);
                    b.setSample (c, i, (float) (fade * std::sin (2.0 * juce::MathConstants<double>::pi * 0.25 * i + juce::MathConstants<double>::pi / 4.0)));
                }
            const auto r = FileAnalysis::analyseFile (writeWav (folder, "between.wav", 48000.0, 32, true, b));
            expectWithinAbsoluteError (r.samplePeakDb, -3.01, 0.02);
            expectWithinAbsoluteError (r.truePeakDb, 0.0, 0.2);
        }

        beginTest ("Clipping is described cautiously");
        {
            auto data = sine (44100.0, 4.0, 220.0, 6.0);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < data.getNumSamples(); ++i)
                    data.setSample (c, i, juce::jlimit (-1.0f, 1.0f, data.getSample (c, i)));
            const auto r = FileAnalysis::analyseFile (writeWav (folder, "clipped.wav", 44100.0, 16, false, data));
            expect (r.clippedRuns > 10, "runs at the ceiling");
            const auto a = FileAdvice::adviseOn (r);
            expect (hasStep (a, FileAdvice::Kind::noChangeNecessary, "not proof"));
            expect (hasStep (a, FileAdvice::Kind::tryIfHearingAProblem, "if you hear"));
            expect (a.cannotKnow.joinIntoString (" ").contains ("Whether the original recording clipped"));
            checkWording (a);
        }

        beginTest ("Silence and very short files are handled honestly");
        {
            juce::AudioBuffer<float> silent (2, 44100 * 3);
            silent.clear();
            const auto s = FileAnalysis::analyseFile (writeWav (folder, "silence.wav", 44100.0, 16, false, silent));
            expect (s.opened && s.silent && ! s.hasIntegratedLoudness && ! std::isfinite (s.truePeakDb));
            expect (FileAdvice::adviseOn (s).summary.contains ("silent"));

            const auto tiny = FileAnalysis::analyseFile (writeWav (folder, "tiny.wav", 44100.0, 16, false, sine (44100.0, 0.2, 997.0, -20.0)));
            expect (tiny.opened && ! tiny.hasIntegratedLoudness && ! tiny.hasLoudnessRange);
            const auto a = FileAdvice::adviseOn (tiny);
            expect (a.measured.joinIntoString (" ").contains ("not available"));
            checkWording (a);

            const auto oneSecond = FileAnalysis::analyseFile (writeWav (folder, "one.wav", 44100.0, 16, false, sine (44100.0, 1.0, 997.0, -20.0)));
            expect (oneSecond.hasIntegratedLoudness && ! oneSecond.hasLoudnessRange, "range needs a 3 s window");

            const auto quiet = FileAnalysis::analyseFile (writeWav (folder, "quiet.wav", 44100.0, 16, false, sine (44100.0, 5.0, 997.0, -80.0)));
            expect (quiet.opened && ! quiet.hasIntegratedLoudness, "below the absolute gate");
        }

        beginTest ("Gating and loudness range on a dynamic file");
        {
            // 10 s at -20, then 10 s at -50: the quiet part is more than 10 below, so it is gated out.
            const auto r = FileAnalysis::analyseFile (writeWav (folder, "gated.wav", 48000.0, 24, false, twoLevels (48000.0, 10.0, -20.0, -50.0)));
            expectWithinAbsoluteError (r.integratedLufs, -20.0, 0.2);
            // 15 s at -40 then 15 s at -20: the range is about 20.
            const auto d = FileAnalysis::analyseFile (writeWav (folder, "range.wav", 48000.0, 24, false, twoLevels (48000.0, 15.0, -40.0, -20.0)));
            expect (d.hasLoudnessRange);
            expectWithinAbsoluteError (d.loudnessRange, 20.0, 1.0);
            // Very loud: -6 dBFS sine reads -6 LUFS and gets the gentle, conditional note.
            const auto loud = FileAnalysis::analyseFile (writeWav (folder, "loud.wav", 44100.0, 16, false, sine (44100.0, 5.0, 997.0, -6.0)));
            expectWithinAbsoluteError (loud.integratedLufs, -6.0, 0.1);
            const auto a = FileAdvice::adviseOn (loud);
            expect (hasStep (a, FileAdvice::Kind::tryIfHearingAProblem, "very loud"));
            checkWording (a);
        }

        beginTest ("A 24-bit file whose values fit in 16 bits says so, and stays calm");
        {
            juce::AudioBuffer<float> b = sine (44100.0, 3.0, 440.0, -12.0);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < b.getNumSamples(); ++i)
                    b.setSample (c, i, (float) (std::round (b.getSample (c, i) * 32768.0) / 32768.0));
            const auto r = FileAnalysis::analyseFile (writeWav (folder, "padded.wav", 44100.0, 24, false, b));
            expectEquals (r.storedBits, 24);
            expectEquals (r.bitsInUse, 16);
            const auto a = FileAdvice::adviseOn (r);
            expect (hasStep (a, FileAdvice::Kind::noChangeNecessary, "harmless"));
            expect (! a.summary.contains ("convert"));
            checkWording (a);
        }

        beginTest ("Malformed, unreadable and unsupported files give plain errors and change nothing");
        {
            const auto garbage = folder.getChildFile ("garbage.wav");
            garbage.replaceWithText ("this is not audio at all");
            const auto before = garbage.loadFileAsString();
            const auto r1 = FileAnalysis::analyseFile (garbage);
            expect (! r1.opened && r1.error.isNotEmpty());
            expectEquals (garbage.loadFileAsString(), before);

            const auto good = writeWav (folder, "whole.wav", 44100.0, 16, false, sine (44100.0, 2.0, 440.0, -20.0));
            juce::MemoryBlock bytes;
            good.loadFileAsData (bytes);
            const auto cut = folder.getChildFile ("cut.wav");
            cut.replaceWithData (bytes.getData(), bytes.getSize() / 2);   // header promises more than the file holds
            const auto r2 = FileAnalysis::analyseFile (cut);
            expect (r2.error.isNotEmpty() || (r2.opened && r2.frames <= 88200), "cut-short file is never reported longer than it was");

            const auto r3 = FileAnalysis::analyseFile (folder.getChildFile ("missing.wav"));
            expect (! r3.opened && r3.error.contains ("could not be found"));

            // A kind of file the analyser does not read is refused in plain words, naming the kinds it does.
            const auto ogg = folder.getChildFile ("song.ogg");
            ogg.replaceWithText ("x");
            const auto r4 = FileAnalysis::analyseFile (ogg);
            expect (! r4.opened && r4.error.contains ("WAV, AIFF, FLAC and MP3"));

            for (const auto& r : { r1, r3, r4 })
                checkWording (FileAdvice::adviseOn (r));
        }

        beginTest ("Analysis never changes the original file, and can be cancelled");
        {
            const auto f = writeWav (folder, "keep.wav", 44100.0, 16, false, sine (44100.0, 3.0, 440.0, -20.0));
            juce::MemoryBlock a, b;
            f.loadFileAsData (a);
            FileAnalysis::analyseFile (f);
            f.loadFileAsData (b);
            expect (a == b, "file bytes unchanged");

            std::atomic<bool> stop { true };
            expect (FileAnalysis::analyseFile (f, &stop).cancelled);
        }

        beginTest ("Advice never uses fear, targets or platform pass marks");
        {
            const juce::StringArray forbidden { "ruined", "unusable", "poor", "bad", "score", "warning", "must", "required", "fail", "pass ",
                                                "spotify", "apple", "youtube", "-14", "upgrade", "restore", "convert", "mud" };
            for (auto* name : { "cd.wav", "hi48000.wav", "float.wav", "clipped.wav", "loud.wav", "padded.wav" })
            {
                const auto a = FileAdvice::adviseOn (FileAnalysis::analyseFile (folder.getChildFile (name)));
                juce::String all = a.summary + " " + a.cannotKnow.joinIntoString (" ");
                for (const auto& s : a.steps) all << " " << s.text;
                for (const auto& w : forbidden)
                    expect (! all.containsIgnoreCase (w), juce::String (name) + " contains \"" + w + "\"");
                expect (! all.contains ("color") && ! all.contains ("analyze"), "British spelling");
            }
        }

        folder.deleteRecursively();
    }

private:
    static juce::AudioBuffer<float> sine (double fs, double seconds, double freq, double levelDb)
    {
        juce::AudioBuffer<float> b (2, (int) std::lround (fs * seconds));
        const double amp = std::pow (10.0, levelDb / 20.0);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto v = (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / fs));
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    static juce::AudioBuffer<float> twoLevels (double fs, double secondsEach, double firstDb, double secondDb)
    {
        auto a = sine (fs, secondsEach, 997.0, firstDb);
        const auto b = sine (fs, secondsEach, 997.0, secondDb);
        const int n = a.getNumSamples();
        a.setSize (2, n * 2, true);
        for (int c = 0; c < 2; ++c) a.copyFrom (c, n, b, c, 0, n);
        return a;
    }

    static void put16 (juce::MemoryOutputStream& o, int v) { o.writeShort ((short) v); }

    // Writes a little-endian WAV by hand: integer PCM (16, 24 or 32 bits) or 32-bit floating point.
    static juce::File writeWav (const juce::File& folder, const juce::String& name, double fs, int bits, bool isFloat, const juce::AudioBuffer<float>& data)
    {
        const int ch = data.getNumChannels(), n = data.getNumSamples(), bytes = bits / 8;
        juce::MemoryOutputStream o;
        o.write ("RIFF", 4); o.writeInt (36 + n * ch * bytes); o.write ("WAVE", 4);
        o.write ("fmt ", 4); o.writeInt (16); put16 (o, isFloat ? 3 : 1); put16 (o, ch);
        o.writeInt ((int) fs); o.writeInt ((int) fs * ch * bytes); put16 (o, ch * bytes); put16 (o, bits);
        o.write ("data", 4); o.writeInt (n * ch * bytes);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c)
            {
                const float x = data.getSample (c, i);
                if (isFloat) { o.writeFloat (x); continue; }
                const double scale = std::pow (2.0, bits - 1);
                const auto v = (juce::int64) juce::jlimit (-scale, scale - 1.0, std::round ((double) x * scale));
                for (int k = 0; k < bytes; ++k) o.writeByte ((char) ((v >> (8 * k)) & 0xff));
            }
        const auto f = folder.getChildFile (name);
        f.replaceWithData (o.getData(), o.getDataSize());
        return f;
    }

    // Writes a 16-bit big-endian AIFF by hand, with the sample rate as an 80-bit extended number.
    static juce::File writeAiff16 (const juce::File& folder, const juce::String& name, const juce::AudioBuffer<float>& data)
    {
        const int ch = data.getNumChannels(), n = data.getNumSamples();
        juce::MemoryOutputStream o;
        o.write ("FORM", 4); o.writeIntBigEndian (4 + 8 + 18 + 8 + 8 + n * ch * 2); o.write ("AIFF", 4);
        o.write ("COMM", 4); o.writeIntBigEndian (18); o.writeShortBigEndian ((short) ch); o.writeIntBigEndian (n); o.writeShortBigEndian (16);
        // 44100 = 0xAC44 * 2^(15-15): exponent 0x400E, mantissa 0xAC44 << 48.
        o.writeShortBigEndian (0x400E); o.writeIntBigEndian ((int) 0xAC440000); o.writeIntBigEndian (0);
        o.write ("SSND", 4); o.writeIntBigEndian (8 + n * ch * 2); o.writeIntBigEndian (0); o.writeIntBigEndian (0);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c)
                o.writeShortBigEndian ((short) juce::jlimit (-32768.0, 32767.0, std::round ((double) data.getSample (c, i) * 32768.0)));
        const auto f = folder.getChildFile (name);
        f.replaceWithData (o.getData(), o.getDataSize());
        return f;
    }

    static bool hasStep (const Songsmith::FileAdvice::Advice& a, Songsmith::FileAdvice::Kind k, const juce::String& text)
    {
        for (const auto& s : a.steps)
            if (s.kind == k && s.text.containsIgnoreCase (text)) return true;
        return false;
    }

    void checkWording (const Songsmith::FileAdvice::Advice& a)
    {
        auto check = [this] (const juce::String& t)
        {
            expect (Songsmith::UI::Wording::findBanned (t).isEmpty(),
                    "wording in \"" + t + "\" (found \"" + Songsmith::UI::Wording::findBanned (t) + "\")");
        };
        check (a.summary);
        for (const auto& m : a.measured) check (m);
        for (const auto& s : a.steps) check (s.text);
        for (const auto& s : a.cannotKnow) check (s);
        for (auto k : { Songsmith::FileAdvice::Kind::noChangeNecessary, Songsmith::FileAdvice::Kind::tryIfHearingAProblem, Songsmith::FileAdvice::Kind::forNextRecording })
            check (Songsmith::FileAdvice::heading (k));
    }
};
