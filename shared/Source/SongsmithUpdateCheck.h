#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>

// Songsmith update check (songsmithstudio#8): one shared implementation for every Songsmith plugin.
//
// When a plugin window opens it asks the Checker, which looks at most once every 24 hours (a failed look is tried again after
// one hour), reuses the answer across every instance and every Songsmith plugin on the computer (a small file in the
// Songsmith Studio folder), and does the network request and the parsing on its own background thread, never on the audio
// thread or the message thread. If the latest published version is newer than the one running, the footer shows an
// "Update available" link to the download page. Nothing is ever installed automatically. Offline, a timeout, a missing
// entry or a damaged answer is ignored quietly: no dialog, no delay, no change to the plugin.
//
// What is fetched: https://beta.songsmith.uk/updates/<plugin>/latest.json (see songsmith-website, src/updates.mjs). It holds a
// version number, a date and the download page, nothing else. What is sent: the plain request, nothing about the user.
namespace Songsmith::Update
{
    //==============================================================================
    // Versions: major.minor.patch with an optional -prerelease, compared as numbers (0.5.10 is newer than 0.5.9) and
    // with the usual prerelease rules (1.0.0-beta.2 is older than 1.0.0).
    struct Version
    {
        int major = 0, minor = 0, patch = 0;
        juce::StringArray prerelease;   // the dot-separated parts after the '-', empty for a final version
    };

    inline bool parseVersion (juce::String text, Version& out)
    {
        text = text.trim();
        if (text.startsWithChar ('v') || text.startsWithChar ('V'))
            text = text.substring (1);
        text = text.upToFirstOccurrenceOf ("+", false, false);   // build metadata does not count

        juce::String core = text, pre;
        if (text.containsChar ('-'))
        {
            core = text.upToFirstOccurrenceOf ("-", false, false);
            pre = text.fromFirstOccurrenceOf ("-", false, false);
            if (pre.isEmpty())
                return false;
        }

        auto parts = juce::StringArray::fromTokens (core, ".", "");
        if (parts.size() != 3)
            return false;
        int numbers[3] {};
        for (int i = 0; i < 3; ++i)
        {
            const auto& p = parts[i];
            if (p.isEmpty() || p.length() > 9 || ! p.containsOnly ("0123456789") || (p.length() > 1 && p.startsWithChar ('0')))
                return false;
            numbers[i] = p.getIntValue();
        }

        juce::StringArray identifiers;
        if (pre.isNotEmpty())
        {
            identifiers = juce::StringArray::fromTokens (pre, ".", "");
            for (const auto& id : identifiers)
                if (id.isEmpty() || ! id.containsOnly ("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-"))
                    return false;
        }
        out = { numbers[0], numbers[1], numbers[2], identifiers };
        return true;
    }

    // -1, 0 or 1.
    inline int compareVersions (const Version& a, const Version& b)
    {
        for (auto [x, y] : { std::pair { a.major, b.major }, std::pair { a.minor, b.minor }, std::pair { a.patch, b.patch } })
            if (x != y)
                return x < y ? -1 : 1;

        if (a.prerelease.isEmpty() || b.prerelease.isEmpty())   // a final version is newer than any prerelease of it
            return a.prerelease.isEmpty() == b.prerelease.isEmpty() ? 0 : (a.prerelease.isEmpty() ? 1 : -1);

        for (int i = 0; i < juce::jmin (a.prerelease.size(), b.prerelease.size()); ++i)
        {
            const auto &x = a.prerelease[i], &y = b.prerelease[i];
            const bool xNumber = x.containsOnly ("0123456789"), yNumber = y.containsOnly ("0123456789");
            if (xNumber && yNumber)
            {
                const auto xv = x.getLargeIntValue(), yv = y.getLargeIntValue();
                if (xv != yv)
                    return xv < yv ? -1 : 1;
            }
            else if (xNumber != yNumber)
                return xNumber ? -1 : 1;   // numbers sort before words
            else if (const int c = x.compare (y); c != 0)
                return c < 0 ? -1 : 1;
        }
        if (a.prerelease.size() != b.prerelease.size())
            return a.prerelease.size() < b.prerelease.size() ? -1 : 1;
        return 0;
    }

    // True when 'latest' is a newer version than 'installed'. Anything unreadable is never "newer".
    inline bool isNewer (const juce::String& latest, const juce::String& installed)
    {
        Version l, i;
        return parseVersion (latest, l) && parseVersion (installed, i) && compareVersions (l, i) > 0;
    }

    //==============================================================================
    // Release channels. Every build today is a beta (version 0.x, or any prerelease); a build from 1.0.0 on is stable.
    // A beta build only ever looks at the beta channel and a stable build only at the stable one, so one never offers the other.
    enum class Channel { beta, stable };

    inline Channel channelFor (const Version& v) { return ! v.prerelease.isEmpty() || v.major == 0 ? Channel::beta : Channel::stable; }
    inline juce::String channelName (Channel c) { return c == Channel::beta ? "beta" : "stable"; }

    // "Songsmith Passive EQ" -> "passive-eq": the identifier the metadata is published under.
    inline juce::String pluginIdFor (const juce::String& productName)
    {
        auto name = productName.trim();
        if (name.startsWithIgnoreCase ("Songsmith "))
            name = name.substring (10);
        juce::String id;
        for (auto c : name.toLowerCase())
        {
            const bool word = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            if (word)
                id << c;
            else if (id.isNotEmpty() && ! id.endsWithChar ('-'))
                id << '-';
        }
        return id.trimCharactersAtEnd ("-");
    }

    inline juce::String metadataUrl (Channel channel, const juce::String& pluginId)
    {
        // The stable site does not exist yet; until it does a stable build's check simply finds nothing and stays quiet.
        const auto host = channel == Channel::beta ? "https://beta.songsmith.uk" : "https://songsmith.uk";
        return juce::String (host) + "/updates/" + pluginId + "/latest.json";
    }

    //==============================================================================
    // The published answer. Parsed strictly: the wrong plugin or channel, an unknown format, an unreadable version or a download
    // page that is not an https address on songsmith.uk is rejected, so a damaged or hijacked answer can never put a stranger's
    // link in front of a musician.
    struct Metadata
    {
        juce::String version, downloadPage;
    };

    inline bool isSongsmithPage (const juce::String& address)
    {
        if (address.length() > 300 || ! address.startsWith ("https://") || address.containsAnyOf (" \t\r\n\"'<>\\@"))
            return false;
        const auto host = address.fromFirstOccurrenceOf ("https://", false, false).upToFirstOccurrenceOf ("/", false, false).toLowerCase();
        return host == "songsmith.uk" || host.endsWith (".songsmith.uk");
    }

    inline bool parseMetadata (const juce::String& text, const juce::String& expectedPlugin, Channel expectedChannel, Metadata& out)
    {
        if (text.isEmpty() || text.length() > 16384)
            return false;
        const auto json = juce::JSON::parse (text);
        auto* object = json.getDynamicObject();
        if (object == nullptr)
            return false;

        if (! object->getProperty ("schema").isInt() || (int) object->getProperty ("schema") != 1)
            return false;
        if (object->getProperty ("plugin").toString() != expectedPlugin || object->getProperty ("channel").toString() != channelName (expectedChannel))
            return false;

        const auto version = object->getProperty ("version").toString();
        const auto page = object->getProperty ("downloadPage").toString();
        Version parsed;
        if (! parseVersion (version, parsed) || ! isSongsmithPage (page))
            return false;

        out = { version, page };
        return true;
    }

    //==============================================================================
    // When to look. 'now' and the times are milliseconds since 1970.
    inline constexpr juce::int64 hour = 3600 * 1000, day = 24 * hour;

    struct Remembered
    {
        juce::int64 checkedAt = 0;   // the last attempt
        bool succeeded = false;
        juce::String version, downloadPage;   // the last good answer
    };

    inline bool dueForCheck (const Remembered& r, juce::int64 now)
    {
        if (r.checkedAt == 0)
            return true;
        if (r.checkedAt > now + hour)   // a clock that went backwards: look again rather than wait for ever
            return true;
        return now - r.checkedAt >= (r.succeeded ? day : hour);
    }

    //==============================================================================
    // What the footer needs to show.
    struct Offer
    {
        bool available = false;
        juce::String version, downloadPage;
    };

    // The one place that fetches, shared by every plugin window in this program (juce::SharedResourcePointer) and, through a file,
    // by every Songsmith plugin on the computer. Everything here is called from the message thread except the fetch itself.
    class Checker final : private juce::Thread
    {
    public:
        Checker() : juce::Thread ("Songsmith update check") {}
        ~Checker() override { stopThread (requestTimeoutMs + 1500); }

        // Tells whoever is listening (the footer) that the answer changed. Safe to call from any thread.
        juce::ChangeBroadcaster changed;

        // Test executables and an environment setting switch the check off, so tests never touch the network.
        static bool enabled()
        {
            if (overrideEnabled().has_value())
                return *overrideEnabled();
            if (juce::SystemStats::getEnvironmentVariable ("SONGSMITH_UPDATE_CHECK", {}).equalsIgnoreCase ("off"))
                return false;
            return ! juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFileNameWithoutExtension().endsWithIgnoreCase ("Tests");
        }

        // Starts a check if one is due; otherwise the remembered answer is used. Returns at once.
        void check (const juce::String& productName, const juce::String& installedVersion)
        {
            Version installed;
            if (! parseVersion (installedVersion, installed))
                return;

            const auto id = pluginIdFor (productName);
            const auto channel = channelFor (installed);
            const auto key = id + "|" + channelName (channel);
            if (id.isEmpty())
                return;

            bool start = false;
            {
                const std::scoped_lock lock (mutex);
                auto remembered = rememberedFor (key);
                if (! dueForCheck (remembered, now()) || busy)
                    return;
                // Claim the attempt now, so several windows opening together make one request.
                remembered.checkedAt = now();
                remembered.succeeded = false;
                remember (key, remembered);
                request = { id, channel, key };
                busy = true;
                start = true;
            }
            if (start)
            {
                if (isThreadRunning())   // the previous look is only just finishing
                    waitForThreadToExit (500);
                startThread (juce::Thread::Priority::low);
            }
        }

        // What to show for this plugin now (from the remembered answer, whenever it was fetched).
        Offer offerFor (const juce::String& productName, const juce::String& installedVersion)
        {
            Version installed;
            if (! parseVersion (installedVersion, installed))
                return {};
            const auto key = pluginIdFor (productName) + "|" + channelName (channelFor (installed));
            const std::scoped_lock lock (mutex);
            const auto r = rememberedFor (key);
            if (r.version.isNotEmpty() && isNewer (r.version, installedVersion) && isSongsmithPage (r.downloadPage))
                return { true, r.version, r.downloadPage };
            return {};
        }

        //==============================================================================
        // Seams for the tests: the network, the clock, and where the answers are remembered.
        using Fetcher = std::function<juce::String (const juce::String& url)>;   // returns the body, or an empty string on any failure
        void setFetcherForTesting (Fetcher f)        { const std::scoped_lock lock (mutex); fetcher = std::move (f); }
        void setClockForTesting (std::function<juce::int64()> c) { const std::scoped_lock lock (mutex); clock = std::move (c); }
        void setCacheFileForTesting (const juce::File& f)        { const std::scoped_lock lock (mutex); cacheFile = f; memory.clear(); loaded = false; }
        static void setEnabledForTesting (std::optional<bool> on) { overrideEnabled() = on; }
        bool isBusy() const { const std::scoped_lock lock (mutex); return busy; }
        void waitUntilIdleForTesting (int timeoutMs)
        {
            const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
            while (isBusy() && juce::Time::getMillisecondCounter() < end)
                juce::Thread::sleep (5);
        }

        static constexpr int requestTimeoutMs = 4000;

    private:
        struct Request { juce::String id; Channel channel = Channel::beta; juce::String key; };

        static std::optional<bool>& overrideEnabled() { static std::optional<bool> v; return v; }

        juce::int64 now() const { return clock ? clock() : juce::Time::currentTimeMillis(); }

        static juce::File defaultCacheFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("Songsmith Studio").getChildFile ("update-check.json");
        }

        void run() override
        {
            Request r;
            Fetcher fetch;
            {
                const std::scoped_lock lock (mutex);
                r = request;
                fetch = fetcher;
            }

            const auto body = fetch ? fetch (metadataUrl (r.channel, r.id)) : fetchOverHttps (metadataUrl (r.channel, r.id), threadShouldExit());
            Metadata metadata;
            const bool ok = ! threadShouldExit() && parseMetadata (body, r.id, r.channel, metadata);

            {
                const std::scoped_lock lock (mutex);
                auto remembered = rememberedFor (r.key);
                remembered.checkedAt = now();
                remembered.succeeded = ok;
                if (ok)
                {
                    remembered.version = metadata.version;
                    remembered.downloadPage = metadata.downloadPage;
                }
                remember (r.key, remembered);
                busy = false;
            }
            changed.sendChangeMessage();
        }

        static juce::String fetchOverHttps (const juce::String& address, bool /*shouldStop*/)
        {
            // Plain GET, no cookies, no identifying headers. A timeout or any error gives an empty answer.
            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                               .withConnectionTimeoutMs (requestTimeoutMs)
                               .withNumRedirectsToFollow (2)
                               .withHttpRequestCmd ("GET");
            if (auto stream = juce::URL (address).createInputStream (options))
            {
                juce::MemoryOutputStream out;
                char buffer[2048];
                while (out.getDataSize() < 16384)
                {
                    const int n = stream->read (buffer, (int) sizeof (buffer));
                    if (n <= 0)
                        break;
                    out.write (buffer, (size_t) n);
                }
                return out.toString();
            }
            return {};
        }

        // The remembered answers: in memory, and in a small file so every Songsmith plugin on the computer shares them.
        // Caller holds the mutex.
        Remembered rememberedFor (const juce::String& key)
        {
            load();
            const auto it = memory.find (key.toStdString());
            return it != memory.end() ? it->second : Remembered {};
        }

        void remember (const juce::String& key, const Remembered& r)
        {
            load();
            memory[key.toStdString()] = r;
            save();
        }

        void load()
        {
            if (loaded)
                return;
            loaded = true;
            const auto file = cacheFile != juce::File() ? cacheFile : defaultCacheFile();
            if (! file.existsAsFile() || file.getSize() > 65536)
                return;
            const auto parsed = juce::JSON::parse (file);   // keep the value alive while its object is used
            auto* object = parsed.getDynamicObject();
            if (object == nullptr)
                return;
            for (auto& p : object->getProperties())
                if (auto* e = p.value.getDynamicObject())
                {
                    Remembered r;
                    r.checkedAt = (juce::int64) e->getProperty ("checkedAt");
                    r.succeeded = (bool) e->getProperty ("succeeded");
                    r.version = e->getProperty ("version").toString();
                    r.downloadPage = e->getProperty ("downloadPage").toString();
                    memory[p.name.toString().toStdString()] = r;
                }
        }

        void save()
        {
            const auto file = cacheFile != juce::File() ? cacheFile : defaultCacheFile();
            auto* all = new juce::DynamicObject();
            juce::var root (all);
            for (auto& [key, r] : memory)
            {
                auto* e = new juce::DynamicObject();
                e->setProperty ("checkedAt", r.checkedAt);
                e->setProperty ("succeeded", r.succeeded);
                e->setProperty ("version", r.version);
                e->setProperty ("downloadPage", r.downloadPage);
                all->setProperty (juce::Identifier (key), juce::var (e));
            }
            // Written through a temporary file so another plugin never reads half of it. A failure is ignored.
            if (file.getParentDirectory().createDirectory())
            {
                const auto temp = file.getSiblingFile (file.getFileName() + ".tmp");
                if (temp.replaceWithText (juce::JSON::toString (root)))
                    temp.moveFileTo (file);
            }
        }

        mutable std::mutex mutex;
        std::map<std::string, Remembered> memory;
        bool loaded = false, busy = false;
        Request request;
        Fetcher fetcher;
        std::function<juce::int64()> clock;
        juce::File cacheFile;
    };
}
