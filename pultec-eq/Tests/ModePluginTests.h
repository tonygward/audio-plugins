#pragma once

// PEQ-04: the EQ Mode in the plugin (parameters, saved sessions, presets, bypass, mono and stereo, Undo) and in the editor
// (the dropdown, the controls of each mode, the response display, layout and wording). Included by PassiveEQTests.cpp after its
// helpers (setParam, getParam, prepare, run, rms).

namespace
{
    // What a session saved before the modes existed holds: the original nine parameters under stateVersion 1, and nothing else.
    juce::ValueTree makeVersion1Session (float lowFrequency, float lowBoost, float lowCut, float highBoostFrequency,
                                         float highBoost, float bandwidth, float highCutFrequency, float highCut)
    {
        juce::ValueTree v ("PARAMETERS");
        v.setProperty ("stateVersion", 1, nullptr);
        auto add = [&v] (const char* id, float value) { v.appendChild (juce::ValueTree ("PARAM", { { "id", id }, { "value", value } }), nullptr); };
        add (Peq::lowFrequencyId, lowFrequency);
        add (Peq::lowBoostId, lowBoost);
        add (Peq::lowCutId, lowCut);
        add (Peq::highBoostFrequencyId, highBoostFrequency);
        add (Peq::highBoostId, highBoost);
        add (Peq::highBandwidthId, bandwidth);
        add (Peq::highCutFrequencyId, highCutFrequency);
        add (Peq::highCutId, highCut);
        add (Peq::bypassId, 0.0f);
        return v;
    }

    std::vector<std::vector<float>> programme (int n)
    {
        std::vector<float> left (n), right (n);
        for (int i = 0; i < n; ++i)
        {
            left[(size_t) i] = 0.25f * (float) std::sin (0.031 * i) + 0.1f * (float) std::sin (0.77 * i);
            right[(size_t) i] = 0.2f * (float) std::sin (0.043 * i) + 0.1f * (float) std::sin (0.51 * i);
        }
        return { left, right };
    }
}

class PassiveEQModePluginTests final : public juce::UnitTest
{
public:
    PassiveEQModePluginTests() : juce::UnitTest ("Songsmith Passive EQ modes (PEQ-04)", "PassiveEQ") {}

    void runTest() override
    {
        beginTest ("Backward compatibility: a session saved before the modes loads as Tone Shaper and sounds exactly as it did");
        {
            const auto saved = makeVersion1Session (3.0f, 6.0f, 2.5f, 5.0f, 7.0f, 2.5f, 2.0f, 3.0f);

            SongsmithPassiveEQProcessor oldSession;
            oldSession.restoreFromValueTree (saved);
            expectEquals (getParam (oldSession, Peq::modeId), 0.0f, "no mode saved means Tone Shaper");
            for (auto* id : { Peq::midLowBoostId, Peq::midDipId, Peq::midHighBoostId })
                expectEquals (getParam (oldSession, id), 0.0f, id);
            expectEquals (getParam (oldSession, Peq::highPassFrequencyId), 0.0f, "filters Off");
            expectEquals (getParam (oldSession, Peq::lowPassFrequencyId), 0.0f);
            expectWithinAbsoluteError (getParam (oldSession, Peq::lowBoostId), 6.0f, 1.0e-3f, "and the old settings are kept");
            expectWithinAbsoluteError (getParam (oldSession, Peq::highBoostId), 7.0f, 1.0e-3f);
            expectEquals (getParam (oldSession, Peq::highCutFrequencyId), 2.0f);

            // Sound: the same as a processor set by hand to those Tone Shaper values, with the mode left alone.
            SongsmithPassiveEQProcessor reference;
            for (auto [id, value] : { std::pair<const char*, float> { Peq::lowFrequencyId, 3.0f }, { Peq::lowBoostId, 6.0f }, { Peq::lowCutId, 2.5f },
                                      { Peq::highBoostFrequencyId, 5.0f }, { Peq::highBoostId, 7.0f }, { Peq::highBandwidthId, 2.5f },
                                      { Peq::highCutFrequencyId, 2.0f }, { Peq::highCutId, 3.0f } })
                setParam (reference, id, value);
            prepare (oldSession, 48000.0, 256);
            prepare (reference, 48000.0, 256);
            const auto in = programme (24000);
            expect (run (oldSession, in, 256) == run (reference, in, 256), "bit-identical to the Tone Shaper it always was");
            expect (run (oldSession, in, 256)[0] != in[0], "and it is doing something");

            // The same, through the host's saved-state path (XML, as a DAW stores it).
            juce::MemoryBlock block;
            if (auto xml = saved.createXml())
                SongsmithPassiveEQProcessor::copyXmlToBinary (*xml, block);
            SongsmithPassiveEQProcessor viaHost;
            viaHost.setStateInformation (block.getData(), (int) block.getSize());
            expectEquals (getParam (viaHost, Peq::modeId), 0.0f);
            expectWithinAbsoluteError (getParam (viaHost, Peq::lowBoostId), 6.0f, 1.0e-3f);

            // Saving now records the new version, and loading that back keeps everything.
            expectEquals (oldSession.createState().getProperty ("stateVersion", 0).operator int(), SongsmithPassiveEQProcessor::stateVersion);
            expectEquals (SongsmithPassiveEQProcessor::stateVersion, 2);
        }

        beginTest ("Backward compatibility: a preset file saved before the modes loads as Tone Shaper, even from another mode");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("peq-mode-preset-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            Peq::Presets::setDefaultFolderRootForTesting (root);
            {
                SongsmithPassiveEQProcessor p;
                const auto folder = p.presets.getUserFolder();
                folder.createDirectory();
                juce::XmlElement xml (Peq::Presets::rootTag);
                xml.setAttribute ("version", 1);
                xml.setAttribute (Peq::lowFrequencyId, 1.0);
                xml.setAttribute (Peq::lowBoostId, 5.0);
                xml.setAttribute (Peq::lowCutId, 2.0);
                xml.setAttribute (Peq::highBoostFrequencyId, 3.0);
                xml.setAttribute (Peq::highBoostId, 0.0);
                xml.setAttribute (Peq::highBandwidthId, 5.0);
                xml.setAttribute (Peq::highCutFrequencyId, 1.0);
                xml.setAttribute (Peq::highCutId, 0.0);
                const auto file = folder.getChildFile ("Old Bass.sspreset");
                expect (xml.writeTo (file));

                setParam (p, Peq::modeId, 2.0f);   // the plug-in is in Frequency Filter with a high-pass set
                setParam (p, Peq::highPassFrequencyId, 4.0f);
                setParam (p, Peq::midDipId, 5.0f);
                juce::String error;
                expect (p.presets.loadFile (file, error), error);
                expectEquals (getParam (p, Peq::modeId), 0.0f, "an old preset is a Tone Shaper preset");
                expectEquals (getParam (p, Peq::highPassFrequencyId), 0.0f, "and leaves no other mode's settings behind");
                expectEquals (getParam (p, Peq::midDipId), 0.0f);
                expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), 5.0f, 1.0e-3f);
            }
            root.deleteRecursively();
            Peq::Presets::setDefaultFolderRootForTesting ({});
        }

        beginTest ("A new preset saves the mode and every setting, and reloads them");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("peq-mode-preset2-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            Peq::Presets::setDefaultFolderRootForTesting (root);
            {
                SongsmithPassiveEQProcessor p;
                setParam (p, Peq::modeId, 1.0f);
                setParam (p, Peq::midLowFrequencyId, 3.0f);
                setParam (p, Peq::midLowBoostId, 4.5f);
                setParam (p, Peq::midDipFrequencyId, 7.0f);
                setParam (p, Peq::midDipId, 3.0f);
                setParam (p, Peq::midHighFrequencyId, 4.0f);
                setParam (p, Peq::midHighBoostId, 6.0f);
                setParam (p, Peq::highPassFrequencyId, 2.0f);
                setParam (p, Peq::lowPassFrequencyId, 3.0f);
                juce::String error;
                expect (p.presets.saveAs ("Mid Test", error), error);
                for (int idIndex = 0; idIndex < Peq::Presets::numPresetParameters; ++idIndex)
                    setParam (p, Peq::Presets::presetParameterIds[idIndex], 0.0f);
                expectEquals (getParam (p, Peq::modeId), 0.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error), error);
                expectEquals (getParam (p, Peq::modeId), 1.0f, "the mode comes back");
                expectEquals (getParam (p, Peq::midLowFrequencyId), 3.0f);
                expectWithinAbsoluteError (getParam (p, Peq::midLowBoostId), 4.5f, 1.0e-3f);
                expectEquals (getParam (p, Peq::midDipFrequencyId), 7.0f);
                expectWithinAbsoluteError (getParam (p, Peq::midDipId), 3.0f, 1.0e-3f);
                expectEquals (getParam (p, Peq::midHighFrequencyId), 4.0f);
                expectWithinAbsoluteError (getParam (p, Peq::midHighBoostId), 6.0f, 1.0e-3f);
                expectEquals (getParam (p, Peq::highPassFrequencyId), 2.0f);
                expectEquals (getParam (p, Peq::lowPassFrequencyId), 3.0f);
            }
            root.deleteRecursively();
            Peq::Presets::setDefaultFolderRootForTesting ({});
        }

        beginTest ("Saved sessions round-trip the mode and every mode's settings, and each mode keeps its own settings");
        {
            SongsmithPassiveEQProcessor a;
            setParam (a, Peq::lowBoostId, 5.0f);            // Tone Shaper
            setParam (a, Peq::modeId, 1.0f);
            setParam (a, Peq::midDipId, 4.0f);              // Mid Shaper
            setParam (a, Peq::midDipFrequencyId, 6.0f);
            setParam (a, Peq::modeId, 2.0f);
            setParam (a, Peq::highPassFrequencyId, 3.0f);   // Frequency Filter
            expectWithinAbsoluteError (getParam (a, Peq::lowBoostId), 5.0f, 1.0e-3f, "switching mode does not discard another mode's setting");
            expectWithinAbsoluteError (getParam (a, Peq::midDipId), 4.0f, 1.0e-3f);

            const auto block = stateOf (a);
            SongsmithPassiveEQProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            for (auto* id : { Peq::modeId, Peq::lowBoostId, Peq::midDipId, Peq::midDipFrequencyId, Peq::highPassFrequencyId, Peq::lowPassFrequencyId })
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 1.0e-3f, id);
            expectEquals (getParam (b, Peq::modeId), 2.0f, "the mode itself");

            // A state with an out-of-range mode is clamped, never trusted.
            auto odd = a.createState();
            for (auto child : odd)
                if (child.getProperty ("id").toString() == Peq::modeId)
                    child.setProperty ("value", 99.0f, nullptr);
            b.restoreFromValueTree (odd);
            expectLessOrEqual (getParam (b, Peq::modeId), 2.0f);
            expectGreaterOrEqual (getParam (b, Peq::modeId), 0.0f);
        }

        beginTest ("The sound follows the mode: Tone Shaper, Mid Shaper and Frequency Filter each do their own thing");
        {
            auto levelAt = [] (SongsmithPassiveEQProcessor& p, double hz)
            {
                prepare (p, 48000.0, 256);
                std::vector<float> tone ((size_t) 48000);
                for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.1f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) i / 48000.0);
                const auto out = run (p, { tone, tone }, 256)[0];
                return 20.0f * std::log10 (rms (out, 24000) / rms (tone, 24000));
            };
            {
                SongsmithPassiveEQProcessor p;   // Tone Shaper: a low boost at 60 Hz
                setParam (p, Peq::lowFrequencyId, 2.0f); setParam (p, Peq::lowBoostId, 8.0f);
                expectGreaterThan (levelAt (p, 25.0), 6.0f, "Tone Shaper boosts the low end");
            }
            {
                SongsmithPassiveEQProcessor p;   // Mid Shaper: a 6 dB dip at 1 kHz
                setParam (p, Peq::modeId, 1.0f); setParam (p, Peq::midDipFrequencyId, 4.0f); setParam (p, Peq::midDipId, 6.0f);
                expectWithinAbsoluteError (levelAt (p, 1000.0), -6.0f, 0.5f, "Mid Shaper dips the middle by the amount set");
            }
            {
                SongsmithPassiveEQProcessor p;   // Frequency Filter: a high-pass at 120 Hz
                setParam (p, Peq::modeId, 2.0f); setParam (p, Peq::highPassFrequencyId, 4.0f);
                expectLessThan (levelAt (p, 30.0), -10.0f, "Frequency Filter removes the low end");
                SongsmithPassiveEQProcessor q;
                setParam (q, Peq::modeId, 2.0f); setParam (q, Peq::highPassFrequencyId, 4.0f);
                expectLessThan (std::abs (levelAt (q, 2000.0)), 0.2f, "and leaves the middle alone");
            }
        }

        beginTest ("Bypass is bit-exact in every mode, and mono and stereo agree");
        for (int mode = 0; mode < 3; ++mode)
        {
            SongsmithPassiveEQProcessor p;
            setParam (p, Peq::modeId, (float) mode);
            setParam (p, Peq::lowBoostId, 6.0f); setParam (p, Peq::midLowBoostId, 6.0f); setParam (p, Peq::midDipId, 6.0f);
            setParam (p, Peq::highPassFrequencyId, 3.0f); setParam (p, Peq::lowPassFrequencyId, 2.0f);
            setParam (p, Peq::bypassId, 1.0f);
            prepare (p, 48000.0, 256);
            const auto in = programme (24000);
            const auto out = run (p, in, 256);
            bool exact = true;
            for (size_t i = 12000; i < in[0].size(); ++i)
                exact = exact && out[0][i] == in[0][i] && out[1][i] == in[1][i];
            expect (exact, "bypassed output equals the input, mode " + juce::String (mode));

            setParam (p, Peq::bypassId, 0.0f);
            prepare (p, 48000.0, 256);
            const auto stereo = run (p, { in[0], in[0] }, 256);
            SongsmithPassiveEQProcessor m;
            setParam (m, Peq::modeId, (float) mode);
            setParam (m, Peq::lowBoostId, 6.0f); setParam (m, Peq::midLowBoostId, 6.0f); setParam (m, Peq::midDipId, 6.0f);
            setParam (m, Peq::highPassFrequencyId, 3.0f); setParam (m, Peq::lowPassFrequencyId, 2.0f);
            prepare (m, 48000.0, 256, 1);
            const auto mono = run (m, { in[0] }, 256);
            expect (mono[0] == stereo[0] && stereo[0] == stereo[1], "mono equals each stereo channel, mode " + juce::String (mode));
            for (auto s : stereo[0]) if (! std::isfinite (s)) { expect (false, "finite"); break; }
        }

        beginTest ("Output metering reads what leaves the plug-in in every mode");
        for (int mode = 0; mode < 3; ++mode)
        {
            SongsmithPassiveEQProcessor p;
            setParam (p, Peq::modeId, (float) mode);
            setParam (p, Peq::highPassFrequencyId, 5.0f);
            prepare (p, 48000.0, 256);
            std::vector<float> left (24000), right (24000);
            for (size_t i = 0; i < left.size(); ++i) { left[i] = 0.5f * (float) std::sin (0.02 * (double) i); right[i] = 0.25f * (float) std::sin (0.02 * (double) i); }
            p.outputMeter.readAndResetChannel (0);
            const auto out = run (p, { left, right }, 256);
            const float l = p.outputMeter.readAndResetChannel (0), r = p.outputMeter.readAndResetChannel (1);
            float peakL = 0.0f, peakR = 0.0f;
            for (size_t i = 0; i < out[0].size(); ++i) { peakL = std::max (peakL, std::abs (out[0][i])); peakR = std::max (peakR, std::abs (out[1][i])); }
            expectWithinAbsoluteError (l, peakL, 0.02f, "left, mode " + juce::String (mode));
            expectWithinAbsoluteError (r, peakR, 0.02f, "right, mode " + juce::String (mode));
        }

        beginTest ("Undo and Redo reverse a mode change and a mode's controls, and host automation of the mode is not in the history");
        {
            SongsmithPassiveEQProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, Peq::midDipId, Peq::midHighBoostId, "Passive EQ mid controls");

            // The mode is a choice, so its steps are checked directly: one gesture is one Undo step.
            p.history.clear();
            auto* mode = p.apvts.getParameter (Peq::modeId);
            mode->beginChangeGesture();
            mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));
            mode->endChangeGesture();
            expectEquals (getParam (p, Peq::modeId), 1.0f);
            expect (p.history.canUndo(), "choosing a mode can be undone");
            p.history.undo();
            expectEquals (getParam (p, Peq::modeId), 0.0f, "Undo goes back to Tone Shaper");
            expect (p.history.canRedo());
            p.history.redo();
            expectEquals (getParam (p, Peq::modeId), 1.0f, "Redo chooses Mid Shaper again");

            // Host automation of the mode is not something the musician did, so it never enters the history.
            p.history.clear();
            setParam (p, Peq::modeId, 2.0f);
            expect (! p.history.canUndo(), "automation is not in the history");
        }

        beginTest ("Every starting point is in its own mode, and loading one is a single Undo step that returns to the previous mode");
        {
            SongsmithPassiveEQProcessor p;
            expectEquals (getParam (p, Peq::modeId), 0.0f);
            p.history.clear();
            int midCount = 0, filterCount = 0;
            for (int i = 0; i < (int) Peq::Presets::factory().size(); ++i)
            {
                const auto& f = Peq::Presets::factory()[(size_t) i];
                midCount += f.mode == 1;
                filterCount += f.mode == 2;
            }
            expectEquals (midCount, 2, "two Mid Shaper starting points");
            expectEquals (filterCount, 2, "two Frequency Filter starting points");
            p.history.runStep ("preset load", [&] { p.presets.loadFactory (6); });
            expectEquals (getParam (p, Peq::modeId), 1.0f, "loading a Mid Shaper starting point selects Mid Shaper");
            p.history.undo();
            expectEquals (getParam (p, Peq::modeId), 0.0f, "one Undo step puts everything back, mode included");
        }
    }
};

//==============================================================================
class PassiveEQModeEditorTests final : public juce::UnitTest
{
public:
    PassiveEQModeEditorTests() : juce::UnitTest ("Songsmith Passive EQ mode editor (PEQ-04)", "PassiveEQ") {}

    static std::unique_ptr<SongsmithPassiveEQEditor> make (SongsmithPassiveEQProcessor& p, int width)
    {
        p.editorWidth = width;
        return std::unique_ptr<SongsmithPassiveEQEditor> (dynamic_cast<SongsmithPassiveEQEditor*> (p.createEditor()));
    }

    void runTest() override
    {
        namespace L = Peq::EditorLayout;

        beginTest ("One EQ Mode dropdown, in its own row under the title bar and outside the blue panel, lists exactly the three modes");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, L::defaultWidth);
            auto& box = ed->getModeBox();
            expectEquals (box.getNumItems(), 3);
            expectEquals (box.getItemText (0), juce::String ("Tone Shaper"));
            expectEquals (box.getItemText (1), juce::String ("Mid Shaper"));
            expectEquals (box.getItemText (2), juce::String ("Frequency Filter"));
            expectEquals (box.getSelectedItemIndex(), 0, "Tone Shaper is the default and is shown");
            expectEquals (ed->getShownMode(), 0);
            expect (box.isVisible() && box.getHeight() >= 26, "visible and an easy click target");
            expect (box.getTitle() == "EQ Mode", "accessible name");
            expect (ed->getModeLabel().getText() == "EQ Mode" && ed->getModeLabel().getRight() <= box.getX(), "captioned, to its left");
            expect (box.getY() >= (int) Songsmith::UI::titleBarHeight, "directly under the title bar");
            expect (box.getBottom() < L::plateTop, "outside the blue panel: the panel starts below the row");
            expect (box.getBottom() <= ed->getCurveBounds().getY(), "above the response graph");
            for (const auto* word : { "Tone Shaper", "Mid Shaper", "Frequency Filter", "EQP-1A", "MEQ-5", "HLF-3C" })
                expect (box.getTooltip().contains (word), juce::String ("the help names ") + word);
            for (const auto* name : { "EQP-1A", "MEQ-5", "HLF-3C" })
                for (int i = 0; i < box.getNumItems(); ++i)
                    expect (! box.getItemText (i).contains (name), "classic names are help text, never the mode names");
        }

        beginTest ("Choosing a mode writes the parameter; automation, presets and restored sessions update the dropdown and the controls");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, L::defaultWidth);
            ed->getModeBox().setSelectedItemIndex (1, juce::sendNotificationSync);
            expectEquals (getParam (p, Peq::modeId), 1.0f, "chosen by the musician");
            expectEquals (ed->getShownMode(), 1);
            setParam (p, Peq::modeId, 2.0f);
            expectEquals (ed->getModeBox().getSelectedItemIndex(), 2, "automation moves the dropdown");
            expectEquals (ed->getShownMode(), 2);
            p.presets.loadFactory (6);   // a Mid Shaper starting point
            expectEquals (ed->getModeBox().getSelectedItemIndex(), 1, "a starting point selects its mode");

            auto q = std::make_unique<SongsmithPassiveEQProcessor>();
            const auto block = stateOf (p);
            q->setStateInformation (block.getData(), (int) block.getSize());
            auto e2 = make (*q, L::defaultWidth);
            expectEquals (e2->getModeBox().getSelectedItemIndex(), 1, "a restored session opens in its mode");
            expectEquals (e2->getShownMode(), 1);

            // An old session opens as Tone Shaper.
            SongsmithPassiveEQProcessor old;
            old.restoreFromValueTree (makeVersion1Session (2.0f, 4.0f, 3.0f, 3.0f, 0.0f, 5.0f, 1.0f, 0.0f));
            auto e3 = make (old, L::defaultWidth);
            expectEquals (e3->getModeBox().getSelectedItemIndex(), 0);
            expect (e3->getLowBoostKnob().isVisible() && ! e3->getMidDipKnob().isVisible(), "with the Tone Shaper controls");
        }

        beginTest ("Each mode shows only its own controls, inside its groups, clear of each other, the graph and the meter");
        for (int width : { L::minWidth, L::defaultWidth, L::maxWidth })
        for (int mode = 0; mode < 3; ++mode)
        {
            SongsmithPassiveEQProcessor p;
            setParam (p, Peq::modeId, (float) mode);
            auto ed = make (p, width);
            const auto context = "mode " + juce::String (mode) + " at " + juce::String (width) + " px";
            const bool tone = mode == 0, mid = mode == 1, filter = mode == 2;

            expect (ed->getLowBoostKnob().isVisible() == tone && ed->getLowSwitch().isVisible() == tone && ed->getHighCutKnob().isVisible() == tone, context + ": Tone Shaper controls");
            expect (ed->getMidLowKnob().isVisible() == mid && ed->getMidDipKnob().isVisible() == mid && ed->getMidHighKnob().isVisible() == mid
                        && ed->getMidDipSwitch().isVisible() == mid, context + ": Mid Shaper controls");
            expect (ed->getHighPassSwitch().isVisible() == filter && ed->getLowPassSwitch().isVisible() == filter, context + ": Frequency Filter controls");

            struct Item { juce::Component* c; juce::Rectangle<int> group; };
            std::vector<Item> items;
            if (tone)
                items = { { &ed->getLowSwitch(), ed->getLowGroup() }, { &ed->getLowBoostKnob(), ed->getLowGroup() }, { &ed->getLowCutKnob(), ed->getLowGroup() },
                          { &ed->getHighBoostSwitch(), ed->getHighBoostGroup() }, { &ed->getHighBoostKnob(), ed->getHighBoostGroup() }, { &ed->getBandwidthKnob(), ed->getHighBoostGroup() },
                          { &ed->getHighCutSwitch(), ed->getHighCutGroup() }, { &ed->getHighCutKnob(), ed->getHighCutGroup() } };
            else if (mid)
                items = { { &ed->getMidLowSwitch(), ed->getMidLowGroup() }, { &ed->getMidLowKnob(), ed->getMidLowGroup() },
                          { &ed->getMidDipSwitch(), ed->getMidDipGroup() }, { &ed->getMidDipKnob(), ed->getMidDipGroup() },
                          { &ed->getMidHighSwitch(), ed->getMidHighGroup() }, { &ed->getMidHighKnob(), ed->getMidHighGroup() } };
            else
                items = { { &ed->getHighPassSwitch(), ed->getHighPassGroup() }, { &ed->getLowPassSwitch(), ed->getLowPassGroup() } };

            const juce::Rectangle<int> design (0, 0, L::designWidth, L::contentHeight);
            for (const auto& it : items)
            {
                expect (it.group.contains (it.c->getBounds()), context + ": a control stays inside its group: " + it.c->getTitle());
                expect (design.contains (it.c->getBounds()), context + ": inside the editor");
                expect (it.c->getY() >= ed->getCurveBounds().getBottom(), context + ": below the graph");
                expect (it.c->getRight() < ed->getMeterBounds().getX(), context + ": left of the meter");
                for (const auto& other : items)
                    if (other.c != it.c)
                        expect (! it.c->getBounds().intersects (other.c->getBounds()), context + ": no overlap between " + it.c->getTitle() + " and " + other.c->getTitle());
            }
            expect (ed->getCurveBounds().getBottom() < items.front().group.getY(), context + ": the graph is above the groups");
            expect (ed->getMeterBounds().getRight() <= L::designWidth - 16 && ed->getMeterBounds().getBottom() <= L::contentHeight, context + ": the meter stays inside");
            expect (ed->getDiagnosticsBounds().getY() >= L::contentHeight, context + ": the Processing Load strip is below everything");
            expect (ed->getBypassButton().isVisible() && ed->getPresetBox().isVisible() && ed->getUndoButton().isVisible(), context + ": the title bar is intact");
            expectEquals (ed->getWidth(), width);
            expectEquals (ed->getHeight(), L::heightFor (width));
        }

        beginTest ("The Mid Shaper's two-row switches show every frequency, one choice lit, and a press is one choice");
        {
            SongsmithPassiveEQProcessor p;
            setParam (p, Peq::modeId, 1.0f);
            auto ed = make (p, L::defaultWidth);
            auto& dip = ed->getMidDipSwitch();
            expectEquals (dip.getNumButtons(), 10);
            expectEquals (dip.getRows(), 2);
            expectEquals (dip.getButton (0).getButtonText(), juce::String ("200 Hz"));
            expectEquals (dip.getButton (5).getButtonText(), juce::String ("1.5 kHz"));
            expectEquals (dip.getButton (9).getButtonText(), juce::String ("7 kHz"));
            expect (dip.getButton (5).getY() > dip.getButton (0).getY(), "the second row is below the first");
            dip.getButton (7).onClick();
            expectEquals (getParam (p, Peq::midDipFrequencyId), 7.0f);
            expect (dip.getButton (7).getToggleState() && ! dip.getButton (4).getToggleState(), "the chosen one is lit");

            setParam (p, Peq::modeId, 2.0f);
            auto f = make (p, L::defaultWidth);
            expectEquals (f->getHighPassSwitch().getButton (0).getButtonText(), juce::String ("Off"));
            expectEquals (f->getHighPassSwitch().getNumButtons(), 6);
            expectEquals (f->getLowPassSwitch().getNumButtons(), 5);
            f->getLowPassSwitch().getButton (3).onClick();
            expectEquals (getParam (p, Peq::lowPassFrequencyId), 3.0f);
        }

        beginTest ("The response display is the engine's own response in every mode and follows the settings");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, L::defaultWidth);
            auto check = [&] (const juce::String& context)
            {
                ed->refreshCurve();
                SongsmithPassive::Engine e;
                e.prepare (48000.0);
                e.setParams (p.getEngineParams());
                for (double hz : { 25.0, 60.0, 250.0, 1000.0, 3000.0, 9000.0, 18000.0 })
                    expectWithinAbsoluteError (ed->getCurve().responseDb ((float) hz), e.responseDb (hz), 1.0e-3f, context + " at " + juce::String (hz) + " Hz");
            };
            setParam (p, Peq::lowBoostId, 8.0f);
            check ("Tone Shaper");
            expectGreaterThan (ed->getCurve().responseDb (60.0f), 3.0f);
            setParam (p, Peq::modeId, 1.0f);
            setParam (p, Peq::midDipId, 7.0f);
            check ("Mid Shaper");
            expectLessThan (ed->getCurve().responseDb (1000.0f), -4.0f, "the dip shows");
            expectLessThan (std::abs (ed->getCurve().responseDb (60.0f)), 0.5f, "and the Tone Shaper's boost is not drawn");
            setParam (p, Peq::modeId, 2.0f);
            setParam (p, Peq::highPassFrequencyId, 4.0f);
            check ("Frequency Filter");
            expectLessThan (ed->getCurve().responseDb (30.0f), -10.0f, "the filter's cut shows");
            setParam (p, Peq::bypassId, 0.0f);
        }

        beginTest ("Plain words in every mode: every text, parameter name and tooltip, and every label fits");
        for (int mode = 0; mode < 3; ++mode)
        for (int width : { L::defaultWidth, L::minWidth })
        {
            SongsmithPassiveEQProcessor p;
            setParam (p, Peq::modeId, (float) mode);
            auto ed = make (p, width);
            const auto context = "Passive EQ mode " + juce::String (mode) + " at " + juce::String (width) + " px";
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            for (auto& pr : Peq::Presets::factory())
            {
                texts.add (pr.name);
                texts.add (pr.hint);
            }
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
            for (auto* t : { "EQ Mode", "Bypass", "Undo", "Redo" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("The names say what they do: starting points are named for their use, and no mode name is a hardware model");
        {
            for (const auto& f : Peq::Presets::factory())
            {
                const juce::String name (f.name);
                expect (! name.containsIgnoreCase ("EQP") && ! name.containsIgnoreCase ("MEQ") && ! name.containsIgnoreCase ("HLF") && ! name.containsIgnoreCase ("Pultec"),
                        name + " is named for what it is used for");
                expect (! juce::String (f.hint).containsIgnoreCase ("Pultec") && ! juce::String (f.hint).containsIgnoreCase ("Gilder"), name + " has a cue that stands on its own");
            }
            for (const auto& mode : Peq::modeNames())
                expect (! mode.containsIgnoreCase ("Pultec") && ! mode.contains ("-"), mode + " is a Songsmith name");
        }
    }
};

static PassiveEQModePluginTests passiveEQModePluginTests;
static PassiveEQModeEditorTests passiveEQModeEditorTests;
