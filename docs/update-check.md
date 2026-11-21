# Update check (songsmithstudio#8)

Every Songsmith plugin tells you when a newer build is published. The shared code is `shared/Source/SongsmithUpdateCheck.h`; the footer's **Update available** link is in `shared/Source/SongsmithDiagnostics.h`; the tests are `shared/Tests/SongsmithUpdateTests.h`, built into every plugin's test executable.

## What a musician sees

When a window opens, a small **Update available** link appears beside the version number at the bottom left, only if a newer build exists. Clicking it opens the download page in the default browser. Nothing is installed by it, and nothing is shown when you are up to date, offline, or the check fails.

## How it works

- **Where:** `https://beta.songsmith.uk/updates/<plugin>/latest.json` (beta builds; a stable build would use `https://songsmith.uk/...`, which does not exist yet, so it finds nothing and stays quiet). `<plugin>` is the product name without "Songsmith", in lower case with hyphens (`analog-desk`, `passive-eq`).
- **What it holds:** `schema`, `plugin`, `channel`, `version`, `date`, `downloadPage` and `notesPage`. No installer names, hashes or release-note text. It is served by the website's Worker from the same verified release record as the download page, so it only changes after the release's installers were verified and published; a failed release leaves the previous answer in place. See `songsmith-website`, `src/updates.mjs`.
- **When:** at most once every 24 hours for each plugin and channel, and a failed look is tried again after one hour. The answer is remembered in `Songsmith Studio/update-check.json` in the user's application data folder, so every window, every instance and every Songsmith plugin on the computer share one look.
- **Where it runs:** on its own low-priority thread, never the audio thread and never the message thread. The request has a four-second timeout. The window never waits for it.
- **Versions:** compared as numbers (0.5.10 is newer than 0.5.9) with the usual prerelease rules (1.0.0-beta.2 is older than 1.0.0). A build with major version 0, or any prerelease, is a **beta** build and asks only the beta channel; 1.0.0 and later is **stable** and asks only the stable channel. An answer for the wrong plugin or channel is refused.
- **Safety:** an answer is accepted only if its download page is an `https` address on `songsmith.uk`. Offline, a timeout, a missing entry, an HTML login page or a damaged answer are ignored without a dialog.
- **Off switch:** set the environment variable `SONGSMITH_UPDATE_CHECK=off`. Test executables never make the request.

## One manual setup step

Cloudflare Access must allow the path `beta.songsmith.uk/updates/*` without sign-in (a Bypass policy on the Access application). The Worker answers it without a sign-in, but Access sits in front of the Worker, and a plugin inside a recording program cannot sign in. Installers, the download page and every other path stay behind Access.
