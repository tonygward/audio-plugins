---
id: ANA-01
priority: medium
effort: medium
dependencies: []
conflicts: []
---

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/334

# ANA-01: Use the official Songsmith logo for the Audio Analyzer app and Start menu icon

The Songsmith Audio Analyzer had no icon of its own, so Windows showed a plain placeholder for the exe, the Start menu entry and the taskbar. Use the existing official Songsmith logo everywhere.

## What was found

- `audio-analyzer/CMakeLists.txt` passed no `ICON_BIG`, so JUCE built the exe with no Songsmith icon.
- The installer's Start menu shortcut (`{autoprograms}\Songsmith Audio Analyzer`) names no icon file, which is right: Windows then takes the icon from the exe. The only fix needed was to put the logo in the exe.
- The official logo is `public/images/songsmith-logo.png` in the website repository (the wordmark with the S mark). No other icon asset existed in this repository.

## What to do

- Crop the S mark square from that logo (no redesign) into `audio-analyzer/Resources/songsmith-icon.png` at 1024 by 1024.
- Pass it as `ICON_BIG`. JUCE embeds it in the Windows exe at several sizes and makes the macOS icon.
- Keep the product name, bundle ID and app identity unchanged.
- Installer tests (Windows, macOS) check the embedded icon and that the shortcut takes its icon from the app.
- Changelog under `## Unreleased`, plain words, with the Windows icon-cache note.

## Done when

- The installed exe, the Start menu shortcut and the taskbar show the logo on Windows, and the macOS app shows it in Finder and the Dock.
- Fresh-install, upgrade and uninstall installer tests pass.
- Not verified: a person looking at the real Start menu or Dock. Windows may keep showing a saved icon until the icon cache refreshes.
