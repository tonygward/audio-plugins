---
id: PEQ-06
priority: medium
effort: medium
dependencies: []
conflicts: []
---

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/330

# PEQ-06: Group Passive EQ presets by category in the dropdown

Group the Songsmith Passive EQ's factory starting points under clear headings in the existing title-bar preset menu, so a musician can find the right one quickly without a new control.

## What was done

- Groups (headings that cannot be chosen, and that the arrow keys skip): **Drums**, **Bass**, **Guitar**, **Vocals**, **Mix Bus and Mastering**, **Utility**, then **Your presets** when there are any. All 19 starting points are classified, each in one group only.
- The grouping is a table of names (`Presets::categories()`), so a starting point keeps its index, name and menu item number: saved sessions, user presets, the host's programs and Undo are unaffected. An empty group is not shown.
- The classic Pultec punch (a Low Boost and a Low Cut together at 60 Hz) is already the **Punchy Kick** starting point; no duplicate under another name was added, and the preset-name rule keeps hardware names out of titles.
- Choices: "Mix Bus and Mastering" is written with *and* (plain words, no symbols); the telephone sound is under Vocals and the soft-sounding part under Utility.

## Tests

Group membership and order, exactly-one-group for every starting point, empty groups dropped, the menu's structure (headings are not choices, item numbers unchanged, your presets after the groups), choosing one from each mode switches the mode and is one Undo step, a session saved and reopened, arrow keys skipping headings, and plain wording of the headings.

## Limits to keep honest

- Not verified in a real host or on a Mac. The open menu is a pop-up window, so there is no screenshot of it, only a text outline in the README.
- The host's program number does not follow a choice made from the menu (it follows only the host's own program changes). That was already how it worked and is unchanged.
