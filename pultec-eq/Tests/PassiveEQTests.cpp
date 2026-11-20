// Songsmith Passive EQ test runner: the plugin (identity, parameters, state, bypass, presets, editor)
// and the shared Songsmith checks. The engine is tested in EngineTests.cpp.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <random>
#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithControlSheet.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "../../shared/Tests/SongsmithHeaderTests.h"
#include "../../shared/Tests/SongsmithDiagnosticsTests.h"
#include "../../shared/Tests/SongsmithGuidanceTests.h"
#include "SongsmithWording.h"

namespace
{
    void setParam (SongsmithPassiveEQProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        jassert (rp != nullptr);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    float getParam (const SongsmithPassiveEQProcessor& p, const char* id)
    {
        return p.apvts.getRawParameterValue (id)->load();
    }

    std::vector<float> noise (int n, float amp, unsigned seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> dist (-amp, amp);
        std::vector<float> v ((size_t) n);
        for (auto& s : v)
            s = dist (rng);
        return v;
    }

    std::vector<float> sine (int n, float amp, double hz, double fs)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) i / fs);
        return v;
    }

    void prepare (SongsmithPassiveEQProcessor& p, double fs, int block, int numChannels = 2)
    {
        const auto set = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        p.setBusesLayout (layout);
        p.setRateAndBufferSizeDetails (fs, block);
        p.prepareToPlay (fs, block);
    }

    // Runs 'in' (one vector per channel) through the processor in blocks of 'block' samples.
    std::vector<std::vector<float>> run (SongsmithPassiveEQProcessor& p, std::vector<std::vector<float>> in, int block)
    {
        const int channels = (int) in.size(), total = (int) in[0].size();
        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            juce::AudioBuffer<float> buffer (channels, n);
            for (int c = 0; c < channels; ++c)
                buffer.copyFrom (c, 0, in[(size_t) c].data() + pos, n);
            p.processBlock (buffer, midi);
            for (int c = 0; c < channels; ++c)
                std::copy (buffer.getReadPointer (c), buffer.getReadPointer (c) + n, in[(size_t) c].begin() + pos);
        }
        return in;
    }

    float rms (const std::vector<float>& v, size_t from = 0)
    {
        double sum = 0.0;
        for (size_t i = from; i < v.size(); ++i)
            sum += (double) v[i] * v[i];
        return (float) std::sqrt (sum / (double) (v.size() - from));
    }

    juce::MemoryBlock stateOf (SongsmithPassiveEQProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    const char* const soundIds[] = { Peq::lowFrequencyId, Peq::lowBoostId, Peq::lowCutId, Peq::highBoostFrequencyId,
                                     Peq::highBoostId, Peq::highBandwidthId, Peq::highCutFrequencyId, Peq::highCutId };
}

//==============================================================================
class PassiveEQPluginTests final : public juce::UnitTest
{
public:
    PassiveEQPluginTests() : juce::UnitTest ("Songsmith Passive EQ plugin", "PassiveEQ") {}

    void runTest() override
    {
        beginTest ("Identity: the product name and the codes hosts store sessions under never change");
        {
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Passive EQ"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));
            expectEquals (juce::String (PEQ_MANUFACTURER_CODE), juce::String ("Tnyw"), "shared with the whole family");
            expectEquals (juce::String (PEQ_PLUGIN_CODE), juce::String ("Peq1"), "this plugin's own code");
            for (auto* other : { "Eq3b", "Sfc1", "Mxlm", "Cmp1", "Opt1" })
                expect (juce::String (PEQ_PLUGIN_CODE) != juce::String (other), juce::String ("differs from ") + other);
        }

        beginTest ("Parameter IDs, ranges and defaults are stable");
        {
            SongsmithPassiveEQProcessor p;
            struct Row { const char* id; float lo, hi, def; const char* name; };
            const Row rows[] = {
                { "low_frequency", 0.0f, 3.0f, 2.0f, "Low Frequency" },
                { "low_boost", 0.0f, 13.5f, 0.0f, "Low Boost" },
                { "low_cut", 0.0f, 17.5f, 0.0f, "Low Cut" },
                { "high_boost_frequency", 0.0f, 6.0f, 3.0f, "High Boost Frequency" },
                { "high_boost", 0.0f, 18.0f, 0.0f, "High Boost" },
                { "high_bandwidth", 0.0f, 10.0f, 5.0f, "High Boost Bandwidth" },
                { "high_cut_frequency", 0.0f, 2.0f, 1.0f, "High Cut Frequency" },
                { "high_cut", 0.0f, 16.0f, 0.0f, "High Cut" },
                { "bypass", 0.0f, 1.0f, 0.0f, "Bypass" },
                // PEQ-04 (version 2): the mode and the Mid Shaper and Frequency Filter controls. Defaults leave the sound unchanged.
                { "eq_mode", 0.0f, 2.0f, 0.0f, "EQ Mode" },
                { "mid_low_frequency", 0.0f, 4.0f, 2.0f, "Low-Mid Boost Frequency" },
                { "mid_low_boost", 0.0f, 10.0f, 0.0f, "Low-Mid Boost" },
                { "mid_dip_frequency", 0.0f, 9.0f, 4.0f, "Mid Dip Frequency" },
                { "mid_dip", 0.0f, 10.0f, 0.0f, "Mid Dip" },
                { "mid_high_frequency", 0.0f, 4.0f, 2.0f, "High-Mid Boost Frequency" },
                { "mid_high_boost", 0.0f, 10.0f, 0.0f, "High-Mid Boost" },
                { "high_pass_frequency", 0.0f, 7.0f, 0.0f, "High-Pass Frequency" },
                { "low_pass_frequency", 0.0f, 5.0f, 0.0f, "Low-Pass Frequency" },
            };
            expectEquals (p.getParameters().size(), (int) std::size (rows), "the nine original parameters (never renamed) and the nine PEQ-04 ones, no more");
            for (const auto& r : rows)
            {
                auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (r.id));
                expect (rp != nullptr, juce::String (r.id) + " exists");
                if (rp == nullptr)
                    continue;
                expectEquals (rp->getName (100), juce::String (r.name));
                expectWithinAbsoluteError (rp->getNormalisableRange().start, r.lo, 1.0e-6f);
                expectWithinAbsoluteError (rp->getNormalisableRange().end, r.hi, 1.0e-6f);
                expectWithinAbsoluteError (rp->convertFrom0to1 (rp->getDefaultValue()), r.def, 1.0e-4f);
            }
            expectEquals (Peq::frequencyNames (SongsmithPassive::lowFrequencies).joinIntoString ("/"), juce::String ("20 Hz/30 Hz/60 Hz/100 Hz"));
            expectEquals (Peq::frequencyNames (SongsmithPassive::highBoostFrequencies).joinIntoString ("/"),
                          juce::String ("3 kHz/4 kHz/5 kHz/8 kHz/10 kHz/12 kHz/16 kHz"));
            expectEquals (Peq::frequencyNames (SongsmithPassive::highCutFrequencies).joinIntoString ("/"), juce::String ("5 kHz/10 kHz/20 kHz"));
            expect (p.getBypassParameter() == p.apvts.getParameter (Peq::bypassId), "the host's own Bypass is the plugin's Bypass");
        }

        beginTest ("Buses: stereo and mono are supported, nothing else");
        {
            SongsmithPassiveEQProcessor p;
            auto layout = [] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
            {
                juce::AudioProcessor::BusesLayout l;
                l.inputBuses.add (in);
                l.outputBuses.add (out);
                return l;
            };
            expect (p.isBusesLayoutSupported (layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
            expect (p.isBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
            expect (! p.isBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())));
            expect (! p.isBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())));
            expectEquals (p.getLatencySamples(), 0, "no latency");
            expectEquals (p.getTailLengthSeconds(), 0.0);
        }

        beginTest ("A new instance leaves the sound exactly as it was, at any block size");
        for (int block : { 1, 64, 480, 4096 })
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, block);
            const auto signal = noise (9600, 0.5f, 3);
            const auto out = run (p, { signal, signal }, block);
            expect (out[0] == signal && out[1] == signal, "bit-identical at a block size of " + juce::String (block));
        }

        beginTest ("The sound follows the parameters, including host automation, at any block size");
        {
            const auto signal = noise (96000, 0.3f, 4);
            std::vector<float> reference;
            for (int block : { 64, 480, 4096 })
            {
                SongsmithPassiveEQProcessor p;
                prepare (p, 48000.0, block);
                setParam (p, Peq::lowBoostId, 10.0f);
                setParam (p, Peq::highBoostId, 8.0f);
                setParam (p, Peq::highCutId, 6.0f);
                const auto out = run (p, { signal, signal }, block);
                expect (out[0] == out[1], "identical channels stay identical");
                expect (out[0] != signal, "the sound is changed");
                if (reference.empty())
                    reference = out[0];
                else
                    expect (out[0] == reference, "the same result at a block size of " + juce::String (block));
            }

            // Each control does what its name says, as the engine's own response predicts.
            struct Case { const char* id; float value; double hz; const char* what; };
            const Case cases[] = { { Peq::lowBoostId, 12.0f, 60.0, "Low Boost raises 60 Hz" },
                                   { Peq::lowCutId, 12.0f, 200.0, "Low Cut lowers the low middle" },
                                   { Peq::highBoostId, 12.0f, 8000.0, "High Boost raises 8 kHz" },
                                   { Peq::highCutId, 12.0f, 15000.0, "High Cut lowers 15 kHz" } };
            for (const auto& c : cases)
            {
                SongsmithPassiveEQProcessor p;
                prepare (p, 48000.0, 480);
                setParam (p, c.id, c.value);
                const auto tone = sine (48000, 0.1f, c.hz, 48000.0);
                const auto out = run (p, { tone, tone }, 480);
                const float measured = 20.0f * std::log10 (rms (out[0], 24000) / rms (tone, 24000));

                SongsmithPassive::Engine e;
                e.prepare (48000.0);
                e.setParams (p.getEngineParams());
                expectWithinAbsoluteError (measured, e.responseDb (c.hz), 0.3f, c.what);
                expect ((c.id == Peq::lowBoostId || c.id == Peq::highBoostId) ? measured > 3.0f : measured < -3.0f, c.what);
            }
        }

        beginTest ("The frequency switches move the boost, with the engine's own response");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Peq::highBoostId, 12.0f);
            float atFrequency[2] {};
            for (int i = 0; i < 2; ++i)
            {
                setParam (p, Peq::highBoostFrequencyId, i == 0 ? 3.0f : 6.0f);   // 8 kHz, then 16 kHz
                p.reset();
                const auto tone = sine (48000, 0.1f, 16000.0, 48000.0);
                const auto out = run (p, { tone, tone }, 480);
                atFrequency[i] = 20.0f * std::log10 (rms (out[0], 24000) / rms (tone, 24000));
            }
            expect (atFrequency[1] > atFrequency[0] + 3.0f, "the 16 kHz setting lifts 16 kHz more than the 8 kHz setting does");
        }

        beginTest ("Mono works");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480, 1);
            setParam (p, Peq::lowBoostId, 10.0f);
            setParam (p, Peq::highBoostId, 10.0f);
            const auto out = run (p, { noise (48000, 0.5f, 9) }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s); }));
        }

        beginTest ("Bypass: the dry sound passes bit-exact, fades in and out without a jump, and never glitches on return");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Peq::lowBoostId, 13.5f);
            setParam (p, Peq::highBoostId, 18.0f);
            const auto signal = noise (96000, 0.5f, 13);
            setParam (p, Peq::bypassId, 1.0f);
            auto out = run (p, { signal, signal }, 480);
            expect (std::equal (signal.begin() + 4800, signal.end(), out[0].begin() + 4800), "bit-exact once the short fade is over");

            float biggestStep = 0.0f;
            for (size_t i = 1; i < 4800; ++i)
                biggestStep = std::max (biggestStep, std::abs (out[0][i] - out[0][i - 1] - (signal[i] - signal[i - 1])));
            expect (biggestStep < 2.0f, "no click at the start of the fade");

            setParam (p, Peq::bypassId, 0.0f);
            out = run (p, { signal, signal }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s) && std::abs (s) < 20.0f; }), "returning is clean");
            expect (out[0] != signal, "and the processing is back");
        }

        beginTest ("Saved state round-trips every parameter and the editor's own settings");
        {
            SongsmithPassiveEQProcessor a;
            setParam (a, Peq::lowFrequencyId, 3.0f);
            setParam (a, Peq::lowBoostId, 7.3f);
            setParam (a, Peq::lowCutId, 12.5f);
            setParam (a, Peq::highBoostFrequencyId, 6.0f);
            setParam (a, Peq::highBoostId, 9.9f);
            setParam (a, Peq::highBandwidthId, 8.2f);
            setParam (a, Peq::highCutFrequencyId, 0.0f);
            setParam (a, Peq::highCutId, 4.4f);
            setParam (a, Peq::bypassId, 1.0f);
            a.editorWidth = 1111;
            a.presets.currentName = "My Voice";
            const auto block = stateOf (a);

            SongsmithPassiveEQProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            for (auto* id : soundIds)
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 1.0e-4f, juce::String (id));
            expectEquals (getParam (b, Peq::bypassId), 1.0f);
            expectEquals (b.editorWidth.load(), 1111);
            expectEquals (b.presets.currentName, juce::String ("My Voice"));

            auto xml = a.createState().createXml();
            expect (xml->getChildByName ("UI") != nullptr, "the editor settings are saved");
            expectEquals (a.createState().getProperty ("stateVersion", 0).operator int(), SongsmithPassiveEQProcessor::stateVersion);
        }

        beginTest ("Older, partial, damaged and out-of-range saved state loads safely");
        {
            SongsmithPassiveEQProcessor p;
            setParam (p, Peq::lowBoostId, 9.0f);
            setParam (p, Peq::highCutFrequencyId, 2.0f);

            // Only High Cut saved: everything else returns to its default, as a session from before it existed would.
            juce::ValueTree partial (p.apvts.state.getType());
            partial.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::highCutId }, { "value", 9.0f } }), nullptr);
            p.restoreFromValueTree (partial);
            expectWithinAbsoluteError (getParam (p, Peq::highCutId), 9.0f, 1.0e-4f);
            expectEquals (getParam (p, Peq::lowBoostId), 0.0f, "missing: default");
            expectEquals (getParam (p, Peq::highCutFrequencyId), (float) Peq::defaultHighCutFrequency, "missing: default switch");
            expectEquals (p.editorWidth.load(), Peq::EditorLayout::defaultWidth, "missing: default size");

            // Out-of-range, non-finite and unknown values.
            juce::ValueTree odd (p.apvts.state.getType());
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::highBoostId }, { "value", 999.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::lowCutId }, { "value", std::numeric_limits<float>::quiet_NaN() } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::lowFrequencyId }, { "value", 77.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", "from_the_future" }, { "value", 1.0f } }), nullptr);
            juce::ValueTree ui ("UI");
            ui.setProperty ("editorWidth", 99999, nullptr);
            odd.appendChild (ui, nullptr);
            p.restoreFromValueTree (odd);
            expectEquals (getParam (p, Peq::highBoostId), SongsmithPassive::maxHighBoostDb, "clamped");
            expectEquals (getParam (p, Peq::lowCutId), 0.0f, "not a number: default");
            expectEquals (getParam (p, Peq::lowFrequencyId), 3.0f, "an unknown switch position is the nearest real one");
            expectEquals (p.editorWidth.load(), Peq::EditorLayout::maxWidth);

            const char junk[] = "this is not a saved session";
            p.setStateInformation (junk, (int) sizeof (junk));
            p.setStateInformation (nullptr, 0);
            expectEquals (p.editorWidth.load(), Peq::EditorLayout::maxWidth, "junk is ignored");
        }

        beginTest ("Undo and Redo work for the musician's changes, and a loaded session starts with a clean history");
        {
            SongsmithPassiveEQProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, Peq::lowBoostId, Peq::highBoostId, "Passive EQ");
        }

        beginTest ("The starting points set every sound parameter, leave Bypass alone, and are the host's programs");
        {
            const auto& list = Peq::Presets::factory();
            expectEquals ((int) list.size(), 19, "ten from earlier, and nine more use-case starting points (PEQ-05)");
            SongsmithPassiveEQProcessor p;
            expectEquals (p.getNumPrograms(), 19);
            setParam (p, Peq::bypassId, 1.0f);
            for (int i = 0; i < p.getNumPrograms(); ++i)
            {
                p.setCurrentProgram (i);
                const auto& f = list[(size_t) i];
                expectEquals (p.getProgramName (i), juce::String (f.name));
                expectEquals (getParam (p, Peq::lowFrequencyId), (float) f.lowFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), f.lowBoost, 1.0e-3f, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::lowCutId), f.lowCut, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::highBoostFrequencyId), (float) f.highBoostFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::highBoostId), f.highBoost, 1.0e-3f, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::highBandwidthId), f.bandwidth, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::highCutFrequencyId), (float) f.highCutFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::highCutId), f.highCut, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::modeId), (float) f.mode, f.name);
                expectEquals (getParam (p, Peq::midLowFrequencyId), (float) f.midLowFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::midLowBoostId), f.midLowBoost, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::midDipFrequencyId), (float) f.midDipFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::midDipId), f.midDip, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::midHighFrequencyId), (float) f.midHighFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::midHighBoostId), f.midHighBoost, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::highPassFrequencyId), (float) f.highPass, f.name);
                expectEquals (getParam (p, Peq::lowPassFrequencyId), (float) f.lowPass, f.name);
                expectEquals (getParam (p, Peq::bypassId), 1.0f, "Bypass is not a preset's business");
                expectEquals (p.getCurrentProgram(), i);
                expectEquals (p.presets.currentName, juce::String (f.name));

                // Gentle: no starting point lifts or cuts more than 6 dB anywhere in the audible range.
                SongsmithPassive::Engine e;
                e.prepare (48000.0);
                e.setParams (p.getEngineParams());
                // (A Frequency Filter starting point is a filter: it takes the sound away beyond its frequency by design, so it is
                // checked to leave the middle alone instead.)
                if (f.mode == 2 && juce::String (f.name).startsWith ("Telephone"))
                {
                    // A deliberate band-limited effect: it removes the low end and the top, and keeps the voice range.
                    expect (e.responseDb (100.0) < -12.0f && e.responseDb (10000.0) < -12.0f, "the telephone sound takes away the low and high ends");
                    expect (std::abs (e.responseDb (1000.0)) <= 3.0f, "and keeps the voice range");
                }
                else if (f.mode == 2)
                {
                    // (The middle is 800 Hz to 2 kHz: the rhythm guitar filters, at 150 Hz and 5 kHz, are within half a decibel of flat there.)
                    for (double hz = 800.0; hz < 2000.0; hz *= 1.1)
                        expect (std::abs (e.responseDb (hz)) <= 0.5f, juce::String (f.name) + " leaves the middle alone at " + juce::String (hz) + " Hz");
                }
                else
                {
                    for (double hz = 20.0; hz < 20000.0; hz *= 1.1)
                        expect (std::abs (e.responseDb (hz)) <= 6.0f, juce::String (f.name) + " stays gentle at " + juce::String (hz) + " Hz");
                }
            }
            expectEquals (p.getProgramName (99), juce::String());

            // Names say what they are for: Title Case, no model names, no tutorial titles, no playful names.
            for (const auto& f : list)
            {
                const juce::String presetName (f.name);
                expect (presetName.length() >= 8 && presetName.length() <= 40, presetName + " is short and clear");
                for (const auto& word : juce::StringArray::fromTokens (presetName, " ", ""))
                    expect (juce::CharacterFunctions::isUpperCase (word[0]) || juce::StringArray { "a", "an", "and", "on", "or", "of" }.contains (word),
                            presetName + " is Title Case (small joining words stay lower case)");
                for (const char* banned : { "Pultec", "EQP", "Gilder", "Magic", "Vintage" })
                    expect (! presetName.containsIgnoreCase (banned), presetName + " keeps model and educator names out of the title");
                expect (juce::String (f.hint).isNotEmpty() && ! juce::String (f.hint).containsIgnoreCase ("Gilder")
                            && ! juce::String (f.hint).containsIgnoreCase ("Pultec"),
                        presetName + " has a listening cue that stands on its own");
            }
        }

        beginTest ("Your own presets: save, list, reload, replace, and refuse the starting points' names and damaged files");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("peq-preset-test-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            Peq::Presets::setDefaultFolderRootForTesting (root);
            {
                SongsmithPassiveEQProcessor p;
                const auto folder = p.presets.getUserFolder();
                expect (folder.getFullPathName().contains ("Songsmith Passive EQ") && folder.getFullPathName().endsWith ("Presets"),
                        "a folder of their own: " + folder.getFullPathName());
                expect (p.presets.getUserPresetFiles().isEmpty());

                setParam (p, Peq::lowFrequencyId, 1.0f);
                setParam (p, Peq::lowBoostId, 6.0f);
                setParam (p, Peq::highBoostFrequencyId, 5.0f);
                setParam (p, Peq::highBoostId, 7.0f);
                setParam (p, Peq::highBandwidthId, 2.5f);
                setParam (p, Peq::highCutFrequencyId, 2.0f);
                setParam (p, Peq::highCutId, 3.0f);
                juce::String error;
                expect (p.presets.saveAs ("  My: Vocal?  ", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1);
                expect (p.presets.userPresetExists ("My Vocal"), "the name is cleaned the same way when checking");
                expectEquals (p.presets.currentName, juce::String ("My Vocal"));

                float saved[8];
                for (int i = 0; i < 8; ++i)
                    saved[i] = getParam (p, soundIds[i]);
                for (auto* id : soundIds)
                    setParam (p, id, 0.0f);
                setParam (p, Peq::bypassId, 1.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error), error);
                for (int i = 0; i < 8; ++i)
                    expectWithinAbsoluteError (getParam (p, soundIds[i]), saved[i], 1.0e-3f, soundIds[i]);
                expectEquals (getParam (p, Peq::bypassId), 1.0f, "a preset never touches Bypass");

                expect (! p.presets.saveAs ("", error) && error.isNotEmpty(), "an empty name is refused");
                for (const auto& f : Peq::Presets::factory())
                    expect (! p.presets.saveAs (f.name, error) && error.contains ("starting point"), juce::String (f.name) + " cannot be replaced");
                expect (! p.presets.saveAs (juce::String (Peq::Presets::factory()[0].name).toUpperCase(), error), "in any capitals");

                // A replaced preset is the new settings.
                setParam (p, Peq::lowBoostId, 2.0f);
                expect (p.presets.saveAs ("My Vocal", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1, "replaced, not duplicated");
                setParam (p, Peq::lowBoostId, 11.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error));
                expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), 2.0f, 1.0e-3f);

                // Damaged files.
                const auto bad = folder.getChildFile ("bad.sspreset");
                bad.replaceWithText ("this is not xml");
                expect (! p.presets.loadFile (bad, error) && error.contains ("isn't a"), "not xml");
                bad.replaceWithText ("<SomethingElse low_boost=\"5\"/>");
                expect (! p.presets.loadFile (bad, error), "the wrong kind of file");
                expect (! p.presets.loadFile (folder.getChildFile ("missing.sspreset"), error), "a missing file");
                bad.replaceWithText ("<SongsmithPassiveEQPreset low_boost=\"9999\" high_cut=\"nan\"/>");
                expect (p.presets.loadFile (bad, error), error);
                expectEquals (getParam (p, Peq::lowBoostId), SongsmithPassive::maxLowBoostDb, "out of range is clamped");
                expectEquals (getParam (p, Peq::highCutId), 0.0f, "not a number is the default");
            }
            Peq::Presets::setDefaultFolderRootForTesting (juce::File());
            root.deleteRecursively();
        }
    }
};

//==============================================================================
class PassiveEQEditorTests final : public juce::UnitTest
{
public:
    PassiveEQEditorTests() : juce::UnitTest ("Songsmith Passive EQ editor", "PassiveEQ") {}

    static std::unique_ptr<SongsmithPassiveEQEditor> make (SongsmithPassiveEQProcessor& p, int width)
    {
        p.editorWidth = width;
        return std::unique_ptr<SongsmithPassiveEQEditor> (dynamic_cast<SongsmithPassiveEQEditor*> (p.createEditor()));
    }

    void runTest() override
    {
        beginTest ("The editor opens at the default, minimum and maximum sizes, keeps its shape and remembers its size");
        for (int width : { Peq::EditorLayout::defaultWidth, Peq::EditorLayout::minWidth, Peq::EditorLayout::maxWidth })
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, width);
            expect (ed != nullptr);
            expectEquals (ed->getWidth(), width);
            expectEquals (ed->getHeight(), Peq::EditorLayout::heightFor (width));
            expectWithinAbsoluteError (ed->getContentScale(), (float) width / (float) Peq::EditorLayout::designWidth, 1.0e-5f);
        }
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, 1000);
            ed->setSize (1200, Peq::EditorLayout::heightFor (1200));
            expectEquals (p.editorWidth.load(), 1200, "the size is saved with the session");
            expectEquals (Peq::EditorLayout::constrainWidth (1), Peq::EditorLayout::minWidth);
            expectEquals (Peq::EditorLayout::constrainWidth (99999), Peq::EditorLayout::maxWidth);
            expectEquals (Peq::EditorLayout::minWidth * 4, Peq::EditorLayout::designWidth * 3, "75 % at the smallest");
            expectEquals (Peq::EditorLayout::maxWidth, Peq::EditorLayout::designWidth * 3, "300 % at the largest");
        }

        beginTest ("Title bar reads name, preset, Undo, Redo, Bypass, and Bypass is top-right in the suite red");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Passive EQ");
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (Peq::bypassId), "Passive EQ");
            expect (ed->getTitleBounds().getRight() < ed->getPresetBox().getX(), "the title does not overlap the preset selector");
            expect (ed->getTitleHint().getBounds() == ed->getTitleBounds(), "the hint sits exactly over the title");
        }

        beginTest ("Layout: the curve on top, Low, High Boost and High Cut groups left to right, each frequency switch above the knobs it belongs to");
        for (int width : { Peq::EditorLayout::minWidth, Peq::EditorLayout::defaultWidth, Peq::EditorLayout::maxWidth })
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, width);
            const auto context = "at " + juce::String (width) + " px";
            struct Part { juce::Component* c; juce::Rectangle<int> group; };
            const auto low = ed->getLowGroup(), hb = ed->getHighBoostGroup(), hc = ed->getHighCutGroup();
            const Part parts[] = { { &ed->getLowSwitch(), low }, { &ed->getLowBoostKnob(), low }, { &ed->getLowCutKnob(), low },
                                   { &ed->getHighBoostSwitch(), hb }, { &ed->getHighBoostKnob(), hb }, { &ed->getBandwidthKnob(), hb },
                                   { &ed->getHighCutSwitch(), hc }, { &ed->getHighCutKnob(), hc } };
            auto* content = ed->getCurve().getParentComponent();
            expect (content->getLocalBounds().contains (ed->getCurve().getBounds()), context + ": the curve is inside");
            expect (ed->getCurve().getBottom() <= low.getY(), context + ": the curve is above the groups");
            expect (low.getRight() <= hb.getX() && hb.getRight() <= hc.getX(), context + ": Low, High Boost, High Cut from left to right");
            expect (hc.getBottom() <= Peq::EditorLayout::contentHeight, context + ": the groups are above the footer strip");

            for (size_t i = 0; i < std::size (parts); ++i)
            {
                expect (parts[i].group.contains (parts[i].c->getBounds()), context + ": control " + juce::String ((int) i) + " sits inside its own group");
                for (size_t j = i + 1; j < std::size (parts); ++j)
                    expect (! parts[i].c->getBounds().intersects (parts[j].c->getBounds()), context + ": no overlap");
            }
            for (auto* knob : { &ed->getLowBoostKnob(), &ed->getLowCutKnob() })
                expect (ed->getLowSwitch().getBottom() <= knob->getY(), context + ": the low frequency switch is above its knobs");
            for (auto* knob : { &ed->getHighBoostKnob(), &ed->getBandwidthKnob() })
                expect (ed->getHighBoostSwitch().getBottom() <= knob->getY(), context + ": the high boost switch is above its knobs");
            expect (ed->getHighCutSwitch().getBottom() <= ed->getHighCutKnob().getY(), context + ": the high cut switch is above its knob");
            expect (ed->getLowBoostKnob().getX() < ed->getLowCutKnob().getX(), context + ": Boost then Cut");
            expect (ed->getHighBoostKnob().getX() < ed->getBandwidthKnob().getX(), context + ": High Boost then Bandwidth");
            expect (ed->getBypassButton().getRight() <= content->getWidth(), context + ": Bypass is inside");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("The frequency switches light up for the choice, from a click, automation and a restored session");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            expectEquals (ed->getLowSwitch().getNumButtons(), 4);
            expectEquals (ed->getHighBoostSwitch().getNumButtons(), 7);
            expectEquals (ed->getHighCutSwitch().getNumButtons(), 3);
            expect (ed->getLowSwitch().getButton (2).getToggleState() && ! ed->getLowSwitch().getButton (0).getToggleState(), "60 Hz at first");
            expect (ed->getHighBoostSwitch().getButton (3).getToggleState(), "8 kHz at first");
            expect (ed->getHighCutSwitch().getButton (1).getToggleState(), "10 kHz at first");

            ed->getLowSwitch().getButton (0).onClick();
            expectEquals (getParam (p, Peq::lowFrequencyId), 0.0f, "a click sets the parameter");
            expect (ed->getLowSwitch().getButton (0).getToggleState() && ! ed->getLowSwitch().getButton (2).getToggleState());
            setParam (p, Peq::lowFrequencyId, 3.0f);
            expect (ed->getLowSwitch().getButton (3).getToggleState(), "automation moves the lit button");
            ed->getHighBoostSwitch().getButton (6).onClick();
            expectEquals (getParam (p, Peq::highBoostFrequencyId), 6.0f);
            ed->getHighCutSwitch().getButton (2).onClick();
            expectEquals (getParam (p, Peq::highCutFrequencyId), 2.0f);

            SongsmithPassiveEQProcessor q;
            setParam (q, Peq::highBoostFrequencyId, 0.0f);
            auto e2 = make (q, Peq::EditorLayout::defaultWidth);
            expect (e2->getHighBoostSwitch().getButton (0).getToggleState(), "a restored session opens lit");
        }

        beginTest ("The knobs show and take values, and double-click returns them to the default");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            expectEquals (ed->getLowBoostKnob().getValueText(), juce::String ("0.0 dB"));
            expectEquals (ed->getBandwidthKnob().getValueText(), juce::String ("5.0"));
            ed->getLowBoostKnob().slider.setValue (6.5, juce::sendNotificationSync);
            ed->getLowCutKnob().slider.setValue (3.0, juce::sendNotificationSync);
            ed->getHighBoostKnob().slider.setValue (12.0, juce::sendNotificationSync);
            ed->getHighCutKnob().slider.setValue (4.0, juce::sendNotificationSync);
            ed->getBandwidthKnob().slider.setValue (10.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), 6.5f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, Peq::lowCutId), 3.0f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, Peq::highBoostId), 12.0f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, Peq::highCutId), 4.0f, 1.0e-3f);
            expectEquals (ed->getLowBoostKnob().getValueText(), juce::String ("+6.5 dB"));
            expectEquals (ed->getLowCutKnob().getValueText(), juce::String ("-3.0 dB"), "a cut reads as minus");
            expectEquals (ed->getHighBoostKnob().getValueText(), juce::String ("+12.0 dB"));
            expectEquals (ed->getHighCutKnob().getValueText(), juce::String ("-4.0 dB"));
            expectEquals (ed->getBandwidthKnob().getValueText(), juce::String ("Narrow"));
            setParam (p, Peq::highBandwidthId, 0.0f);
            expectEquals (ed->getBandwidthKnob().getValueText(), juce::String ("Broad"));
            setParam (p, Peq::lowBoostId, 0.0f);
            expectEquals (ed->getLowBoostKnob().getValueText(), juce::String ("0.0 dB"));
        }

        beginTest ("The response curve is the engine's own response for the settings, and follows them");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            ed->refreshCurve();
            for (double hz : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
                expectWithinAbsoluteError (ed->getCurve().responseDb ((float) hz), 0.0f, 1.0e-3f, "flat when every amount is zero");

            setParam (p, Peq::lowBoostId, 10.0f);
            setParam (p, Peq::highBoostId, 10.0f);
            ed->refreshCurve();
            SongsmithPassive::Engine e;
            e.prepare (48000.0);
            e.setParams (p.getEngineParams());
            for (double hz : { 40.0, 60.0, 500.0, 8000.0 })
                expectWithinAbsoluteError (ed->getCurve().responseDb ((float) hz), e.responseDb (hz), 1.0e-3f, juce::String (hz) + " Hz");
            expect (ed->getCurve().responseDb (60.0f) > 5.0f && ed->getCurve().responseDb (8000.0f) > 5.0f, "the boosts show");

            using Curve = Peq::UI::ResponseCurve;
            const auto plot = ed->getCurve().plotArea();
            expect (plot.getWidth() > 400.0f && plot.getHeight() > 60.0f, "there is room to draw");
            expect (Curve::xFor (20.0f, plot) < Curve::xFor (1000.0f, plot) && Curve::xFor (1000.0f, plot) < Curve::xFor (20000.0f, plot), "frequency runs left to right");
            expect (Curve::yFor (10.0f, plot) < Curve::yFor (0.0f, plot) && Curve::yFor (-10.0f, plot) > Curve::yFor (0.0f, plot), "boosts are drawn above the 0 dB line");
        }

        beginTest ("Plain words: every text, parameter name and preset, and every label fits at two sizes");
        for (int width : { Peq::EditorLayout::defaultWidth, Peq::EditorLayout::minWidth })
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, width);
            const auto context = "Passive EQ at " + juce::String (width) + " px";
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& pr : Peq::Presets::factory())
            {
                texts.add (pr.name);
                texts.add (pr.hint);
            }
            expectGreaterThan (texts.size(), 30, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
            for (auto* t : { "Low Boost", "Low Cut", "High Boost", "High Cut", "High Boost Bandwidth", "Low Frequency 60 Hz", "High Boost Frequency 16 kHz",
                             "High Cut Frequency 20 kHz", "20 Hz", "100 Hz", "3 kHz", "16 kHz", "5 kHz", "Bypass", "Undo", "Redo" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Every control uses the shared bevelled look, the embedded typeface and the type scale");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *ed, "Passive EQ");
            Songsmith::UI::Testing::checkEditorFonts (*this, *ed, "Passive EQ editor");
        }

        beginTest ("The footer shows the version and latency and the Processing Load switch, in the shared strip");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            expectEquals (ed->getDiagnosticsBounds().getY(), Peq::EditorLayout::contentHeight);
            expectEquals (ed->getDiagnosticsBounds().getHeight(), Peq::EditorLayout::footerHeight);
            Songsmith::Diagnostics::Testing::checkVersionBinding (*this, p, "Passive EQ");
            Songsmith::Diagnostics::Testing::checkLatencyDisplay (*this, p, "Passive EQ");
            Songsmith::Diagnostics::Testing::checkFooterLayout (*this, p, "Passive EQ");
        }

        beginTest ("The preset menu lists the starting points, your presets, Save and the folder, and loading a starting point is one Undo step");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            auto& box = ed->getPresetBox();
            int starting = 0;
            for (int i = 0; i < box.getNumItems(); ++i)
                if (box.getItemId (i) >= 1 && box.getItemId (i) <= 10)
                    ++starting;
            expectEquals (starting, 10, "ten starting points");
            box.setSelectedId (1, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), Peq::Presets::factory()[0].lowBoost, 1.0e-3f);
            expect (ed->getUndoButton().isEnabled(), "Undo is available after loading");
            expectEquals (box.getText(), juce::String (Peq::Presets::factory()[0].name));
            expect (box.getTooltip().contains (Peq::Presets::factory()[0].hint), "the listening cue is on hover");
            expect (ed->getLowSwitch().getButton (Peq::Presets::factory()[0].lowFrequency).getToggleState(), "the switch follows the preset");
            p.history.undo();
            expectEquals (getParam (p, Peq::lowBoostId), 0.0f, "one Undo restores everything the preset changed");
            expectEquals (getParam (p, Peq::lowCutId), 0.0f);
            expectEquals (getParam (p, Peq::lowFrequencyId), (float) Peq::defaultLowFrequency);
        }

        beginTest ("PEQ-06: every starting point is in exactly one group, in a sensible group, and no group is empty");
        {
            const auto& factory = Peq::Presets::factory();
            const auto& groups = Peq::Presets::categories();

            const juce::StringArray expectedGroups { "Drums", "Bass", "Guitar", "Vocals", "Mix Bus and Mastering", "Utility" };
            expectEquals ((int) groups.size(), expectedGroups.size());
            for (int g = 0; g < (int) groups.size(); ++g)
            {
                expectEquals (juce::String (groups[(size_t) g].name), expectedGroups[g], "the groups are in this order");
                expect (! groups[(size_t) g].presets.empty(), juce::String (groups[(size_t) g].name) + " is not empty");
            }

            // Each of the 19 starting points appears once, so none is lost and none is listed twice under two names.
            std::vector<int> seen (factory.size(), 0);
            for (auto& group : groups)
                for (int index : group.presets)
                {
                    expect (index >= 0 && index < (int) factory.size(), "a real starting point");
                    if (index >= 0 && index < (int) factory.size())
                        ++seen[(size_t) index];
                }
            for (size_t i = 0; i < factory.size(); ++i)
                expectEquals (seen[i], 1, juce::String (factory[i].name) + " is in exactly one group");

            // The groups the task named. The telephone sound is under Vocals only, and the soft-sounding part under Utility.
            const std::pair<const char*, const char*> expectedPlacement[] = {
                { "Punchy Kick", "Drums" }, { "Kick Drum Weight", "Drums" },
                { "Deep, Tight Bass", "Bass" }, { "Bass Definition", "Bass" }, { "Bass Weight", "Bass" },
                { "Tighter Rhythm Guitar", "Guitar" }, { "Acoustic Guitar Brightness", "Guitar" },
                { "Clear Lead Vocal", "Vocals" }, { "Telephone Voice or Guitar", "Vocals" }, { "Lead Vocal Air", "Vocals" }, { "Lead Vocal Presence", "Vocals" },
                { "Mix Bus Air", "Mix Bus and Mastering" }, { "Mix Bus Polish", "Mix Bus and Mastering" },
                { "Rumble Cleanup", "Utility" }, { "More Attack on a Soft-Sounding Part", "Utility" } };
            for (auto& [name, group] : expectedPlacement)
            {
                int found = -1;
                for (int i = 0; i < (int) factory.size(); ++i)
                    if (juce::String (factory[(size_t) i].name) == name)
                        found = i;
                expect (found >= 0, juce::String (name) + " exists");
                expectEquals (Peq::Presets::categoryOf (found), juce::String (group), name);
            }
            expectEquals (Peq::Presets::categoryOf (-1), juce::String());
            expectEquals (Peq::Presets::categoryOf (1000), juce::String());

            // The classic low-end trick (a boost and a cut together at 60 Hz) is already Punchy Kick; there is no second copy under another name.
            const auto& punchy = factory[(size_t) groups[0].presets[0]];
            expectEquals (juce::String (punchy.name), juce::String ("Punchy Kick"));
            expectEquals (punchy.lowFrequency, 2, "60 Hz");
            expect (punchy.lowBoost > 0.0f && punchy.lowCut > 0.0f, "a boost and a cut together");
            int copies = 0;
            for (auto& p : factory)
                if (p.lowFrequency == 2 && p.lowBoost > 0.0f && p.lowCut > 0.0f && p.mode == 0 && p.lowBoost >= 9.0f)
                    ++copies;
            expectEquals (copies, 1, "only one starting point is the 60 Hz punch");

            // A group with nothing in it is not shown.
            const std::vector<Peq::FactoryCategory> withEmpty { { "First", { 1, 2 } }, { "Nothing here", {} }, { "Last", { 3 } } };
            const auto kept = Peq::Presets::withoutEmptyCategories (withEmpty);
            expectEquals ((int) kept.size(), 2);
            expectEquals (juce::String (kept[0].name), juce::String ("First"));
            expectEquals (juce::String (kept[1].name), juce::String ("Last"));
            expectEquals ((int) Peq::Presets::withoutEmptyCategories ({}).size(), 0);
        }

        beginTest ("PEQ-06: the preset menu shows the groups as headings you cannot choose, then your presets, Save and the folder");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("songsmith-peq06", "", false);
            root.createDirectory();
            Peq::Presets::setDefaultFolderRootForTesting (root);
            {
                SongsmithPassiveEQProcessor p;
                auto ed = make (p, Peq::EditorLayout::defaultWidth);
                auto& box = ed->getPresetBox();
                const auto& factory = Peq::Presets::factory();
                const auto& groups = Peq::Presets::categories();

                struct Entry { bool heading, separator; juce::String text; int id; bool enabled; };
                auto readMenu = [&] (juce::ComboBox& source)
                {
                    std::vector<Entry> entries;
                    juce::PopupMenu::MenuItemIterator it (*source.getRootMenu());
                    while (it.next())
                        entries.push_back ({ it.getItem().isSectionHeader, it.getItem().isSeparator, it.getItem().text, it.getItem().itemID, it.getItem().isEnabled });
                    return entries;
                };

                // The menu, in order: each group's heading, then its starting points; with no presets of your own, nothing follows but Save and the folder.
                auto menu = readMenu (box);
                size_t at = 0;
                for (auto& group : groups)
                {
                    expect (at < menu.size() && menu[at].heading, juce::String (group.name) + " starts with a heading");
                    if (at >= menu.size()) break;
                    expectEquals (menu[at].text, juce::String (group.name));
                    expectEquals (menu[at].id, 0, "a heading has no item number, so it cannot be chosen");
                    ++at;
                    for (int index : group.presets)
                    {
                        expect (at < menu.size() && ! menu[at].heading, juce::String (factory[(size_t) index].name) + " is a choice");
                        if (at >= menu.size()) break;
                        expectEquals (menu[at].text, juce::String (factory[(size_t) index].name));
                        expectEquals (menu[at].id, index + 1, "its item number is its index plus one, as before the groups");
                        expect (menu[at].enabled, "and it can be chosen");
                        ++at;
                    }
                }
                expect (at < menu.size() && menu[at].separator, "a line, then Save and the folder");
                expectEquals (menu[at + 1].text, juce::String ("Save preset..."));
                expectEquals (menu.back().text, juce::String ("Open presets folder"));
                int headings = 0;
                for (auto& e : menu) headings += e.heading ? 1 : 0;
                expectEquals (headings, (int) groups.size(), "only the groups are headings when you have no presets of your own");

                // Your own presets: a heading of their own after the starting points, never mixed into a group.
                juce::String error;
                setParam (p, Peq::lowBoostId, 3.0f);
                expect (p.presets.saveAs ("My Own Sound", error), error);
                auto reopened = make (p, Peq::EditorLayout::defaultWidth);   // a new editor builds the menu again
                menu = readMenu (reopened->getPresetBox());
                int yourHeading = -1, myItem = -1, lastGroupItem = -1;
                for (int i = 0; i < (int) menu.size(); ++i)
                {
                    if (menu[(size_t) i].heading && menu[(size_t) i].text == "Your presets") yourHeading = i;
                    if (menu[(size_t) i].text == "My Own Sound") myItem = i;
                    if (menu[(size_t) i].id >= 1 && menu[(size_t) i].id <= (int) factory.size()) lastGroupItem = i;
                }
                expect (yourHeading > lastGroupItem && myItem == yourHeading + 1, "your presets come after every group, under their own heading");
                expect (menu[(size_t) myItem].id >= 100, "and keep their own item numbers");
            }
            Peq::Presets::setDefaultFolderRootForTesting ({});
            root.deleteRecursively();
        }

        beginTest ("PEQ-06: choosing from the grouped menu loads the starting point, switches the mode, and is one Undo step");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            auto& box = ed->getPresetBox();
            const auto& factory = Peq::Presets::factory();

            // One from each mode: a Tone Shaper (Punchy Kick), a Mid Shaper (Clear Lead Vocal), a Frequency Filter (Tighter Rhythm Guitar).
            const char* names[] = { "Punchy Kick", "Clear Lead Vocal", "Tighter Rhythm Guitar" };
            for (int expectedMode = 0; expectedMode < 3; ++expectedMode)
            {
                int index = -1;
                for (int i = 0; i < (int) factory.size(); ++i)
                    if (juce::String (factory[(size_t) i].name) == names[expectedMode])
                        index = i;
                expect (index >= 0, names[expectedMode]);
                box.setSelectedId (index + 1, juce::sendNotificationSync);
                const auto& f = factory[(size_t) index];
                expectEquals (f.mode, expectedMode, juce::String (names[expectedMode]) + " is in the mode the test expects");
                expectEquals (getParam (p, Peq::modeId), (float) expectedMode, "choosing it switches the mode");
                expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), f.lowBoost, 1.0e-3f, names[expectedMode]);
                expectWithinAbsoluteError (getParam (p, Peq::midHighBoostId), f.midHighBoost, 1.0e-3f, names[expectedMode]);
                expectEquals (getParam (p, Peq::highPassFrequencyId), (float) f.highPass, names[expectedMode]);
                expectEquals (getParam (p, Peq::lowPassFrequencyId), (float) f.lowPass, names[expectedMode]);
                expectEquals (box.getText(), juce::String (f.name), "the box shows the choice");
                expectEquals (box.getSelectedId(), index + 1, "and the same item stays chosen");
            }
            p.history.undo();   // back from the Frequency Filter
            expectEquals (getParam (p, Peq::modeId), 1.0f, "one Undo undoes only the last choice, including its mode");
            p.history.undo();
            expectEquals (getParam (p, Peq::modeId), 0.0f);

            // Every starting point, chosen from the menu by its item number, loads its own settings.
            for (int i = 0; i < (int) factory.size(); ++i)
            {
                box.setSelectedId (i + 1, juce::sendNotificationSync);
                expectEquals (p.presets.currentName, juce::String (factory[(size_t) i].name));
                expectEquals (getParam (p, Peq::modeId), (float) factory[(size_t) i].mode, factory[(size_t) i].name);
            }
        }

        beginTest ("PEQ-06: a grouped choice comes back when the session is saved and reopened, and the box shows it");
        {
            SongsmithPassiveEQProcessor p;
            const auto& factory = Peq::Presets::factory();
            int index = -1;
            for (int i = 0; i < (int) factory.size(); ++i)
                if (juce::String (factory[(size_t) i].name) == "Bass Definition")
                    index = i;
            {
                auto ed = make (p, Peq::EditorLayout::defaultWidth);
                ed->getPresetBox().setSelectedId (index + 1, juce::sendNotificationSync);
            }
            juce::MemoryBlock saved;
            p.getStateInformation (saved);

            SongsmithPassiveEQProcessor q;
            q.setStateInformation (saved.getData(), (int) saved.getSize());
            expectEquals (getParam (q, Peq::modeId), 1.0f, "reopened in the Mid Shaper");
            expectWithinAbsoluteError (getParam (q, Peq::midLowBoostId), factory[(size_t) index].midLowBoost, 1.0e-3f);
            expectEquals (q.presets.currentName, juce::String ("Bass Definition"));
            auto ed = make (q, Peq::EditorLayout::defaultWidth);
            expectEquals (ed->getPresetBox().getText(), juce::String ("Bass Definition"), "the box shows it");
            expectEquals (ed->getPresetBox().getSelectedId(), index + 1, "under its own item number");
        }

        beginTest ("PEQ-06: the arrow keys step through the starting points in menu order and skip the headings");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            auto& box = ed->getPresetBox();
            const auto& groups = Peq::Presets::categories();
            std::vector<int> ids;   // the item numbers in the order the menu shows them
            for (auto& group : groups)
                for (int index : group.presets)
                    ids.push_back (index + 1);
            expectEquals ((int) ids.size(), (int) Peq::Presets::factory().size());

            box.setSelectedId (ids.front(), juce::sendNotificationSync);
            for (size_t i = 1; i < ids.size(); ++i)
            {
                expect (box.keyPressed (juce::KeyPress (juce::KeyPress::downKey)), "Down is handled");
                expectEquals (box.getSelectedId(), ids[i], "Down goes to the next starting point, over any heading, not onto it");
            }
            for (size_t i = ids.size() - 1; i-- > 0;)
            {
                expect (box.keyPressed (juce::KeyPress (juce::KeyPress::upKey)), "Up is handled");
                expectEquals (box.getSelectedId(), ids[i], "Up goes back the same way");
            }
        }

        beginTest ("PEQ-06: the preset box keeps its place and words at the smallest, default and largest editor");
        {
            for (int width : { Peq::EditorLayout::minWidth, Peq::EditorLayout::defaultWidth, Peq::EditorLayout::maxWidth })
            {
                SongsmithPassiveEQProcessor p;
                auto ed = make (p, width);
                const auto context = "Passive EQ at " + juce::String (width) + " px";
                expect (ed->getPresetBox().isVisible() && ed->getPresetBox().getWidth() > 100, context + ": the preset box is there");
                expect (ed->getTitleBounds().getRight() < ed->getPresetBox().getX(), context + ": the title does not overlap the preset box");
                juce::StringArray texts;
                for (auto& group : Peq::Presets::categories())
                    texts.add (group.name);
                Songsmith::UI::Wording::expectPlainWords (*this, texts, context + " group names");
                for (auto& group : Peq::Presets::categories())
                    expect (juce::String (group.name).length() >= 4 && juce::String (group.name).length() <= 24, juce::String (group.name) + " is a short, clear heading");
            }
        }

        beginTest ("The output meter reads each channel of the processed sound: silence, a calibrated sine, imbalance, boosts, Bypass, clipping and mono");
        {
            auto sine = [] (int n, float amp, double hz, double fs)
            {
                std::vector<float> v ((size_t) n);
                for (int i = 0; i < n; ++i)
                    v[(size_t) i] = amp * (float) std::sin (2.0 * 3.14159265358979 * hz * (double) i / fs);
                return v;
            };
            const double fs = 48000.0;

            {
                SongsmithPassiveEQProcessor p;
                prepare (p, fs, 480);
                run (p, { std::vector<float> (4800, 0.0f), std::vector<float> (4800, 0.0f) }, 480);
                expectEquals (p.outputMeter.readAndResetChannel (0), 0.0f, "silence reads as silence");

                // Left 0.5 and right 0.1, flat: the meter reads what comes out, separately for each side.
                run (p, { sine (48000, 0.5f, 1000.0, fs), sine (48000, 0.1f, 1000.0, fs) }, 480);
                const float left = p.outputMeter.readAndResetChannel (0), right = p.outputMeter.readAndResetChannel (1);
                expectWithinAbsoluteError (left, 0.5f, 0.01f, "a calibrated sine on the left");
                expectWithinAbsoluteError (right, 0.1f, 0.005f, "and a quieter one on the right");
                expectWithinAbsoluteError (juce::Decibels::gainToDecibels (left) - juce::Decibels::gainToDecibels (right), 13.98f, 0.3f, "the imbalance is 14 dB");
                expectEquals (p.outputMeter.readAndResetChannel (0), 0.0f, "reading clears it");
                expectEquals (p.outputMeter.channels.load(), 2);
            }
            {
                // After the processing: a low boost makes the output louder than the input.
                SongsmithPassiveEQProcessor p;
                prepare (p, fs, 480);
                setParam (p, Peq::lowFrequencyId, 3.0f);   // 100 Hz
                setParam (p, Peq::lowBoostId, 10.0f);
                const auto in = sine (48000, 0.3f, 100.0, fs);
                run (p, { in, in }, 480);
                expect (p.outputMeter.readAndResetChannel (0) > 0.3f * 1.5f, "the meter reads the boosted sound, not the input");
            }
            {
                // Bypass: what leaves is the input.
                SongsmithPassiveEQProcessor p;
                prepare (p, fs, 480);
                setParam (p, Peq::lowFrequencyId, 3.0f);
                setParam (p, Peq::lowBoostId, 10.0f);
                setParam (p, Peq::bypassId, 1.0f);
                const auto in = sine (48000, 0.3f, 100.0, fs);
                run (p, { in, in }, 480);
                p.outputMeter.readAndResetChannel (0);
                run (p, { in, in }, 480);
                expectWithinAbsoluteError (p.outputMeter.readAndResetChannel (0), 0.3f, 0.005f, "bypassed, the meter reads the untouched input");
            }
            {
                // Clipping: the warning lights at 0 dBFS and clears after the sound has stopped.
                SongsmithPassiveEQProcessor p;
                prepare (p, fs, 480);
                run (p, { sine (4800, 1.2f, 1000.0, fs), sine (4800, 0.2f, 1000.0, fs) }, 480);
                expect (p.outputMeter.clipLatched.load(), "the clip warning lights");
                run (p, { std::vector<float> (96000, 0.0f), std::vector<float> (96000, 0.0f) }, 480);
                expect (! p.outputMeter.clipLatched.load(), "and clears in silence");
            }
            {
                SongsmithPassiveEQProcessor p;
                prepare (p, fs, 480, 1);
                run (p, { sine (4800, 0.4f, 1000.0, fs) }, 480);
                expectEquals (p.outputMeter.channels.load(), 1, "a mono host shows one bar");
                expectWithinAbsoluteError (p.outputMeter.readAndResetChannel (0), 0.4f, 0.01f);
            }
        }

        beginTest ("The output meter sits on the right edge, labelled, beside the curve and the groups, and follows the audio");
        for (int width : { Peq::EditorLayout::minWidth, Peq::EditorLayout::defaultWidth, Peq::EditorLayout::maxWidth })
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            auto ed = make (p, width);
            const auto context = "at " + juce::String (width) + " px";
            auto& meter = ed->getOutputMeter();
            auto* content = meter.getParentComponent();
            expect (content->getLocalBounds().contains (meter.getBounds()), context + ": inside the editor");
            expect (meter.getBottom() <= Peq::EditorLayout::contentHeight, context + ": above the footer strip");
            expect (meter.getX() >= ed->getCurve().getRight() && meter.getX() >= ed->getHighCutGroup().getRight(), context + ": right of the curve and every group");
            expectEquals (meter.getTitle(), juce::String ("Output level"));
            expect (meter.getTooltip().contains ("not true peak") && meter.getTooltip().contains ("left and right"), context + ": the tooltip says what it measures");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            std::vector<float> in (48000);
            for (size_t i = 0; i < in.size(); ++i)
                in[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) i / 48000.0);
            run (p, { in, std::vector<float> (in.size(), 0.0f) }, 480);
            ed->advanceMeter (0.033);
            expectEquals (ed->getOutputMeter().getNumChannels(), 2);
            expect (ed->getOutputMeter().getPeakText() != juce::String::charToString (0x2013), "the held peak shows a number");
            expectWithinAbsoluteError (ed->getOutputMeter().getHoldDb(), -6.0f, 0.3f, "the held peak is about -6 dBFS");
        }

        beginTest ("The faceplate is blue and textured, not flat, and the title bar keeps the suite's neutral look");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            const auto image = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            // The strip of faceplate left of the first group, between the screws, over a tall stretch.
            int bluer = 0, samples = 0;
            std::set<juce::uint32> distinct;
            for (int y = 200; y < 380; ++y)
                for (int x = 9; x < 13; ++x)
                {
                    const auto c = image.getPixelAt (x, y);
                    ++samples;
                    bluer += c.getBlue() > c.getRed() + 30 && c.getBlue() > c.getGreen() + 10 ? 1 : 0;
                    distinct.insert (c.getARGB());
                }
            expect (bluer > samples * 9 / 10, "the faceplate is clearly blue: " + juce::String (bluer) + " of " + juce::String (samples));
            expect (distinct.size() > 8, "with a grain and a shaded gradient, not a flat fill: " + juce::String ((int) distinct.size()) + " colours");
            const auto titleBar = image.getPixelAt (700, 10);
            expect (std::abs (titleBar.getBlue() - titleBar.getRed()) < 20, "the suite's title bar stays neutral");
        }
    }
};

static PassiveEQPluginTests passiveEQPluginTests;
static PassiveEQEditorTests passiveEQEditorTests;

#include "ModePluginTests.h"   // PEQ-04: the EQ Mode (parameters, sessions, presets, editor)

//==============================================================================
static SongsmithUITests sharedUITests { "PassiveEQ" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "PassiveEQ" };
static SongsmithGuidanceTests sharedGuidanceTests { "PassiveEQ" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// PassiveEQTests --screenshot <file.png> [--scale 2] [--preset N]: renders the editor with a starting point
// loaded to a PNG, so UI changes can be checked without a host.
static int renderScreenshot (const juce::File& file, float scale, int preset)
{
    SongsmithPassiveEQProcessor p;
    p.setCurrentProgram (preset);
    prepare (p, 48000.0, 512);
    std::unique_ptr<SongsmithPassiveEQEditor> editor (dynamic_cast<SongsmithPassiveEQEditor*> (p.createEditor()));
    editor->refreshCurve();

    // Two seconds of a steady tone, a little louder on the left, so the output meter has something to show.
    std::vector<float> left (96000), right (96000);
    for (size_t i = 0; i < left.size(); ++i)
    {
        const float tone = (float) std::sin (2.0 * 3.14159265358979 * 440.0 * (double) i / 48000.0);
        left[i] = 0.35f * tone;
        right[i] = 0.25f * tone;
    }
    run (p, { left, right }, 512);
    editor->advanceMeter (0.5);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, out))
        return 1;
    std::printf ("Wrote %s\n", file.getFullPathName().toRawUTF8());
    return 0;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--screenshot")
        {
            float scale = 1.0f;
            int preset = 0;
            for (int j = 1; j < argc; ++j)
            {
                if (juce::String (argv[j]) == "--scale" && j + 1 < argc)
                    scale = juce::jlimit (1.0f, 4.0f, juce::String (argv[j + 1]).getFloatValue());
                if (juce::String (argv[j]) == "--preset" && j + 1 < argc)
                    preset = juce::jlimit (0, (int) Peq::Presets::factory().size() - 1, juce::String (argv[j + 1]).getIntValue());
            }
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale, preset);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("PassiveEQ");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
