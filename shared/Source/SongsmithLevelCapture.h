#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

// Set Levels listening: copies the incoming (dry) audio into a buffer allocated in prepareToPlay, so the
// message thread can analyse it afterwards. Shared by every plugin with a Set Levels panel (songsmithstudio#19).
//
// The audio thread only calls push(): plain copies into preallocated memory, never allocating, locking,
// logging or touching a parameter. Everything else is for the message thread.
namespace Songsmith
{
    class LevelCapture
    {
    public:
        static constexpr double maxSeconds = 12.0;
        static constexpr float heardSoundThreshold = 1.0e-4f;   // -80 dBFS: quieter than this is silence, not something to listen to

        // Message thread, before audio runs (prepareToPlay). Allocates.
        void prepare (double sampleRate, int channels)
        {
            rate = sampleRate;
            numChannels = juce::jlimit (1, 2, channels);
            capacity = (int) (maxSeconds * sampleRate);
            for (auto& c : data)
                c.assign ((size_t) capacity, 0.0f);
            count.store (0);
            active.store (false);
            resetRequested.store (false);
            heard.store (false);
        }

        void start() noexcept
        {
            heard.store (false);
            resetRequested.store (true);
            count.store (0, std::memory_order_release);   // the audio thread also restarts from 0 on its next block
            active.store (true, std::memory_order_release);
        }

        void stop() noexcept { active.store (false, std::memory_order_release); }
        bool isActive() const noexcept { return active.load (std::memory_order_acquire); }
        int getSamples() const noexcept { return count.load (std::memory_order_acquire); }
        double getSeconds() const noexcept { return rate > 0.0 ? (double) getSamples() / rate : 0.0; }
        bool isFull() const noexcept { return getSamples() >= capacity && capacity > 0; }
        double getSampleRate() const noexcept { return rate; }

        // True once any sound louder than silence has arrived since start(): the ear shows "Listening" only then.
        bool hasHeardSound() const noexcept { return heard.load (std::memory_order_acquire); }

        // Samples below getSamples() never change until the next start().
        const float* channel (int c) const noexcept { return data[(size_t) juce::jmin (c, numChannels - 1)].data(); }

        // Audio thread. Real-time safe.
        void push (const float* const* ch, int numCh, int n) noexcept
        {
            if (! active.load (std::memory_order_acquire) || capacity == 0)
                return;
            int pos = resetRequested.exchange (false) ? 0 : count.load (std::memory_order_relaxed);
            const int room = juce::jmin (n, capacity - pos);
            float peak = 0.0f;
            for (int c = 0; c < numChannels; ++c)
            {
                const float* src = ch[juce::jmin (c, numCh - 1)];
                std::copy (src, src + room, data[(size_t) c].data() + pos);
                for (int i = 0; i < room; ++i)
                    peak = std::max (peak, std::abs (src[i]));
            }
            if (peak > heardSoundThreshold)
                heard.store (true, std::memory_order_release);
            count.store (pos + room, std::memory_order_release);
            if (pos + room >= capacity)
                active.store (false, std::memory_order_release);   // full: stop on its own
        }

    private:
        double rate = 0.0;
        int numChannels = 2, capacity = 0;
        std::vector<float> data[2];
        std::atomic<int> count { 0 };
        std::atomic<bool> active { false }, resetRequested { false }, heard { false };
    };
}
