#pragma once

// songsmithstudio#8: shared checks for the update check, compiled into each plugin's test executable:
//   static SongsmithUpdateTests sharedUpdateTests { "MyPlugin" };   // once per executable
// Nothing here touches the network or the real settings folder: the fetcher, the clock and the cache file are replaced.

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "SongsmithDiagnosticsTests.h"
#include "../Source/SongsmithUpdateCheck.h"

class SongsmithUpdateTests final : public juce::UnitTest
{
public:
    explicit SongsmithUpdateTests (const juce::String& category) : juce::UnitTest ("Shared Songsmith Update Check", category) {}

    static juce::String metadata (const juce::String& plugin, const juce::String& version, const juce::String& channel = "beta",
                                  const juce::String& page = "https://beta.songsmith.uk/", int schema = 1)
    {
        return "{\"schema\":" + juce::String (schema) + ",\"plugin\":\"" + plugin + "\",\"channel\":\"" + channel + "\",\"version\":\"" + version
               + "\",\"date\":\"2026-11-21T10:00:00Z\",\"downloadPage\":\"" + page + "\",\"notesPage\":\"" + page + "\"}";
    }

    void runTest() override
    {
        using namespace Songsmith::Update;

        auto newer = [] (const char* latest, const char* installed) { return isNewer (latest, installed); };

        beginTest ("Newer, equal and older versions compare by number, not by text");
        {
            expect (newer ("0.5.64", "0.5.63"), "a newer patch");
            expect (! newer ("0.5.63", "0.5.63"), "the same version is not newer");
            expect (! newer ("0.5.62", "0.5.63"), "an older version is not newer");
            expect (newer ("0.5.10", "0.5.9"), "0.5.10 is newer than 0.5.9 (numbers, not text)");
            expect (! newer ("0.5.9", "0.5.10"));
            expect (newer ("0.6.0", "0.5.99") && newer ("1.0.0", "0.99.99") && newer ("10.0.0", "9.99.99"));
            expect (newer ("v0.5.64", "0.5.63") && ! newer ("v0.5.63", "v0.5.63"), "a leading v is allowed");
            expect (newer ("1.0.0", "1.0.0-beta.2"), "a final version is newer than a prerelease of it");
            expect (! newer ("1.0.0-beta.2", "1.0.0"));
            expect (newer ("1.0.0-beta.11", "1.0.0-beta.2"), "prerelease numbers compare as numbers");
            expect (newer ("1.0.0-beta.1", "1.0.0-alpha.9"), "and words in order");
            expect (newer ("1.0.0-alpha.1", "1.0.0-alpha"), "more parts are newer");
            expect (newer ("1.0.0-alpha.beta", "1.0.0-alpha.1"), "a word sorts after a number");
            expect (! newer ("1.0.0+build7", "1.0.0+build1"), "build metadata is ignored");
            for (const char* bad : { "", "1.2", "1.2.3.4", "1.2.x", "01.2.3", "1..3", "a.b.c", "1.2.3-", "99999999999.0.0", "1.2.3-be ta", "latest" })
            {
                expect (! newer (bad, "0.0.1") && ! newer ("9.9.9", bad), juce::String ("unreadable versions are never newer: \"") + bad + "\"");
                Version v;
                expect (! parseVersion (bad, v), juce::String ("and are refused: \"") + bad + "\"");
            }
        }

        beginTest ("Beta and stable builds are told apart, and each looks only at its own channel");
        {
            auto channelOf = [] (const char* v) { Version p; parseVersion (v, p); return channelFor (p); };
            expect (channelOf ("0.5.63") == Channel::beta, "today's builds are beta");
            expect (channelOf ("1.0.0-beta.1") == Channel::beta && channelOf ("2.3.4-rc.1") == Channel::beta, "any prerelease is beta");
            expect (channelOf ("1.0.0") == Channel::stable && channelOf ("3.2.1") == Channel::stable);
            expectEquals (metadataUrl (Channel::beta, "analog-desk"), juce::String ("https://beta.songsmith.uk/updates/analog-desk/latest.json"));
            expect (metadataUrl (Channel::stable, "analog-desk") != metadataUrl (Channel::beta, "analog-desk"), "the two channels never share an address");
            expect (metadataUrl (Channel::stable, "eq").startsWith ("https://") && metadataUrl (Channel::beta, "eq").startsWith ("https://"), "always HTTPS");
        }

        beginTest ("Every product is published under its own plain identifier");
        {
            const std::pair<const char*, const char*> names[] = {
                { "Songsmith EQ", "eq" }, { "Songsmith Analog Compressor", "analog-compressor" }, { "Songsmith Maximiser", "maximiser" },
                { "Songsmith Compressor", "compressor" }, { "Songsmith Optical Leveller", "optical-leveller" }, { "Songsmith Passive EQ", "passive-eq" },
                { "Songsmith Bus Compressor", "bus-compressor" }, { "Songsmith Analog Desk", "analog-desk" },
                { "Songsmith Plugin Analyzer", "plugin-analyzer" }, { "Songsmith Audio Analyzer", "audio-analyzer" } };
            for (auto& [product, id] : names)
                expectEquals (pluginIdFor (product), juce::String (id));
            expectEquals (pluginIdFor ("  Songsmith   Odd  Name!! "), juce::String ("odd-name"));
        }

        beginTest ("A published answer is accepted only when it is the right plugin and channel and a Songsmith page");
        {
            Metadata m;
            expect (parseMetadata (metadata ("eq", "0.5.64"), "eq", Channel::beta, m) && m.version == "0.5.64" && m.downloadPage == "https://beta.songsmith.uk/");
            expect (! parseMetadata (metadata ("eq", "0.5.64"), "maximiser", Channel::beta, m), "another plugin's answer");
            expect (! parseMetadata (metadata ("eq", "0.5.64", "stable"), "eq", Channel::beta, m), "another channel's answer");
            expect (! parseMetadata (metadata ("eq", "0.5.64", "beta", "https://beta.songsmith.uk/", 2), "eq", Channel::beta, m), "an unknown format");
            expect (! parseMetadata (metadata ("eq", "not-a-version"), "eq", Channel::beta, m), "an unreadable version");
            for (const char* page : { "http://beta.songsmith.uk/", "https://evil.example/", "https://songsmith.uk.evil.example/", "https://evilsongsmith.uk/",
                                      "https://beta.songsmith.uk@evil.example/", "javascript:alert(1)", "", "https://beta.songsmith.uk/ x" })
                expect (! parseMetadata (metadata ("eq", "0.5.64", "beta", page), "eq", Channel::beta, m), juce::String ("a download page of \"") + page + "\" is refused");
            expect (parseMetadata (metadata ("eq", "0.5.64", "beta", "https://songsmith.uk/download"), "eq", Channel::beta, m), "the main site is allowed too");
            for (const char* junk : { "", "not json", "[]", "null", "{}", "{\"schema\":1}", "<html>Access login</html>", "{\"schema\":\"1\"}" })
                expect (! parseMetadata (junk, "eq", Channel::beta, m), juce::String ("damaged answers are refused: ") + junk);
            expect (! parseMetadata (metadata ("eq", "0.5.64") + juce::String::repeatedString (" ", 20000), "eq", Channel::beta, m), "an oversized answer");
        }

        beginTest ("A look is due at most once a day, and a failed look is tried again after an hour");
        {
            Remembered never;
            expect (dueForCheck (never, 1000), "never looked");
            const juce::int64 t0 = 1'700'000'000'000;
            Remembered good { t0, true, "0.5.64", "https://beta.songsmith.uk/" };
            expect (! dueForCheck (good, t0 + hour) && ! dueForCheck (good, t0 + day - 1), "not due within 24 hours");
            expect (dueForCheck (good, t0 + day), "due after 24 hours");
            Remembered failed { t0, false, {}, {} };
            expect (! dueForCheck (failed, t0 + hour - 1) && dueForCheck (failed, t0 + hour), "a failed look is retried after one hour");
            expect (dueForCheck (good, t0 - 10 * day), "a clock that went backwards looks again");
        }

        // The checker itself, with a pretend network, clock and settings file.
        auto cacheIn = [] (const juce::File& dir) { return dir.getChildFile ("update-check.json"); };
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("songsmith-update-test-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        temp.createDirectory();
        struct Cleanup { juce::File f; ~Cleanup() { f.deleteRecursively(); } } cleanup { temp };

        struct World
        {
            std::atomic<juce::int64> clock { 1'700'000'000'000 };
            std::atomic<int> fetches { 0 };
            juce::CriticalSection lock;
            juce::StringArray urls;
            juce::String answer;
            int delayMs = 0;

            Songsmith::Update::Checker::Fetcher fetcher()
            {
                return [this] (const juce::String& url)
                {
                    if (delayMs > 0)
                        juce::Thread::sleep (delayMs);
                    ++fetches;
                    const juce::ScopedLock sl (lock);
                    urls.add (url);
                    return answer;
                };
            }
        };

        auto attach = [&] (Checker& c, World& w, const juce::File& cache)
        {
            c.setFetcherForTesting (w.fetcher());
            c.setClockForTesting ([&w] { return (juce::int64) w.clock.load(); });
            c.setCacheFileForTesting (cache);
        };

        beginTest ("A newer build is offered, an equal or older one is not, and the link goes to the download page");
        {
            struct Case { const char* installed; const char* latest; bool offered; };
            for (auto c : { Case { "0.5.63", "0.5.64", true }, Case { "0.5.63", "0.5.63", false }, Case { "0.5.63", "0.5.62", false }, Case { "0.5.9", "0.5.10", true } })
            {
                World w;
                w.answer = metadata ("analog-desk", c.latest);
                Checker checker;
                attach (checker, w, cacheIn (temp.getChildFile (juce::String (c.installed) + c.latest)));
                checker.check ("Songsmith Analog Desk", c.installed);
                checker.waitUntilIdleForTesting (3000);
                const auto offer = checker.offerFor ("Songsmith Analog Desk", c.installed);
                expect (offer.available == c.offered, juce::String ("installed ") + c.installed + ", latest " + c.latest + ": offered = " + (c.offered ? "yes" : "no"));
                if (c.offered)
                    expect (offer.version == c.latest && offer.downloadPage == "https://beta.songsmith.uk/");
            }
        }

        beginTest ("Beta and stable builds are isolated: each asks only its own channel and ignores the other's answer");
        {
            World w;
            w.answer = metadata ("eq", "2.0.0", "stable", "https://songsmith.uk/");
            Checker stable;
            attach (stable, w, cacheIn (temp.getChildFile ("iso-stable")));
            stable.check ("Songsmith EQ", "1.0.0");
            stable.waitUntilIdleForTesting (3000);
            expect (w.urls.size() == 1 && w.urls[0] == metadataUrl (Channel::stable, "eq"), "a stable build asked the stable address");
            expect (stable.offerFor ("Songsmith EQ", "1.0.0").available, "and is offered the stable release");

            World b;
            b.answer = metadata ("eq", "2.0.0", "stable", "https://songsmith.uk/");   // a stable answer arriving at a beta address
            Checker beta;
            attach (beta, b, cacheIn (temp.getChildFile ("iso-beta")));
            beta.check ("Songsmith EQ", "0.5.63");
            beta.waitUntilIdleForTesting (3000);
            expect (b.urls.size() == 1 && b.urls[0] == metadataUrl (Channel::beta, "eq"), "a beta build asked the beta address");
            expect (! beta.offerFor ("Songsmith EQ", "0.5.63").available, "and refused a stable answer");

            // One file can hold both without mixing them up.
            Checker both;
            World w2;
            w2.answer = metadata ("eq", "0.5.70");
            attach (both, w2, cacheIn (temp.getChildFile ("iso-both")));
            both.check ("Songsmith EQ", "0.5.63");
            both.waitUntilIdleForTesting (3000);
            expect (both.offerFor ("Songsmith EQ", "0.5.63").available, "the beta build is offered the beta release");
            expect (! both.offerFor ("Songsmith EQ", "1.0.0").available, "while a stable build is not offered it");
        }

        beginTest ("Several windows and several plugins share one look a day, and what was learned");
        {
            World w;
            w.answer = metadata ("compressor", "0.5.70");
            const auto cache = cacheIn (temp.getChildFile ("shared"));
            Checker first;
            attach (first, w, cache);
            for (int i = 0; i < 6; ++i)
                first.check ("Songsmith Compressor", "0.5.63");   // six windows opening together
            first.waitUntilIdleForTesting (3000);
            expectEquals (w.fetches.load(), 1, "one request for six windows");

            first.check ("Songsmith Compressor", "0.5.63");
            first.waitUntilIdleForTesting (3000);
            expectEquals (w.fetches.load(), 1, "and none for a window opened a minute later");

            Checker otherProgram;   // another DAW, another instance of the same plugin, later the same day
            attach (otherProgram, w, cache);
            w.clock += 5 * hour;
            otherProgram.check ("Songsmith Compressor", "0.5.63");
            otherProgram.waitUntilIdleForTesting (3000);
            expectEquals (w.fetches.load(), 1, "another program the same day reuses the saved answer");
            expect (otherProgram.offerFor ("Songsmith Compressor", "0.5.63").available, "and shows the update");

            w.clock += 20 * hour;   // 25 hours in all
            otherProgram.check ("Songsmith Compressor", "0.5.63");
            otherProgram.waitUntilIdleForTesting (3000);
            expectEquals (w.fetches.load(), 2, "a day later it looks again");
        }

        beginTest ("Offline, a timeout or a damaged answer is quiet: no offer, no error, and a retry only after an hour");
        {
            World w;   // the network gives nothing at all
            Checker checker;
            attach (checker, w, cacheIn (temp.getChildFile ("offline")));
            checker.check ("Songsmith Bus Compressor", "0.5.63");
            checker.waitUntilIdleForTesting (3000);
            expectEquals (w.fetches.load(), 1);
            expect (! checker.offerFor ("Songsmith Bus Compressor", "0.5.63").available, "nothing to show");
            w.clock += 30 * 60 * 1000;
            checker.check ("Songsmith Bus Compressor", "0.5.63");
            checker.waitUntilIdleForTesting (3000);
            expectEquals (w.fetches.load(), 1, "not retried after half an hour");
            w.clock += 31 * 60 * 1000;
            w.answer = metadata ("bus-compressor", "0.5.70");   // back online
            checker.check ("Songsmith Bus Compressor", "0.5.63");
            checker.waitUntilIdleForTesting (3000);
            expectEquals (w.fetches.load(), 2, "retried after an hour");
            expect (checker.offerFor ("Songsmith Bus Compressor", "0.5.63").available, "and the update appears");

            for (const char* bad : { "<html>Please sign in</html>", "{\"schema\":1}", "{{{{", "" })
            {
                World x;
                x.answer = bad;
                Checker c;
                attach (c, x, cacheIn (temp.getChildFile (juce::String ("bad") + juce::String (juce::String (bad).hashCode()))));
                c.check ("Songsmith EQ", "0.5.63");
                c.waitUntilIdleForTesting (3000);
                expect (! c.offerFor ("Songsmith EQ", "0.5.63").available, juce::String ("damaged answer ignored: ") + bad);
            }

            // A damaged or unwritable settings file does not matter either.
            const auto broken = cacheIn (temp.getChildFile ("broken"));
            broken.getParentDirectory().createDirectory();
            broken.replaceWithText ("this is not json {{{");
            World y;
            y.answer = metadata ("eq", "0.5.70");
            Checker c;
            attach (c, y, broken);
            c.check ("Songsmith EQ", "0.5.63");
            c.waitUntilIdleForTesting (3000);
            expect (c.offerFor ("Songsmith EQ", "0.5.63").available, "a damaged settings file is replaced");
        }

        beginTest ("A failed release leaves the previous answer in place, and the window never waits for the network");
        {
            World w;
            w.answer = metadata ("maximiser", "0.5.70");
            Checker checker;
            attach (checker, w, cacheIn (temp.getChildFile ("failed")));
            checker.check ("Songsmith Maximiser", "0.5.63");
            checker.waitUntilIdleForTesting (3000);
            expect (checker.offerFor ("Songsmith Maximiser", "0.5.63").available);

            w.clock += 25 * hour;
            w.answer = "";   // the site has a problem the next day
            checker.check ("Songsmith Maximiser", "0.5.63");
            checker.waitUntilIdleForTesting (3000);
            const auto offer = checker.offerFor ("Songsmith Maximiser", "0.5.63");
            expect (offer.available && offer.version == "0.5.70", "the previous answer is still shown");

            World slow;
            slow.answer = metadata ("maximiser", "0.5.80");
            slow.delayMs = 400;
            Checker c;
            attach (c, slow, cacheIn (temp.getChildFile ("slow")));
            const auto start = juce::Time::getMillisecondCounter();
            c.check ("Songsmith Maximiser", "0.5.63");
            const auto took = juce::Time::getMillisecondCounter() - start;
            expectLessThan ((int) took, 150, "check() returns at once while the request runs on its own thread");
            expect (! c.offerFor ("Songsmith Maximiser", "0.5.63").available, "nothing is shown until the answer arrives");
            c.waitUntilIdleForTesting (5000);
            expect (c.offerFor ("Songsmith Maximiser", "0.5.63").available, "and then it is");
        }

        beginTest ("The footer shows a quiet Update available link beside the version, and only when there is something to say");
        {
            juce::AudioProcessorGraph processor;
            Songsmith::Diagnostics::Testing::TemporarySettings settings;
            Songsmith::Diagnostics::LoadMeter meter;
            Songsmith::Diagnostics::Bar bar (processor, meter, "Songsmith Test");
            for (int width : { 720, 1280, 2000 })
            {
                bar.setSize (width, Songsmith::Diagnostics::Bar::designHeight);
                bar.getLoadSwitch().setToggleState (true, juce::sendNotificationSync);
                bar.refresh();
                const auto where = "at " + juce::String (width) + " px: ";
                expect (! bar.getUpdateLink().isVisible(), where + "hidden when there is nothing to say");
                bar.showUpdateForTesting ("0.5.70", "https://beta.songsmith.uk/");
                auto& link = bar.getUpdateLink();
                auto& version = bar.getVersionLabel();
                auto& sw = bar.getLoadSwitch();
                expect (link.isVisible(), where + "shown");
                expect (link.getButtonText() == "Update available", where + "plain words");
                expect (link.getX() >= version.getRight() && link.getX() - version.getRight() <= 16, where + "directly beside the version");
                expect (link.getRight() <= sw.getX(), where + "clear of the Processing Load switch");
                expect (link.getBounds().getY() >= 0 && link.getBottom() <= bar.getHeight(), where + "inside the strip");
                expect (link.getTooltip().contains ("0.5.70") && link.getTooltip().contains ("Nothing is installed"), where + "says what it is and that nothing happens by itself");
                expectEquals (link.getURL().toString (true), juce::String ("https://beta.songsmith.uk/"));
                bar.clearUpdateForTesting();
                expect (! bar.getUpdateLink().isVisible(), where + "and hidden again when there is nothing to say");
            }
        }
    }
};
