#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include "../../shared/Source/SongsmithFileAnalysis.h"

// REC-02: reads one file and measures it on a background thread, so the window never freezes on a long file.
//
//  - start() cancels whatever is being read and begins a new file; only the newest file's result is ever delivered.
//  - The result arrives on the message thread, through the callback.
//  - cancel() stops reading and delivers nothing.
//  - The file is only read. It is never written, copied or moved.
namespace Songsmith::Analyzer
{
    class AnalysisRunner : private juce::Thread, private juce::AsyncUpdater
    {
    public:
        using Callback = std::function<void (const juce::File&, const FileAnalysis::Result&)>;

        explicit AnalysisRunner (Callback onFinished) : juce::Thread ("Songsmith Audio Analyzer: reading a file"), finished (std::move (onFinished)) {}

        ~AnalysisRunner() override
        {
            cancelPendingUpdate();
            stopAndWait();
        }

        // Message thread only.
        void start (const juce::File& file)
        {
            stopAndWait();
            {
                const juce::ScopedLock lock (resultLock);
                hasResult = false;
            }
            cancelFlag.store (false);
            progressValue.store (0.0f);
            current = file;
            running.store (true);
            startThread (juce::Thread::Priority::background);
        }

        // Message thread only. Nothing is delivered for the file that was being read.
        void cancel()
        {
            cancelPendingUpdate();
            stopAndWait();
            const juce::ScopedLock lock (resultLock);
            hasResult = false;
        }

        bool isRunning() const noexcept { return running.load(); }
        float getProgress() const noexcept { return progressValue.load(); }
        juce::File getFile() const { return current; }

    private:
        void stopAndWait()
        {
            cancelFlag.store (true);
            stopThread (10000);
            running.store (false);
        }

        void run() override
        {
            auto result = FileAnalysis::analyseFile (current, &cancelFlag, &progressValue);
            if (cancelFlag.load() || result.cancelled)
            {
                running.store (false);
                return;
            }
            {
                const juce::ScopedLock lock (resultLock);
                latest = std::move (result);
                hasResult = true;
            }
            running.store (false);
            triggerAsyncUpdate();
        }

        void handleAsyncUpdate() override
        {
            FileAnalysis::Result r;
            {
                const juce::ScopedLock lock (resultLock);
                if (! hasResult)
                    return;
                r = latest;
                hasResult = false;
            }
            if (finished)
                finished (current, r);
        }

        Callback finished;
        juce::File current;
        std::atomic<bool> cancelFlag { false }, running { false };
        std::atomic<float> progressValue { 0.0f };
        juce::CriticalSection resultLock;
        FileAnalysis::Result latest;
        bool hasResult = false;
    };
}
