// Songsmith Audio Analyzer test runner (REC-02): the standalone window, the readings it shows for real WAV, AIFF and FLAC
// reference files, errors, repeated and cancelled analyses, text rules and layout, plus the shared REC-01 analysis and
// advice checks and the shared Songsmith UI checks.
//
// Reference signals are generated here, written to a temporary folder as real files and read back through the same code
// the app uses. Expected loudness comes from the standard itself: a stereo sine with the same level on both channels at
// about 1 kHz reads the same number of LUFS as its level in dBFS (EBU Tech 3341, level test cases).
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <functional>
#include "AnalyzerComponent.h"
#include "../../shared/Tests/SongsmithFileAdvisorTests.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "SongsmithWording.h"

namespace
{
    using namespace Songsmith;
    using AnalyzerUI = Analyzer::AnalyzerComponent;

    juce::AudioBuffer<float> sine (double fs, double seconds, double freq, double levelDb, int channels = 2, double phase = 0.0)
    {
        juce::AudioBuffer<float> b (channels, (int) std::lround (fs * seconds));
        const double amp = std::pow (10.0, levelDb / 20.0);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto v = (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / fs + phase));
            for (int c = 0; c < channels; ++c)
                b.setSample (c, i, v);
        }
        return b;
    }

    // A WAV by hand: integer PCM (16, 24 or 32 bits) or 32-bit floating point.
    juce::File writeWav (const juce::File& folder, const juce::String& name, double fs, int bits, bool isFloat, const juce::AudioBuffer<float>& data)
    {
        const int ch = data.getNumChannels(), n = data.getNumSamples(), bytes = bits / 8;
        juce::MemoryOutputStream o;
        o.write ("RIFF", 4); o.writeInt (36 + n * ch * bytes); o.write ("WAVE", 4);
        o.write ("fmt ", 4); o.writeInt (16); o.writeShort ((short) (isFloat ? 3 : 1)); o.writeShort ((short) ch);
        o.writeInt ((int) fs); o.writeInt ((int) fs * ch * bytes); o.writeShort ((short) (ch * bytes)); o.writeShort ((short) bits);
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

    // AIFF or FLAC, written by JUCE's own writers (the file's extension picks the format).
    juce::File writeWithJuce (const juce::File& folder, const juce::String& name, double fs, int bits, const juce::AudioBuffer<float>& data)
    {
        const auto f = folder.getChildFile (name);
        f.deleteFile();
        std::unique_ptr<juce::AudioFormat> format;
        if (f.hasFileExtension ("flac")) format = std::make_unique<juce::FlacAudioFormat>();
        else                             format = std::make_unique<juce::AiffAudioFormat>();
        std::unique_ptr<juce::OutputStream> stream (f.createOutputStream());
        auto writer = format->createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (fs).withNumChannels (data.getNumChannels()).withBitsPerSample (bits));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (data, 0, data.getNumSamples());
        return f;
    }

    // Runs the message loop until 'done' or the time runs out, so background analyses can deliver their results.
    bool waitUntil (const std::function<bool()>& done, int timeoutMs = 30000)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (! done() && juce::Time::getMillisecondCounter() < end)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        return done();
    }

    void settle (int milliseconds = 120) { juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds); }

    // Reads one file to its result through the app's own background reader.
    bool analyseInWindow (AnalyzerUI& ui, const juce::File& f)
    {
        ui.analyse (f);
        return waitUntil ([&] { return ui.getState() == AnalyzerUI::State::showing; });
    }

    juce::String readingValue (AnalyzerUI& ui, const juce::String& caption)
    {
        for (int i = 0; i < ui.getNumTiles(); ++i)
            if (ui.getTile (i).getReading().caption == caption)
                return ui.getTile (i).getReading().value;
        return "(no such reading)";
    }

    juce::String readingNote (AnalyzerUI& ui, const juce::String& caption)
    {
        for (int i = 0; i < ui.getNumTiles(); ++i)
            if (ui.getTile (i).getReading().caption == caption)
                return ui.getTile (i).getReading().note;
        return {};
    }

    // ANA-03: a buffer whose channels are sines at different levels, so each channel's peak is known.
    juce::AudioBuffer<float> sineLevels (double fs, double seconds, double freq, std::initializer_list<double> levelsDb)
    {
        juce::AudioBuffer<float> b ((int) levelsDb.size(), (int) std::lround (fs * seconds));
        int c = 0;
        for (auto level : levelsDb)
        {
            const double amp = std::pow (10.0, level / 20.0);
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / fs)));
            ++c;
        }
        return b;
    }

    // ANA-02: a real, valid MP3 made of silent frames (MPEG-1 Layer III, a header and zeroed data). It decodes to digital silence,
    // so it exercises everything about opening and describing an MP3 (format, sample rate, channels, length, bitrate, long files,
    // constant and varying bitrates) without an encoder.
    //   sampleRateIndex: 0 = 44.1 kHz, 1 = 48 kHz, 2 = 32 kHz.   channelMode: 0 = stereo, 3 = mono.
    //   bitrateIndexOf (frame): 1 to 14 = 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 kilobits per second.
    juce::File writeSilentMp3 (const juce::File& folder, const juce::String& name, int frames, int sampleRateIndex, int channelMode,
                               const std::function<int (int)>& bitrateIndexOf)
    {
        static const int kbps[] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
        static const int rates[] = { 44100, 48000, 32000 };
        juce::MemoryOutputStream o;
        int carry = 0;   // the padding bit keeps the average frame length exact, as an encoder does
        for (int i = 0; i < frames; ++i)
        {
            const int index = bitrateIndexOf (i);
            const int numerator = 144000 * kbps[index];
            const int length = numerator / rates[sampleRateIndex];
            carry += numerator % rates[sampleRateIndex];
            const int padding = carry >= rates[sampleRateIndex] ? 1 : 0;
            if (padding != 0) carry -= rates[sampleRateIndex];
            o.writeByte ((char) 0xFF);
            o.writeByte ((char) 0xFB);   // MPEG-1, Layer III, no error check
            o.writeByte ((char) ((index << 4) | (sampleRateIndex << 2) | (padding << 1)));
            o.writeByte ((char) (channelMode << 6));
            for (int k = 4; k < length + padding; ++k)
                o.writeByte (0);
        }
        const auto file = folder.getChildFile (name);
        file.replaceWithData (o.getData(), o.getDataSize());
        return file;
    }

    juce::File writeSilentMp3 (const juce::File& folder, const juce::String& name, double seconds, int sampleRateIndex, int channelMode, int bitrateIndex)
    {
        static const double rates[] = { 44100.0, 48000.0, 32000.0 };
        const int frames = (int) std::lround (seconds * rates[sampleRateIndex] / 1152.0);
        return writeSilentMp3 (folder, name, frames, sampleRateIndex, channelMode, [bitrateIndex] (int) { return bitrateIndex; });
    }

    // Something that sounds like a song: a steady low note, brighter notes that come and go, and a little noise.
    juce::AudioBuffer<float> songLike (double fs, double seconds)
    {
        juce::AudioBuffer<float> b (2, (int) std::lround (fs * seconds));
        juce::Random rng (7);
        const double pi = juce::MathConstants<double>::pi;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double t = (double) i / fs;
            const double bar = std::fmod (t, 2.0) / 2.0;
            const double env = std::exp (-3.0 * bar);
            double l = 0.26 * std::sin (2.0 * pi * 55.0 * t) + 0.12 * std::sin (2.0 * pi * 110.0 * t);
            double r = l;
            l += env * 0.22 * std::sin (2.0 * pi * 440.0 * t) + 0.04 * (double) (rng.nextFloat() - 0.5f);
            r += env * 0.22 * std::sin (2.0 * pi * 554.4 * t) + 0.04 * (double) (rng.nextFloat() - 0.5f);
            b.setSample (0, i, (float) l);
            b.setSample (1, i, (float) r);
        }
        return b;
    }
}

//==============================================================================
class AudioAnalyzerTests final : public juce::UnitTest
{
public:
    AudioAnalyzerTests() : juce::UnitTest ("Songsmith Audio Analyzer (REC-02)", "AudioAnalyzer") {}

    void runTest() override
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("songsmith-rec02", "", false);
        folder.createDirectory();

        beginTest ("Identity: the product name and bundle ID the installers recognise never change, and the version is the shared one");
        {
            expectEquals (juce::String (ANALYZER_PRODUCT_NAME), juce::String ("Songsmith Audio Analyzer"));
            expectEquals (juce::String (ANALYZER_BUNDLE_ID), juce::String ("com.songsmithstudio.songsmithaudioanalyzer"));
            auto file = juce::File::getCurrentWorkingDirectory();
            for (int up = 0; up < 8 && ! file.getChildFile ("VERSION").existsAsFile(); ++up)
                file = file.getParentDirectory();
            if (file.getChildFile ("VERSION").existsAsFile())
                expectEquals (juce::String (ANALYZER_VERSION), file.getChildFile ("VERSION").loadFileAsString().trim());
        }

        beginTest ("The readings for 16-bit and 24-bit WAV reference files: loudness, true peak, sample rate, bit depth, duration");
        {
            AnalyzerUI ui;
            expect (analyseInWindow (ui, writeWav (folder, "cd.wav", 44100.0, 16, false, sine (44100.0, 10.0, 997.0, -23.0))), "the file is read");
            expect (ui.getResult().opened, ui.getResult().error);
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("-23.0 LUFS"));
            expectEquals (readingValue (ui, "True Peak"), juce::String ("-23.0 dBTP"));
            expectEquals (readingValue (ui, "Sample Rate"), juce::String ("44.1 kHz"));
            expectEquals (readingValue (ui, "Bit Depth and Encoding"), juce::String ("16-bit"));
            expectEquals (readingNote (ui, "Bit Depth and Encoding"), juce::String ("WAV, uncompressed"));
            expectEquals (readingValue (ui, "Duration"), juce::String ("0:10.0"));
            expectEquals (readingValue (ui, "Channels"), juce::String ("Stereo"));
            expect (ui.getAdvice().summary.contains ("CD quality"), ui.getAdvice().summary);

            expect (analyseInWindow (ui, writeWav (folder, "hi.wav", 48000.0, 24, false, sine (48000.0, 65.5, 997.0, -14.0))), "a second, longer file");
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("-14.0 LUFS"));
            expectEquals (readingValue (ui, "Sample Rate"), juce::String ("48 kHz"));
            expectEquals (readingValue (ui, "Bit Depth and Encoding"), juce::String ("24-bit"));
            expectEquals (readingValue (ui, "Duration"), juce::String ("1:05.5"));
            expect (ui.getAdvice().summary.contains ("good recording settings"), ui.getAdvice().summary);
        }

        beginTest ("The readings for AIFF, FLAC, 32-bit floating point and mono files");
        {
            AnalyzerUI ui;
            const auto reference = sine (48000.0, 8.0, 997.0, -20.0);
            expect (analyseInWindow (ui, writeWithJuce (folder, "ref.aiff", 48000.0, 24, reference)), "AIFF");
            expect (ui.getResult().opened, ui.getResult().error);
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("-20.0 LUFS"));
            expectEquals (readingValue (ui, "Bit Depth and Encoding"), juce::String ("24-bit"));
            expectEquals (readingNote (ui, "Bit Depth and Encoding"), juce::String ("AIFF, uncompressed"));

            expect (analyseInWindow (ui, writeWithJuce (folder, "ref.flac", 48000.0, 24, reference)), "FLAC");
            expect (ui.getResult().opened, ui.getResult().error);
            expectEquals (ui.getResult().formatName, juce::String ("FLAC"));
            expectWithinAbsoluteError (ui.getResult().integratedLufs, -20.0, 0.1, "FLAC is lossless, so it reads the same");
            expectEquals (readingNote (ui, "Bit Depth and Encoding"), juce::String ("FLAC, lossless"));
            expectEquals (readingValue (ui, "Duration"), juce::String ("0:08.0"));

            expect (analyseInWindow (ui, writeWav (folder, "float.wav", 48000.0, 32, true, reference)), "floating point");
            expectEquals (readingValue (ui, "Bit Depth and Encoding"), juce::String ("32-bit"));
            expectEquals (readingNote (ui, "Bit Depth and Encoding"), juce::String ("WAV, floating point"));

            // A mono file: 1 kHz sine at -23 dBFS in one channel reads 3.0 lower, as the standard says (one channel counts once).
            expect (analyseInWindow (ui, writeWav (folder, "mono.wav", 44100.0, 16, false, sine (44100.0, 10.0, 997.0, -20.0, 1))), "mono");
            expectEquals (readingValue (ui, "Channels"), juce::String ("Mono"));
            expectWithinAbsoluteError (ui.getResult().integratedLufs, -23.0, 0.15, "a mono file counts its one channel once");
        }

        beginTest ("MP3 files (ANA-02): opened by the app's own reader, described as lossy with no bit depth, and never given invented figures");
        {
            AnalyzerUI ui;
            // The usual bitrates and the usual sample rates, stereo and mono. Each is a valid MP3 of digital silence.
            struct Case { const char* label; int sampleRateIndex, channelMode, bitrateIndex, expectedKbps, expectedRate; };
            const Case cases[] = { { "128 kbps 44.1 kHz stereo", 0, 0, 9, 128, 44100 },
                                   { "192 kbps 44.1 kHz stereo", 0, 0, 11, 192, 44100 },
                                   { "256 kbps 44.1 kHz stereo", 0, 0, 13, 256, 44100 },
                                   { "320 kbps 44.1 kHz stereo", 0, 0, 14, 320, 44100 },
                                   { "128 kbps 48 kHz stereo", 1, 0, 9, 128, 48000 },
                                   { "64 kbps 32 kHz mono", 2, 3, 5, 64, 32000 },
                                   { "128 kbps 44.1 kHz mono", 0, 3, 9, 128, 44100 } };
            for (const auto& c : cases)
            {
                const auto mp3 = writeSilentMp3 (folder, "silent.mp3", 3.0, c.sampleRateIndex, c.channelMode, c.bitrateIndex);
                expect (analyseInWindow (ui, mp3), c.label);
                const auto& r = ui.getResult();
                expect (r.opened, juce::String (c.label) + ": " + r.error);
                expectEquals (r.formatName, juce::String ("MP3"), c.label);
                expect (r.lossy, c.label);
                expectEquals ((int) r.sampleRate, c.expectedRate, c.label);
                expectEquals (r.channels, c.channelMode == 3 ? 1 : 2, c.label);
                expectWithinAbsoluteError (r.durationSeconds, 3.0, 0.1, c.label);
                expectEquals (r.averageBitrateKbps, c.expectedKbps, c.label);
                expectEquals (r.storedBits, 0, "no bit depth is made up for a compressed file");
                expectEquals (readingValue (ui, "Bit Depth and Encoding"), juce::String ("Not applicable"));
                expectEquals (readingNote (ui, "Bit Depth and Encoding"), juce::String ("MP3, lossy"));
                expect (ui.getFileDetailLabel().getText().contains ("about " + juce::String (c.expectedKbps) + " kilobits per second"),
                        "the line under the file name gives the average bitrate: " + ui.getFileDetailLabel().getText());
                expectEquals (readingValue (ui, "Channels"), juce::String (c.channelMode == 3 ? "Mono" : "Stereo"), c.label);
            }

            // Digital silence: no loudness or peak is invented for it.
            expect (analyseInWindow (ui, writeSilentMp3 (folder, "silent.mp3", 3.0, 0, 0, 9)), "silent MP3");
            expect (ui.getResult().silent, "a silent MP3 is reported as silent");
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("Not available"));
            expectEquals (readingValue (ui, "True Peak"), juce::String ("Silent"));
            expect (ui.getAdvice().summary.contains ("silent"), ui.getAdvice().summary);
        }

        beginTest ("MP3 files (ANA-02): a changing bitrate, a long file, and the words used about a compressed file");
        {
            AnalyzerUI ui;
            // A file whose bitrate changes from frame to frame, as a variable-bitrate file does.
            static const int steps[] = { 5, 9, 13, 9, 7, 11 };   // 64, 128, 256, 128, 96, 192 kilobits per second: the mean is 144
            const auto vbr = writeSilentMp3 (folder, "vbr.mp3", 2600, 0, 0, [] (int frame) { return steps[frame % 6]; });
            expect (analyseInWindow (ui, vbr), "variable bitrate");
            expect (ui.getResult().opened, ui.getResult().error);
            expectWithinAbsoluteError ((double) ui.getResult().averageBitrateKbps, 144.0, 3.0, "the average bitrate of a changing-rate file is its mean");
            expectWithinAbsoluteError (ui.getResult().durationSeconds, 2600.0 * 1152.0 / 44100.0, 0.5, "the length is that of the frames");

            // Twelve minutes: read in the background, without the window freezing.
            const auto longFile = writeSilentMp3 (folder, "long.mp3", 720.0, 0, 0, 9);
            ui.analyse (longFile);
            expect (waitUntil ([&] { return ui.getState() == AnalyzerUI::State::showing; }, 120000), "a twelve minute MP3 finishes");
            expect (ui.getResult().opened, ui.getResult().error);
            expectWithinAbsoluteError (ui.getResult().durationSeconds, 720.0, 1.0, "long file length");
            expectEquals (readingValue (ui, "Duration"), juce::String ("12:00.0"));

            // What is said about a compressed file that has sound in it (no encoder is needed: a measured result is made by hand).
            // No bit depth, no claim about the original recording, plain words, and the loudest-peak guidance still applies.
            FileAnalysis::Result measured;
            measured.opened = true; measured.formatName = "MP3"; measured.lossy = true; measured.floatingPoint = true;
            measured.sampleRate = 44100.0; measured.channels = 2; measured.durationSeconds = 30.0; measured.averageBitrateKbps = 128;
            measured.hasIntegratedLoudness = true; measured.integratedLufs = -12.0; measured.samplePeakDb = 0.2; measured.truePeakDb = 1.2;
            measured.samplesAboveFullScale = 4;
            const auto advice = FileAdvice::adviseOn (measured);
            expect (advice.summary.contains ("MP3") && advice.summary.contains ("lossy"), advice.summary);
            juce::String everything = advice.summary;
            for (auto& s : advice.steps) everything += " " + s.text;
            for (auto& s : advice.cannotKnow) everything += " " + s;
            for (auto& s : advice.measured) everything += " " + s;
            expect (! everything.containsIgnoreCase ("-bit"), "no bit depth is claimed: " + everything);
            expect (! everything.containsIgnoreCase ("CD quality"), "an MP3 is not called CD quality");
            expectEquals (Songsmith::UI::Wording::findBanned (everything), juce::String(), "plain words: " + everything);
            expect (advice.measured.joinIntoString (" ").contains ("Average bitrate: about 128 kilobits per second"), advice.measured.joinIntoString (" | "));
        }

        beginTest ("MP3 files (ANA-02): real encoded files of a known signal read the loudness and true peak of what they decode to");
        {
            // A 997 Hz sine at -20 dBFS in both channels, 4 seconds, encoded with LAME (Tests/fixtures/README.md says how). Before
            // encoding it reads -20.0 LUFS and -20.0 dBTP (EBU Tech 3341 level case). A lossy file is not bit for bit the same, and the
            // lower bitrates decode a little under the source. So each file has its own reference: what ffmpeg's EBU R128 meter
            // (an independent decoder and meter) reads for the decoded file, recorded in Tests/fixtures/README.md.
            const juce::File fixtures (ANALYZER_FIXTURES_DIR);
            struct Fixture { const char* file; int expectedKbps; double kbpsTolerance; double referenceLufs, referenceTruePeak; };
            const Fixture fixturesToRead[] = { { "sine-997hz-minus20db-cbr128.mp3", 128, 3.0, -20.5, -20.4 },
                                               { "sine-997hz-minus20db-cbr192.mp3", 192, 3.0, -20.3, -20.3 },
                                               { "sine-997hz-minus20db-cbr320.mp3", 320, 3.0, -20.0, -20.0 },
                                               { "sine-997hz-minus20db-vbr.mp3", 0, 0.0, -20.0, -19.9 } };
            AnalyzerUI ui;
            for (const auto& fx : fixturesToRead)
            {
                const auto mp3 = fixtures.getChildFile (fx.file);
                expect (mp3.existsAsFile(), juce::String (fx.file) + " is in the repository");
                expect (analyseInWindow (ui, mp3), fx.file);
                const auto& r = ui.getResult();
                expect (r.opened, juce::String (fx.file) + ": " + r.error);
                expectEquals (r.formatName, juce::String ("MP3"), fx.file);
                expect (r.lossy, fx.file);
                expectEquals ((int) r.sampleRate, 44100, fx.file);
                expectEquals (r.channels, 2, fx.file);
                expect (r.hasIntegratedLoudness, fx.file);
                // The reference figures are rounded to a tenth of a decibel, so within 0.15 is as close as they can be compared.
                expectWithinAbsoluteError (r.integratedLufs, fx.referenceLufs, 0.15, juce::String (fx.file) + " reads what ffmpeg's EBU R128 meter reads");
                expectWithinAbsoluteError (r.truePeakDb, fx.referenceTruePeak, 0.15, juce::String (fx.file) + " true peak reads what ffmpeg's meter reads");
                expectWithinAbsoluteError (r.integratedLufs, -20.0, 0.6, juce::String (fx.file) + " is within 0.6 dB of the -20 LUFS it was made at");
                expectGreaterOrEqual (r.truePeakDb, r.samplePeakDb - 0.01, "the true peak is never below the sample peak");
                if (fx.expectedKbps > 0)
                    expectWithinAbsoluteError ((double) r.averageBitrateKbps, (double) fx.expectedKbps, fx.kbpsTolerance, juce::String (fx.file) + " average bitrate");
                else
                    expect (r.averageBitrateKbps > 0 && r.averageBitrateKbps < 200, "a variable bitrate file has an average bitrate: " + juce::String (r.averageBitrateKbps));
                // Four seconds, give or take the encoder's start and end padding (under a twentieth of a second).
                expectWithinAbsoluteError (r.durationSeconds, 4.0, 0.12, juce::String (fx.file) + " length");
                expectEquals (readingValue (ui, "Bit Depth and Encoding"), juce::String ("Not applicable"));
                expectEquals (readingNote (ui, "Bit Depth and Encoding"), juce::String ("MP3, lossy"));
                // The charts show the same measurements: two bars at about -20 dBTP, and a waveform of a steady tone.
                expectEquals (ui.getPeakDisplay().getNumBars(), 2);
                expectWithinAbsoluteError (ui.getPeakDisplay().getBarTruePeakDb (0), r.truePeakDb, 0.3, "the left bar");
                expectWithinAbsoluteError (ui.getPeakDisplay().getBarTruePeakDb (1), r.truePeakDb, 0.3, "the right bar");
                const auto& wave = ui.getWaveform().getPeaks();
                expectEquals ((int) wave.size(), Songsmith::FileAnalysis::waveformColumns);
                expectWithinAbsoluteError (wave[wave.size() / 2], 0.1f, 0.01f, "the middle of the tone is its level (-20 dBFS is 0.1)");
            }
        }

        beginTest ("MP3 files (ANA-02): damaged, cut short, or not an MP3 at all, each with an actionable message and no false readings");
        {
            AnalyzerUI ui;
            const auto notMp3 = folder.getChildFile ("not-really.mp3");
            notMp3.replaceWithText ("this is plain text with an mp3 name");
            expect (analyseInWindow (ui, notMp3), "text named .mp3");
            expect (! ui.getResult().opened && ui.getResult().error.contains ("could not be opened"), ui.getResult().error);
            expect (! ui.getResult().hasIntegratedLoudness && ! std::isfinite (ui.getResult().truePeakDb), "no readings are invented");

            const auto empty = folder.getChildFile ("empty.mp3");
            empty.replaceWithData ("", 0);
            expect (analyseInWindow (ui, empty), "empty .mp3");
            expect (! ui.getResult().opened && ui.getResult().error.isNotEmpty());

            // A real MP3 cut short in the middle of a frame: it either reads what is there or says plainly that it cannot,
            // and never shows a loudness it did not measure.
            const auto whole = writeSilentMp3 (folder, "whole.mp3", 4.0, 0, 0, 9);
            juce::MemoryBlock block;
            whole.loadFileAsData (block);
            const auto cutShort = folder.getChildFile ("cut.mp3");
            cutShort.replaceWithData (block.getData(), block.getSize() / 2 + 100);
            expect (analyseInWindow (ui, cutShort), "cut short");
            if (ui.getResult().opened)
                expect (ui.getResult().silent && ! ui.getResult().hasIntegratedLoudness, "what was read is silence, with no invented loudness");
            else
                expect (ui.getResult().error.isNotEmpty(), "the reason is given");

            // The file is never changed.
            juce::MemoryBlock after;
            whole.loadFileAsData (after);
            expect (after == block, "the MP3 was only read");
        }

        beginTest ("True peak is a real measurement: it is above the sample peak when the wave rises between samples, and never relabelled");
        {
            AnalyzerUI ui;
            // A sine at a quarter of the sample rate, phase 45 degrees: every stored sample is 0.707 of the wave's top.
            expect (analyseInWindow (ui, writeWav (folder, "between.wav", 48000.0, 32, true, sine (48000.0, 5.0, 12000.0, 0.0, 2, juce::MathConstants<double>::pi / 4.0))), "read");
            expectWithinAbsoluteError (ui.getResult().samplePeakDb, -3.01, 0.05, "the stored samples are about -3 dBFS");
            expectWithinAbsoluteError (ui.getResult().truePeakDb, 0.0, 0.3, "the wave itself reaches about 0 dBTP");
            expectEquals (readingValue (ui, "True Peak"), Analyzer::formatPeak (ui.getResult().truePeakDb));
            expect (readingValue (ui, "True Peak").endsWith ("dBTP") && ! readingValue (ui, "True Peak").contains ("dBFS"), "the unit says true peak");
            expect (readingNote (ui, "True Peak").startsWith ("Highest stored sample: -3.0 dBFS"), "the sample peak is labelled as a sample peak: " + readingNote (ui, "True Peak"));
            expectGreaterThan (ui.getResult().truePeakDb, ui.getResult().samplePeakDb + 2.0, "true peak is not the sample peak");
        }

        beginTest ("A measurement that cannot be made is said to be missing, never invented");
        {
            AnalyzerUI ui;
            juce::AudioBuffer<float> silence (2, 44100 * 3);
            silence.clear();   // a new buffer holds whatever was in memory: silence has to be asked for
            expect (analyseInWindow (ui, writeWav (folder, "silence.wav", 44100.0, 16, false, silence)), "silent file");
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("Not available"));
            expectEquals (readingNote (ui, "Integrated Loudness"), juce::String ("The file is silent"));
            expectEquals (readingValue (ui, "True Peak"), juce::String ("Silent"));
            expect (ui.getAdvice().summary.contains ("silent"), ui.getAdvice().summary);

            expect (analyseInWindow (ui, writeWav (folder, "tiny.wav", 44100.0, 16, false, sine (44100.0, 0.2, 997.0, -20.0))), "a clip shorter than one measuring window");
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("Not available"));
            expectEquals (readingNote (ui, "Integrated Loudness"), juce::String ("Too short or too quiet to measure"));
            expect (readingValue (ui, "True Peak").endsWith ("dBTP"), "the peak is still measured");
            expectEquals (readingValue (ui, "Duration"), juce::String ("0:00.2"));

            expect (analyseInWindow (ui, writeWav (folder, "quiet.wav", 44100.0, 16, false, sine (44100.0, 5.0, 997.0, -80.0))), "a file far below the measuring gate");
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("Not available"));
        }

        beginTest ("Errors are meaningful, the window stays usable, and another file can be analysed straight after");
        {
            AnalyzerUI ui;
            const auto text = folder.getChildFile ("notes.txt");
            text.replaceWithText ("not audio");
            const auto damaged = folder.getChildFile ("damaged.wav");
            damaged.replaceWithText ("RIFF....WAVEfmt this is not a real wave file");
            const auto empty = folder.getChildFile ("empty.wav");
            empty.replaceWithData ("", 0);
            const auto notReallyFlac = folder.getChildFile ("fake.flac");
            notReallyFlac.replaceWithText ("this is plain text with a flac name");

            const std::pair<juce::File, juce::String> cases[] = {
                { folder.getChildFile ("missing.wav"), "could not be found" },
                { text, "Only WAV, AIFF, FLAC and MP3" },
                { folder, "folder" },
                { damaged, "" },
                { empty, "" },
                { notReallyFlac, "" } };
            for (auto& [file, expected] : cases)
            {
                expect (analyseInWindow (ui, file), "the window answers for " + file.getFileName());
                expect (! ui.getResult().opened, file.getFileName() + " is reported as not analysed");
                expect (ui.getResult().error.isNotEmpty() || file.isDirectory(), file.getFileName() + " has a reason");
                expect (ui.getAdvice().summary.isNotEmpty() && ui.getAdvice().summary.contains ("."), file.getFileName() + ": " + ui.getAdvice().summary);
                if (expected.isNotEmpty())
                    expect (ui.getAdvice().summary.contains (expected), file.getFileName() + ": " + ui.getAdvice().summary);
                expect (! ui.getAdvice().steps.empty(), "an error says what to try");
                for (int i = 0; i < ui.getNumTiles(); ++i)
                    expect (! ui.getTile (i).isVisible(), "no reading boxes for a file that could not be read");
                expect (ui.getAnotherButton().isVisible() && ui.getAnotherButton().isEnabled(), "the button to try another file is there");
                expect (ui.getFileDetailLabel().getText().isNotEmpty(), "the detail line explains");
                expect (text.existsAsFile() && juce::File (text).loadFileAsString() == "not audio", "the original file is untouched");
            }
            // And a good file straight after an error.
            expect (analyseInWindow (ui, writeWav (folder, "good.wav", 44100.0, 16, false, sine (44100.0, 4.0, 997.0, -23.0))), "a good file after the errors");
            expect (ui.getResult().opened);
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("-23.0 LUFS"));
        }

        beginTest ("The reader runs in the background: the window stays responsive, shows progress, and a long file reads to the end");
        {
            // Three minutes of mono 16-bit at 44.1 kHz: a little over 15 MB.
            const auto longFile = writeWav (folder, "long.wav", 44100.0, 16, false, sine (44100.0, 180.0, 997.0, -18.0, 1));
            AnalyzerUI ui;
            ui.analyse (longFile);
            expect (ui.getState() == AnalyzerUI::State::reading, "reading begins at once and the call returns");
            expect (ui.getProgressBar().isVisible() && ui.getCancelButton().isVisible(), "progress and Cancel are shown");
            expect (! ui.getAnotherButton().isVisible(), "no results yet");
            int turns = 0;
            const bool done = waitUntil ([&] { ++turns; return ui.getState() == AnalyzerUI::State::showing; });
            expect (done, "the long file finishes");
            expectGreaterThan (turns, 1, "the message loop kept running while the file was read");
            expect (ui.getResult().opened, ui.getResult().error);
            expectEquals (readingValue (ui, "Duration"), juce::String ("3:00.0"));
            expectWithinAbsoluteError (ui.getResult().integratedLufs, -21.0, 0.15);
        }

        beginTest ("Cancelling and repeating: only the newest file's result is shown, and Cancel delivers nothing");
        {
            const auto first = writeWav (folder, "first.wav", 44100.0, 16, false, sine (44100.0, 60.0, 997.0, -30.0, 1));
            const auto second = writeWav (folder, "second.wav", 44100.0, 16, false, sine (44100.0, 6.0, 997.0, -18.0));
            AnalyzerUI ui;
            ui.analyse (first);
            ui.analyse (second);   // while the first is still being read
            expect (waitUntil ([&] { return ui.getState() == AnalyzerUI::State::showing; }));
            settle (300);
            expectEquals (ui.getFile().getFileName(), juce::String ("second.wav"));
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("-18.0 LUFS"));

            ui.analyse (first);
            ui.getCancelButton().onClick();
            expect (ui.getState() == AnalyzerUI::State::waiting, "Cancel goes back to the start");
            settle (400);
            expect (ui.getState() == AnalyzerUI::State::waiting, "and nothing arrives later from the cancelled file");
            expect (ui.getChooseButton().isVisible() && ! ui.getCancelButton().isVisible());

            // Escape does the same while reading.
            ui.analyse (first);
            expect (ui.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
            expect (ui.getState() == AnalyzerUI::State::waiting);
            expect (! ui.isReading());

            // Another file straight away after a cancel.
            expect (analyseInWindow (ui, second));
            expectEquals (readingValue (ui, "Integrated Loudness"), juce::String ("-18.0 LUFS"));
        }

        beginTest ("The analysis runner delivers only the newest file, on the message thread, and nothing after cancel()");
        {
            std::vector<juce::String> delivered;
            bool onMessageThread = true;
            Analyzer::AnalysisRunner runner ([&] (const juce::File& f, const FileAnalysis::Result& r)
                                             {
                                                 onMessageThread = onMessageThread && juce::MessageManager::getInstance()->isThisTheMessageThread();
                                                 delivered.push_back (f.getFileName() + (r.opened ? " ok" : " failed"));
                                             });
            const auto a = writeWav (folder, "ra.wav", 44100.0, 16, false, sine (44100.0, 90.0, 997.0, -20.0, 1));
            const auto b = writeWav (folder, "rb.wav", 44100.0, 16, false, sine (44100.0, 3.0, 997.0, -20.0));
            runner.start (a);
            runner.start (b);
            expect (waitUntil ([&] { return ! delivered.empty(); }));
            settle (300);
            expectEquals ((int) delivered.size(), 1, "one result for two requests");
            expectEquals (delivered.front(), juce::String ("rb.wav ok"));
            expect (onMessageThread, "delivered on the message thread");

            delivered.clear();
            runner.start (a);
            runner.cancel();
            settle (400);
            expect (delivered.empty(), "a cancelled read delivers nothing");
            expect (runner.getProgress() <= 1.0f, "progress is never more than all of the file");
        }

        beginTest ("Dropping files: one file is analysed, extra files are mentioned, and a folder or other file gets a clear message");
        {
            AnalyzerUI ui;
            expect (ui.isInterestedInFileDrag ({ "anything.wav" }));
            const auto g = writeWav (folder, "dropA.wav", 44100.0, 16, false, sine (44100.0, 3.0, 997.0, -23.0));
            const auto h = writeWav (folder, "dropB.wav", 44100.0, 16, false, sine (44100.0, 3.0, 997.0, -10.0));
            ui.filesDropped ({ g.getFullPathName(), h.getFullPathName() }, 10, 10);
            expect (waitUntil ([&] { return ui.getState() == AnalyzerUI::State::showing; }));
            expectEquals (ui.getFile().getFileName(), juce::String ("dropA.wav"));
            expect (ui.getAdvice().summary.isNotEmpty());
            expect (ui.getResult().opened);

            ui.filesDropped ({ folder.getFullPathName() }, 10, 10);
            expect (waitUntil ([&] { return ui.getState() == AnalyzerUI::State::showing; }));
            expect (ui.getAdvice().summary.contains ("folder"), ui.getAdvice().summary);

            ui.filesDropped ({ folder.getChildFile ("notes.txt").getFullPathName() }, 10, 10);
            expect (waitUntil ([&] { return ui.getState() == AnalyzerUI::State::showing && ! ui.getResult().opened; }));
            expect (ui.getAdvice().summary.contains ("WAV, AIFF, FLAC and MP3"), ui.getAdvice().summary);
            ui.filesDropped ({}, 0, 0);   // nothing dropped: nothing happens
        }

        beginTest ("Back to the start: the waiting screen shows the drop area and the choose button, and the file is never touched");
        {
            AnalyzerUI ui;
            expect (ui.getState() == AnalyzerUI::State::waiting);
            expect (ui.getChooseButton().isVisible() && ui.getChooseButton().getButtonText() == "Choose File...");
            expect (ui.getDropPanelBounds().contains (ui.getChooseButton().getBounds()), "the button sits inside the drop area, beside what it opens");
            expect (! ui.getAnotherButton().isVisible() && ! ui.getCancelButton().isVisible() && ! ui.getProgressBar().isVisible());
            const auto f = writeWav (folder, "keep.wav", 44100.0, 16, false, sine (44100.0, 3.0, 997.0, -23.0));
            const auto before = f.loadFileAsString();
            const auto modified = f.getLastModificationTime();
            expect (analyseInWindow (ui, f));
            ui.reset();
            expect (ui.getState() == AnalyzerUI::State::waiting);
            expect (f.loadFileAsString() == before && f.getLastModificationTime() == modified, "the analysed file is byte for byte the same");
            expectEquals (folder.findChildFiles (juce::File::findFiles, false, "*.tmp").size(), 0, "no temporary files");
        }

        beginTest ("The True Peak chart (ANA-03) draws each channel's measured peak at the right height, and shows what it is");
        {
            AnalyzerUI ui;
            auto& chart = ui.getPeakDisplay();

            // Left at -6 dBFS and right at -12 dBFS: each bar is its own channel's peak, and the heights follow the numbers.
            expect (analyseInWindow (ui, writeWav (folder, "lr.wav", 48000.0, 32, true, sineLevels (48000.0, 4.0, 997.0, { -6.0, -12.0 }))), "left and right");
            expectEquals (chart.getNumBars(), 2);
            expectEquals (chart.getBarName (0), juce::String ("Left"));
            expectEquals (chart.getBarName (1), juce::String ("Right"));
            expectWithinAbsoluteError (chart.getBarTruePeakDb (0), -6.0, 0.1, "the left bar is the left channel's true peak");
            expectWithinAbsoluteError (chart.getBarTruePeakDb (1), -12.0, 0.1, "the right bar is the right channel's true peak");
            expectEquals (chart.getBarReadout (0), juce::String ("-6.0"));
            expectEquals (chart.getBarReadout (1), juce::String ("-12.0"));
            expectWithinAbsoluteError (ui.getResult().truePeakDb, chart.getBarTruePeakDb (0), 0.001, "the headline true peak is the highest bar");
            expectGreaterThan (chart.yForDb (chart.getBarTruePeakDb (1)), chart.yForDb (chart.getBarTruePeakDb (0)), "a lower level is drawn lower");

            // The scale: 0 dBTP, the top and the bottom are where the numbers say.
            const auto scale = chart.getScaleBounds();
            expectWithinAbsoluteError ((double) chart.yForDb ((double) Analyzer::PeakScale::minDb), (double) scale.getBottom(), 0.01, "the bottom of the scale");
            expectWithinAbsoluteError ((double) chart.yForDb ((double) Analyzer::PeakScale::maxDb), (double) scale.getY(), 0.01, "the top of the scale");
            const double zeroFraction = (0.0 - Analyzer::PeakScale::minDb) / (double) (Analyzer::PeakScale::maxDb - Analyzer::PeakScale::minDb);
            expectWithinAbsoluteError ((double) chart.yForDb (0.0), (double) scale.getBottom() - zeroFraction * (double) scale.getHeight(), 0.01, "0 dBTP is marked at its place");
            expectWithinAbsoluteError ((double) (chart.yForDb (-6.0) - chart.yForDb (-12.0)), -(double) scale.getHeight() * 6.0 / (double) (Analyzer::PeakScale::maxDb - Analyzer::PeakScale::minDb), 0.01, "the scale is linear in decibels");
            expectEquals (chart.yForDb (40.0), chart.yForDb ((double) Analyzer::PeakScale::maxDb), "a level far above the top sits on the top edge");
            expectEquals (chart.yForDb (-90.0), chart.yForDb ((double) Analyzer::PeakScale::minDb), "a level far below the bottom sits on the bottom edge");
            expect (! chart.isOverFullScale(), "nothing is over 0 dBTP");

            // Above 0 dBTP: drawn above the 0 line, with a plus sign, and flagged. Stored samples reach 1.3 on the left.
            expect (analyseInWindow (ui, writeWav (folder, "over.wav", 48000.0, 32, true, sineLevels (48000.0, 4.0, 997.0, { 2.0, -3.0 }))), "over");
            expect (chart.isOverFullScale(), "a true peak above 0 dBTP is flagged");
            expectGreaterThan (chart.getBarTruePeakDb (0), 1.5, "the left bar is above 0 dBTP");
            expect (chart.getBarReadout (0).startsWith ("+"), "an over reading has a plus sign: " + chart.getBarReadout (0));
            expect (chart.yForDb (chart.getBarTruePeakDb (0)) < chart.yForDb (0.0), "a level over 0 dBTP is drawn above the 0 line");
            expect (! chart.getBarReadout (1).startsWith ("+"), "the right channel is below 0 dBTP");
            expectGreaterThan (ui.getTile (1).getValue().getText().length(), 0);
            expect (ui.getTile (1).getValue().findColour (juce::Label::textColourId) == Analyzer::PeakScale::over, "the headline number is red when over 0 dBTP");
            expect (ui.getTile (1).getValue().getText().contains ("dBTP"), "and says dBTP in words");

            // Sample peak and true peak are two different readings, drawn differently, and never swapped.
            expect (analyseInWindow (ui, writeWav (folder, "between2.wav", 48000.0, 32, true, sine (48000.0, 5.0, 12000.0, 0.0, 2, juce::MathConstants<double>::pi / 4.0))), "between samples");
            expectWithinAbsoluteError (chart.getBarSamplePeakDb (0), -3.01, 0.05, "the tick is the highest stored sample");
            expectGreaterThan (chart.getBarTruePeakDb (0), chart.getBarSamplePeakDb (0) + 2.0, "and it is below the true peak bar");
            expect (chart.getLegend().getText().contains ("true peak") && chart.getLegend().getText().contains ("stored sample"), chart.getLegend().getText());

            // One bar for mono, one for more than two channels (the highest of all).
            expect (analyseInWindow (ui, writeWav (folder, "mono-chart.wav", 44100.0, 24, false, sine (44100.0, 3.0, 997.0, -9.0, 1))), "mono");
            expectEquals (chart.getNumBars(), 1);
            expectEquals (chart.getBarName (0), juce::String ("Mono"));
            expectWithinAbsoluteError (chart.getBarTruePeakDb (0), -9.0, 0.1);
            expect (analyseInWindow (ui, writeWav (folder, "six.wav", 48000.0, 32, true, sineLevels (48000.0, 2.0, 997.0, { -20.0, -18.0, -16.0, -14.0, -9.0, -12.0 }))), "six channels");
            expectEquals (chart.getNumBars(), 1);
            expectEquals (chart.getBarName (0), juce::String ("All"));
            expectWithinAbsoluteError (chart.getBarTruePeakDb (0), -9.0, 0.1, "the highest of all six");

            // Silence draws nothing and says so.
            expect (analyseInWindow (ui, writeWav (folder, "silent-chart.wav", 44100.0, 16, false, sine (44100.0, 3.0, 997.0, -200.0))), "silent");
            expect (chart.isSilent());
            expectEquals (chart.getBarReadout (0), juce::String ("Silent"));
            expect (chart.getDescription().contains ("silent"), chart.getDescription());

            // It says what it is: a whole-file result, never a live meter. And it is named for screen readers, with the numbers.
            expect (analyseInWindow (ui, writeWav (folder, "lr2.wav", 48000.0, 32, true, sineLevels (48000.0, 3.0, 997.0, { -6.0, -12.0 }))), "again");
            expectEquals (chart.getHeading().getText(), juce::String ("Highest peak in the file"));
            expect (chart.getTooltip().contains ("not a live meter") && chart.getTooltip().contains ("0 dBTP"), chart.getTooltip());
            expect (chart.getTitle().isNotEmpty() && chart.getDescription().contains ("Left: -6.0 dBTP") && chart.getDescription().contains ("Right: -12.0 dBTP"), chart.getDescription());
            expect (chart.isAccessible(), "visible to screen readers");
            expect (chart.isVisible(), "shown with the results");
        }

        beginTest ("The waveform overview (ANA-03) is mapped to the file, capped in size, and never mistaken for loudness or true peak");
        {
            AnalyzerUI ui;
            auto& wave = ui.getWaveform();

            // Five seconds of silence, then five seconds of a tone at -6 dBFS: the first half is empty, the second is not.
            juce::AudioBuffer<float> halves (2, 10 * 48000);
            halves.clear();
            for (int i = 5 * 48000; i < halves.getNumSamples(); ++i)
            {
                const auto v = (float) (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 997.0 * i / 48000.0));
                halves.setSample (0, i, v);
                halves.setSample (1, i, v);
            }
            expect (analyseInWindow (ui, writeWav (folder, "halves.wav", 48000.0, 24, false, halves)), "halves");
            expectEquals ((int) wave.getPeaks().size(), Songsmith::FileAnalysis::waveformColumns);
            const auto& peaks = wave.getPeaks();
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                if (i < peaks.size() / 2 - 2)
                    expectEquals (peaks[i], 0.0f, "the silent half is empty at slice " + juce::String ((int) i));
                if (i > peaks.size() / 2 + 2)
                    expectWithinAbsoluteError (peaks[i], 0.5f, 0.01f, "the tone's slice " + juce::String ((int) i) + " is its level");
            }
            expectGreaterThan (wave.getLoudestPosition(), 0.5, "the biggest sound is in the second half");
            expectEquals (wave.getEnd().getText(), juce::String ("0:10.0"));
            expectEquals (wave.getStart().getText(), juce::String ("0:00"));
            expectWithinAbsoluteError (wave.getDurationSeconds(), 10.0, 0.01);
            expectEquals (wave.heightOfSlice (10), 0.0f, "a silent slice has no height");
            expectWithinAbsoluteError (wave.heightOfSlice ((int) peaks.size() - 10), wave.getPlotBounds().getHeight() * 0.5f * 0.5f, 0.6f, "a slice is as tall as its sample, out of full scale");

            // A long file does not grow the picture.
            const auto longFile = writeSilentMp3 (folder, "long-wave.mp3", 600.0, 0, 0, 9);
            expect (analyseInWindow (ui, longFile), "ten minutes");
            expectEquals ((int) wave.getPeaks().size(), Songsmith::FileAnalysis::waveformColumns, "ten minutes still hold the same number of slices");
            expect (ui.getResult().waveformPeaks.size() <= (size_t) Songsmith::FileAnalysis::waveformColumns, "a capped amount of memory");

            // A very short file holds one slice per sample, never more slices than it has samples.
            expect (analyseInWindow (ui, writeWav (folder, "tiny.wav", 44100.0, 16, false, sine (44100.0, 0.005, 997.0, -10.0))), "tiny");
            expectLessOrEqual ((int) wave.getPeaks().size(), (int) std::lround (44100.0 * 0.005));

            // It says what it is not: no loudness, no true peak. Red only where the sound passes the top.
            expect (wave.getHeading().getText().contains ("how big the sound is"), wave.getHeading().getText());
            expect (wave.getNote().getText().contains ("not a loudness chart"), wave.getNote().getText());
            expect (wave.getTooltip().contains ("not a loudness chart") && wave.getTooltip().contains ("does not show true peak"), wave.getTooltip());
            expect (wave.getTitle().isNotEmpty() && wave.getDescription().contains ("shows size, not loudness"), wave.getDescription());
            expect (wave.isAccessible(), "visible to screen readers");

            // A silent file draws nothing and says so.
            expect (analyseInWindow (ui, writeWav (folder, "silent-wave.wav", 44100.0, 16, false, sine (44100.0, 3.0, 997.0, -200.0))), "silent");
            expect (wave.isSilent() && wave.getNote().getText().contains ("silent"), wave.getNote().getText());
        }

        beginTest ("The dashboard (ANA-03): the charts show with the results only, sit inside the window, and the True Peak chart is the principal one");
        {
            namespace L = Analyzer::Layout;
            AnalyzerUI ui;
            const juce::Rectangle<int> window (0, 0, L::designWidth, L::designHeight);
            expect (! ui.getPeakDisplay().isVisible() && ! ui.getWaveform().isVisible(), "no charts on the waiting screen");

            ui.analyse (writeWav (folder, "dash-slow.wav", 44100.0, 16, false, sine (44100.0, 90.0, 997.0, -20.0, 1)));
            expect (ui.isReading());
            expect (! ui.getPeakDisplay().isVisible() && ! ui.getWaveform().isVisible(), "no charts while reading");
            ui.reset();

            expect (analyseInWindow (ui, writeWav (folder, "dash.wav", 48000.0, 24, false, songLike (48000.0, 12.0))), "a file");
            expect (ui.getPeakDisplay().isVisible() && ui.getWaveform().isVisible(), "both charts with the results");
            juce::Array<juce::Component*> parts { &ui.getPeakDisplay(), &ui.getWaveform() };
            for (int i = 0; i < ui.getNumTiles(); ++i)
                parts.add (&ui.getTile (i));
            for (auto* p : parts)
            {
                expect (window.contains (p->getBounds()), p->getTitle() + " is inside the window");
                expect (p->getBottom() <= ui.getGuidanceBounds().getY(), p->getTitle() + " is above the guidance");
                expect (p->getY() >= ui.getTitleBounds().getBottom(), p->getTitle() + " is below the title");
            }
            for (int i = 0; i < parts.size(); ++i)
                for (int j = i + 1; j < parts.size(); ++j)
                    expect (! parts[i]->getBounds().intersects (parts[j]->getBounds()), parts[i]->getTitle() + " and " + parts[j]->getTitle() + " do not overlap");

            // True Peak is the principal picture: the tallest thing on the screen, to the right, and drawn with its own tooltip.
            const auto chartBounds = ui.getPeakDisplay().getBounds();
            for (auto* p : parts)
                if (p != &ui.getPeakDisplay())
                    expectGreaterOrEqual (chartBounds.getHeight(), p->getHeight(), "the True Peak chart is at least as tall as " + p->getTitle());
            expectGreaterThan (chartBounds.getX(), ui.getWaveform().getRight() - 1, "the True Peak chart is on the right");
            expectGreaterThan (chartBounds.getHeight(), 150, "the True Peak chart is large enough to read");

            // Integrated Loudness is the large number but not a second chart: the reading is a plain tile, larger than the others.
            expectGreaterThan (ui.getTile (0).getValue().getFont().getHeight(), ui.getTile (1).getValue().getFont().getHeight(), "the loudness figure is the larger number");
            expectGreaterOrEqual (ui.getTile (0).getWidth(), ui.getTile (2).getWidth(), "and its box is the larger one");
            expect (ui.getTile (0).getReading().caption == "Integrated Loudness" && ui.getTile (0).getTooltip().contains ("There is no single number a track has to reach"), "no loudness pass or fail");

            // The facts are compact, below the charts: sample rate, bit depth and encoding, duration and channels.
            const juce::StringArray captions { "Sample Rate", "Bit Depth and Encoding", "Duration", "Channels" };
            for (int i = 0; i < 4; ++i)
            {
                expectEquals (ui.getTile (2 + i).getReading().caption, captions[i]);
                expectGreaterOrEqual (ui.getTile (2 + i).getY(), ui.getPeakDisplay().getBottom(), captions[i] + " is below the charts");
            }

            // A file that cannot be read has no charts, and the advice starts under the name.
            expect (analyseInWindow (ui, folder.getChildFile ("not-there.wav")), "missing");
            expect (! ui.getPeakDisplay().isVisible() && ! ui.getWaveform().isVisible(), "no charts for an error");
            ui.reset();
            expect (! ui.getPeakDisplay().isVisible() && ui.getPeakDisplay().getNumBars() == 0, "reset clears the chart");
            expect (ui.getWaveform().getPeaks().empty(), "and the waveform");
        }

        beginTest ("Layout: in every state the controls sit inside the window, the reading boxes do not overlap, and the guidance has room");
        {
            namespace L = Analyzer::Layout;
            AnalyzerUI ui;
            const juce::Rectangle<int> window (0, 0, L::designWidth, L::designHeight);
            expect (analyseInWindow (ui, writeWav (folder, "layout.wav", 48000.0, 24, false, songLike (48000.0, 12.0))), "a file");
            expectEquals (ui.getNumTiles(), 6);
            for (int i = 0; i < ui.getNumTiles(); ++i)
            {
                auto& t = ui.getTile (i);
                expect (t.isVisible() && window.contains (t.getBounds()), "tile " + juce::String (i) + " is inside the window");
                expect (t.getBottom() <= ui.getGuidanceBounds().getY(), "tile " + juce::String (i) + " is above the guidance");
                for (int j = i + 1; j < ui.getNumTiles(); ++j)
                    expect (! t.getBounds().intersects (ui.getTile (j).getBounds()), "tiles " + juce::String (i) + " and " + juce::String (j) + " do not overlap");
                expect (t.getTooltip().isNotEmpty() && t.getTitle().isNotEmpty(), "tile " + juce::String (i) + " explains itself and has a name for screen readers");
            }
            expect (window.contains (ui.getGuidanceBounds()) && window.contains (ui.getAnotherButton().getBounds()));
            expect (ui.getAnotherButton().getY() >= ui.getGuidanceBounds().getBottom(), "the button is below the guidance");
            expect (ui.getAnotherButton().getButtonText() == "Analyse Another File");
            expectGreaterThan (ui.getGuidance().getView().getHeight(), 40, "there is guidance to read");
            expect (ui.getTitleBounds().getBottom() <= ui.getTile (0).getY(), "the title is above the readings");
        }

        beginTest ("Plain words and text fit, at the smallest, the default and the largest window, in every screen");
        {
            const auto good = writeWav (folder, "words.wav", 48000.0, 24, false, songLike (48000.0, 12.0));
            const auto clipped = [&]
            {
                auto b = sine (44100.0, 4.0, 997.0, 3.0);
                return writeWav (folder, "clipped.wav", 44100.0, 16, false, b);
            }();
            const auto missing = folder.getChildFile ("not-there.wav");
            const auto mp3 = writeSilentMp3 (folder, "words.mp3", 3.0, 0, 0, 14);   // 320 kilobits per second: the longest wording
            for (int width : { Analyzer::Layout::designWidth * 3 / 4, Analyzer::Layout::designWidth, Analyzer::Layout::designWidth * 2 })
            {
                const int height = width * Analyzer::Layout::designHeight / Analyzer::Layout::designWidth;
                Analyzer::AnalyzerRoot root;
                root.setSize (width, height);
                auto& ui = root.content;
                const auto context = "Audio Analyzer at " + juce::String (width) + " px";
                expectWithinAbsoluteError (ui.getTransform().mat00, (float) width / (float) Analyzer::Layout::designWidth, 0.01f, context + ": scales as a whole");

                struct Screen { juce::String name; std::function<void()> show; };
                const Screen screens[] = {
                    { "waiting", [&] { ui.reset(); } },
                    { "reading", [&] { ui.analyse (writeWav (folder, "slow-words.wav", 44100.0, 16, false, sine (44100.0, 120.0, 997.0, -20.0, 1))); } },
                    { "a good file", [&] { analyseInWindow (ui, good); } },
                    { "a clipped file", [&] { analyseInWindow (ui, clipped); } },
                    { "an MP3 file", [&] { analyseInWindow (ui, mp3); } },
                    { "a missing file", [&] { analyseInWindow (ui, missing); } } };
                for (auto& s : screens)
                {
                    s.show();
                    settle (60);
                    const auto where = context + ", " + s.name;
                    juce::StringArray texts;
                    Songsmith::UI::Wording::collect (ui, texts);
                    for (auto& line : ui.getGuidance().getView().getAllText())
                        texts.add (line);
                    for (auto& line : ui.getAdvice().measured) texts.add (line);
                    expectGreaterThan (texts.size(), 8, where + ": found the text");
                    Songsmith::UI::Wording::expectPlainWords (*this, texts, where);
                    Songsmith::UI::Wording::expectTextFits (*this, ui, ui.getLookAndFeel(), where);
                    Songsmith::UI::Testing::checkEditorFonts (*this, ui, where);
                }
                ui.reset();
            }
        }

        beginTest ("Accessibility: buttons are reachable by keyboard, named, and have a tooltip that explains them");
        {
            AnalyzerUI ui;
            for (auto* b : { &ui.getChooseButton(), &ui.getAnotherButton(), &ui.getCancelButton() })
            {
                expect (b->getButtonText().isNotEmpty(), "the button has words on it");
                expect (b->getWantsKeyboardFocus(), b->getButtonText() + " can be reached from the keyboard");
                expect (b->getTooltip().isNotEmpty(), b->getButtonText() + " has a tooltip");
                expect (b->isAccessible(), b->getButtonText() + " is visible to screen readers");
            }
            expect (ui.getTitle().isNotEmpty() && ui.getDescription().isNotEmpty(), "the window has a name and a description");
            analyseInWindow (ui, writeWav (folder, "access.wav", 44100.0, 16, false, sine (44100.0, 3.0, 997.0, -23.0)));
            for (int i = 0; i < ui.getNumTiles(); ++i)
                expect (ui.getTile (i).getTitle().isNotEmpty() && ui.getTile (i).getDescription().isNotEmpty(), "tile " + juce::String (i) + " reads out its name and value");
            expect (ui.getGuidance().getTitle().isNotEmpty(), "the guidance area has a name");
        }

        folder.deleteRecursively();
    }
};

static AudioAnalyzerTests audioAnalyzerTests;
static SongsmithFileAdvisorTests sharedFileAdvisorTests { "AudioAnalyzer" };   // REC-01: the analysis and advice the app is built on
static SongsmithUITests sharedUITests { "AudioAnalyzer" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// AudioAnalyzerTests --screenshots <folder> [--scale 2]: renders the three screens (waiting, results, an error) to PNGs.
static int renderScreenshots (const juce::File& folder, float scale)
{
    folder.createDirectory();
    const auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("songsmith-rec02-shots", "", false);
    work.createDirectory();

    auto save = [&] (Analyzer::AnalyzerRoot& root, const juce::String& name)
    {
        const auto image = root.createComponentSnapshot (root.getLocalBounds(), true, scale);
        const auto file = folder.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (! out.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, out))
            return false;
        std::printf ("Wrote %s\n", file.getFullPathName().toRawUTF8());
        return true;
    };

    bool ok = true;
    Analyzer::AnalyzerRoot root;
    ok = save (root, "audio-analyzer-waiting.png") && ok;

    const auto song = writeWav (work, "My Song Master.wav", 48000.0, 24, false, songLike (48000.0, 24.0));
    root.content.analyse (song);
    waitUntil ([&] { return root.content.getState() == Analyzer::AnalyzerComponent::State::showing; });
    settle (80);
    ok = save (root, "audio-analyzer-results.png") && ok;

    // A loud master whose left channel passes 0 dBTP, to show the red above the line.
    {
        auto loud = songLike (48000.0, 24.0);
        loud.applyGain (0, 0, loud.getNumSamples(), 1.95f);   // only the loudest moments of the left channel pass full scale
        loud.applyGain (1, 0, loud.getNumSamples(), 1.5f);    // the right channel stays below it
        root.content.analyse (writeWav (work, "Loud Master.wav", 48000.0, 32, true, loud));
        waitUntil ([&] { return root.content.getState() == Analyzer::AnalyzerComponent::State::showing; });
        settle (80);
        ok = save (root, "audio-analyzer-results-over.png") && ok;
    }

    work.getChildFile ("Not A Song.txt").replaceWithText ("This is not audio.");
    root.content.analyse (work.getChildFile ("Not A Song.txt"));
    waitUntil ([&] { return root.content.getState() == Analyzer::AnalyzerComponent::State::showing; });
    settle (80);
    ok = save (root, "audio-analyzer-error.png") && ok;

    root.content.reset();
    work.deleteRecursively();
    return ok ? 0 : 1;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--screenshots")
        {
            float scale = 1.0f;
            for (int j = 1; j + 1 < argc; ++j)
                if (juce::String (argv[j]) == "--scale")
                    scale = juce::jlimit (1.0f, 4.0f, juce::String (argv[j + 1]).getFloatValue());
            return renderScreenshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("AudioAnalyzer");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
