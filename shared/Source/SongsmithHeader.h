#pragma once

// SUI-18: the suite title bar, the same in every Songsmith plugin.
//
//     plugin name | preset | Undo | Redo ............................ Bypass (far right)
//
// This file holds the shared layout (so sizes and spacing cannot drift between plugins) and a small,
// honest Undo/Redo history for plugins that have no edit controller of their own (Analog Compressor,
// Maximiser). Songsmith EQ keeps its own edit controller and uses only the layout.
//
// What the history records: changes the musician makes in the plugin window (a knob drag, a button
// click, choosing a preset), each as one step. What it never records: host automation and anything
// the host does outside this window. A host's own Undo is separate and is not touched.

#include <juce_audio_processors/juce_audio_processors.h>
#include "SongsmithType.h"

namespace Songsmith::UI::Header
{
    // Design units, the same in every plugin.
    constexpr int presetWidth = 220;
    constexpr int historyButtonWidth = 60;
    constexpr int bypassWidth = 84;
    constexpr int gap = 8;               // between preset, Undo, Redo (Undo and Redo sit closer together)
    constexpr int historyGap = 6;
    constexpr int titleToPreset = 16;
    constexpr int buttonHeight = 26;

    struct Layout
    {
        juce::Rectangle<int> title, preset, undo, redo, bypass;
    };

    // `row` is the strip the controls live in (already inset from the window edges).
    inline Layout layout (juce::Rectangle<int> row, const juce::String& pluginName)
    {
        Layout l;
        l.title = row.removeFromLeft ((int) std::ceil (titleWidth (pluginName)) + 4);
        row.removeFromLeft (titleToPreset);
        l.preset = row.removeFromLeft (presetWidth);
        row.removeFromLeft (gap);
        l.undo = row.removeFromLeft (historyButtonWidth);
        row.removeFromLeft (historyGap);
        l.redo = row.removeFromLeft (historyButtonWidth);
        l.bypass = row.removeFromRight (bypassWidth);
        return l;
    }

    // A row of the standard height, centred in a title bar `barHeight` tall.
    inline juce::Rectangle<int> rowIn (int x, int width, int barHeight)
    {
        return { x, (barHeight - buttonHeight) / 2, width, buttonHeight };
    }

    //==============================================================================
    // Message-thread only. Records one step per user gesture (or per runStep call) as the parameter
    // values before and after, and puts them back on Undo / Redo. Host automation sends no gestures,
    // so it is never recorded.
    class ParameterHistory : private juce::AudioProcessorParameter::Listener
    {
    public:
        explicit ParameterHistory (juce::AudioProcessor& processor)
        {
            for (auto* p : processor.getParameters())
            {
                params.add (p);
                p->addListener (this);
            }
        }

        ~ParameterHistory() override
        {
            for (auto* p : params)
                p->removeListener (this);
        }

        bool canUndo() const noexcept   { return ! undoSteps.empty(); }
        bool canRedo() const noexcept   { return ! redoSteps.empty(); }
        juce::String getUndoName() const { return canUndo() ? undoSteps.back().name : juce::String(); }
        juce::String getRedoName() const { return canRedo() ? redoSteps.back().name : juce::String(); }

        bool undo()
        {
            if (! canUndo())
                return false;
            auto step = undoSteps.back();
            undoSteps.pop_back();
            apply (step.before);
            redoSteps.push_back (std::move (step));
            return true;
        }

        bool redo()
        {
            if (! canRedo())
                return false;
            auto step = redoSteps.back();
            redoSteps.pop_back();
            apply (step.after);
            undoSteps.push_back (std::move (step));
            return true;
        }

        // Runs `change` (for example loading a preset) and records everything it changed as one step.
        void runStep (const juce::String& name, const std::function<void()>& change)
        {
            const auto before = capture();
            {
                const juce::ScopedValueSetter<bool> guard (inStep, true);
                change();
            }
            commit (name, before, capture());
        }

        void clear()
        {
            undoSteps.clear();
            redoSteps.clear();
        }

    private:
        using Snapshot = std::vector<float>;
        struct Step { juce::String name; Snapshot before, after; };
        static constexpr size_t maxSteps = 200;

        Snapshot capture() const
        {
            Snapshot s;
            s.reserve ((size_t) params.size());
            for (auto* p : params)
                s.push_back (p->getValue());
            return s;
        }

        void apply (const Snapshot& target)
        {
            const juce::ScopedValueSetter<bool> guard (applying, true);
            for (int i = 0; i < params.size() && (size_t) i < target.size(); ++i)
            {
                auto* p = params.getUnchecked (i);
                if (std::abs (p->getValue() - target[(size_t) i]) > 1.0e-6f)
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (target[(size_t) i]);
                    p->endChangeGesture();
                }
            }
        }

        void commit (const juce::String& name, const Snapshot& before, const Snapshot& after)
        {
            bool changed = before.size() != after.size();
            for (size_t i = 0; i < before.size() && ! changed; ++i)
                changed = std::abs (before[i] - after[i]) > 1.0e-6f;
            if (! changed)
                return;

            undoSteps.push_back ({ name, before, after });
            if (undoSteps.size() > maxSteps)
                undoSteps.erase (undoSteps.begin());
            redoSteps.clear();
        }

        void parameterValueChanged (int, float) override {}

        void parameterGestureChanged (int index, bool starting) override
        {
            if (applying || inStep)
                return;

            if (starting)
            {
                if (gestureDepth++ == 0)
                {
                    gestureBefore = capture();
                    gestureName = juce::isPositiveAndBelow (index, params.size()) ? params[index]->getName (40) : juce::String ("change");
                }
            }
            else if (gestureDepth > 0 && --gestureDepth == 0)
            {
                commit (gestureName, gestureBefore, capture());
            }
        }

        juce::Array<juce::AudioProcessorParameter*> params;
        std::vector<Step> undoSteps, redoSteps;
        Snapshot gestureBefore;
        juce::String gestureName;
        int gestureDepth = 0;
        bool applying = false, inStep = false;
    };

    //==============================================================================
    // Button wording and state, the same everywhere.
    inline void refreshHistoryButtons (juce::Button& undoButton, juce::Button& redoButton, const ParameterHistory& history)
    {
        undoButton.setEnabled (history.canUndo());
        redoButton.setEnabled (history.canRedo());
        undoButton.setTooltip (history.canUndo() ? "Undo " + history.getUndoName().toLowerCase() + " (Ctrl+Z). Only changes you made in this window."
                                                 : juce::String ("Undo (Ctrl+Z): nothing to undo yet"));
        redoButton.setTooltip (history.canRedo() ? "Redo " + history.getRedoName().toLowerCase() + " (Ctrl+Y)"
                                                 : juce::String ("Redo (Ctrl+Y): nothing to redo"));
    }

    // Ctrl/Cmd+Z undoes, Ctrl/Cmd+Y or Ctrl/Cmd+Shift+Z redoes. Returns true when the key was used.
    inline bool handleHistoryKey (const juce::KeyPress& key, ParameterHistory& history)
    {
        const auto mods = key.getModifiers();
        const int code = key.getKeyCode();
        if (! mods.isCommandDown())
            return false;

        if (code == 'Z' || code == 'z')
        {
            if (mods.isShiftDown()) history.redo();
            else                    history.undo();
            return true;
        }
        if (code == 'Y' || code == 'y')
        {
            history.redo();
            return true;
        }
        return false;
    }
}
