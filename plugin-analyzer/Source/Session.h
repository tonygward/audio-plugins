#pragma once

#include <array>
#include <atomic>
#include <cstdint>

// ANALYZER-01: how a Reference and a Result find each other.
//
// Both instances live in the same program (the host), so they share eight numbered sessions held in this one place. A session is
// only a handful of atomic numbers. Nothing here allocates, locks or waits, so the audio thread may read and write it freely.
// Two instances only ever meet when they are in the same session number, and a different session never sees them.
//
// Hosts that run each plugin in its own process cannot pair instances this way. The Result then never sees a Reference, and says so.
namespace PluginAnalyzer
{
    inline constexpr int numSessions = 8;

    enum class Role { reference = 0, result = 1 };

    struct Session
    {
        std::atomic<int> references { 0 };      // how many Reference instances have chosen this session
        std::atomic<int> results { 0 };         // how many Result instances
        std::atomic<std::uint32_t> generation { 0 };       // raised each time a measurement starts
        std::atomic<std::uint32_t> abortedGeneration { 0 };  // the generation that was stopped before it finished
        std::atomic<bool> measuring { false };  // the Reference is playing its test program
        std::atomic<bool> startRequested { false };
        std::atomic<bool> stopRequested { false };
        std::atomic<float> testLevelDb { -18.0f };           // the level the Reference is sending
        std::atomic<double> sampleRate { 0.0 };              // the Reference's sample rate
        std::atomic<std::int64_t> sentSamples { 0 };         // how much of the program has been played (for progress)
        std::atomic<std::int64_t> programLength { 0 };

        // Back to the start (used when the last instance leaves, and by Reset).
        void clear() noexcept
        {
            measuring.store (false);
            startRequested.store (false);
            stopRequested.store (false);
            sentSamples.store (0);
        }
    };

    // The eight sessions. They live for the whole life of the program, so an instance can keep a plain pointer to one.
    inline Session& sessionNumber (int index) noexcept
    {
        static std::array<Session, (size_t) numSessions> sessions;
        return sessions[(size_t) (index < 0 ? 0 : (index >= numSessions ? numSessions - 1 : index))];
    }
}
