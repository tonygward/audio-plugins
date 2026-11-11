#pragma once

// SUI-18 checks, compiled into every plugin's test executable:
//   Songsmith::UI::Testing::checkSuiteHeader (*this, *editor, "context");           // in an editor test
//   Songsmith::UI::Testing::checkParameterHistory (*this, processor, id, other, "context");

#include "SongsmithUITests.h"
#include "../Source/SongsmithHeader.h"
#include "../Source/SongsmithGuidance.h"

namespace Songsmith::UI::Testing
{
    // The title bar reads name | preset | Undo | Redo | Bypass, and holds nothing else.
    inline void checkSuiteHeader (juce::UnitTest& t, juce::Component& editor, const juce::String& context)
    {
        auto* undo = findButton (editor, "Undo");
        auto* redo = findButton (editor, "Redo");
        auto* bypass = findButton (editor, "Bypass");
        t.expect (undo != nullptr && redo != nullptr && bypass != nullptr, context + ": Undo, Redo and Bypass exist");
        if (undo == nullptr || redo == nullptr || bypass == nullptr)
            return;

        auto& content = *bypass->getParentComponent();
        t.expect (undo->getParentComponent() == &content && redo->getParentComponent() == &content, context + ": same parent");

        t.expectEquals (undo->getY(), redo->getY(), context + ": Undo and Redo on one line");
        t.expectEquals (undo->getY(), bypass->getY(), context + ": Bypass on the same line");
        t.expectEquals (undo->getWidth(), Header::historyButtonWidth, context + ": Undo width");
        t.expectEquals (redo->getWidth(), Header::historyButtonWidth, context + ": Redo width");
        t.expectEquals (bypass->getWidth(), Header::bypassWidth, context + ": Bypass width");
        t.expect (undo->getRight() < redo->getX() && redo->getRight() < bypass->getX(), context + ": order is Undo, Redo, Bypass");

        // Everything else that sits in the title bar: exactly one thing, the preset selector, before Undo.
        int others = 0;
        for (auto* c : content.getChildren())
            // The invisible title hint (SUI-17) sits over the painted name; it is not a control.
            if (c->isVisible() && c != undo && c != redo && c != bypass && c->getBottom() <= 50
                && dynamic_cast<Songsmith::Guidance::TitleHint*> (c) == nullptr)
            {
                ++others;
                t.expect (c->getRight() <= undo->getX(), context + ": the preset selector comes before Undo");
                t.expectEquals (c->getWidth(), Header::presetWidth, context + ": preset width");
            }
        t.expectEquals (others, 1, context + ": the title bar holds only the preset selector besides Undo, Redo and Bypass");

        // Keyboard order: preset, Undo, Redo, Bypass.
        t.expect (undo->getExplicitFocusOrder() < redo->getExplicitFocusOrder()
                      && redo->getExplicitFocusOrder() < bypass->getExplicitFocusOrder(),
                  context + ": keyboard order is Undo, Redo, Bypass");
    }

    // Undo and Redo of what the musician does in the window, and not of host automation.
    // `id` and `otherId` are two automatable parameters.
    template <typename Processor>
    inline void checkParameterHistory (juce::UnitTest& t, Processor& p, const char* id, const char* otherId, const juce::String& context)
    {
        auto& history = p.history;
        history.clear();
        auto* a = p.apvts.getParameter (id);
        auto* b = p.apvts.getParameter (otherId);
        const float startA = a->getValue(), startB = b->getValue();
        auto closeTo = [] (float x, float y) { return std::abs (x - y) < 1.0e-4f; };   // not "near": a Windows macro

        auto userEdit = [] (juce::RangedAudioParameter* prm, float value)
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (value);
            prm->endChangeGesture();
        };
        const float v1 = startA < 0.5f ? 0.6f : 0.1f, v2 = startA < 0.5f ? 0.8f : 0.3f, v3 = startA < 0.5f ? 1.0f : 0.0f;

        t.expect (! history.canUndo() && ! history.canRedo(), context + ": nothing to undo at first");

        userEdit (a, v1);
        userEdit (a, v2);
        userEdit (a, v3);
        t.expect (history.canUndo(), context + ": user edits can be undone");

        history.undo();
        t.expect (closeTo (a->getValue(), v2), context + ": one undo goes back one edit");
        history.undo();
        history.undo();
        t.expect (closeTo (a->getValue(), startA),context + ": three undos return to the start");
        t.expect (! history.canUndo() && history.canRedo(), context + ": buttons: nothing left to undo, redo available");

        history.redo();
        t.expect (closeTo (a->getValue(), v1), context + ": redo reapplies the first edit");

        userEdit (a, v3);   // a new edit ends the redo chain
        t.expect (! history.canRedo(), context + ": a new edit clears redo");

        // Host automation (a change with no gesture) is never recorded.
        history.clear();
        a->setValueNotifyingHost (v2);
        t.expect (! history.canUndo(), context + ": host automation is not an undo step");

        // One step, however many parameters it changes (a preset load).
        history.runStep ("preset load", [&] { a->setValueNotifyingHost (v1); b->setValueNotifyingHost (startB < 0.5f ? 0.9f : 0.1f); });
        history.undo();
        t.expect (closeTo (a->getValue(), v2) && closeTo (b->getValue(), startB),context + ": a preset load undoes in one step");
        t.expect (! history.canUndo(), context + ": and was only one step");
        history.redo();
        t.expect (closeTo (a->getValue(), v1), context + ": and redoes in one step");

        // Undo and redo themselves do not become steps.
        history.clear();
        userEdit (a, v3);
        history.undo();
        history.redo();
        t.expect (history.canUndo() && ! history.canRedo(), context + ": undo and redo leave the history consistent");

        // Leave the processor as found.
        history.clear();
        a->setValueNotifyingHost (startA);
        b->setValueNotifyingHost (startB);
    }
}
