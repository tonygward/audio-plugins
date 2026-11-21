---
id: ANALYZER-02
priority: medium
effort: high
dependencies: [ANALYZER-01]
conflicts: []
---

**Tracking issue:** https://github.com/tonygward/songsmithstudio/issues/5

# ANALYZER-02: Start-driven capture and automatic waveform comparison

The Plugin Analyzer's Session, Measure Into, Show, Compare With and Reference/Result controls made a plain A/B measurement confusing. Make **Start** the entry point and compare automatically.

## What was done

- **Start is the entry point.** On the After Plugin, Start with no capture runs the sweep and saves **Capture 1** (renameable). With a capture, Start asks (in a panel over the graph) whether to **Replace "<name>"** or **Add to Compare**; with two captures it asks which one to replace. Cancel and Escape change nothing, and a capture is only replaced when the new measurement finishes and has a curve (a failed measurement keeps the old capture and says why).
- **Automatic overlay.** Both curves are drawn on one graph, the first solid blue and the second dashed orange (a colour-blind-safe pair that does not rely on colour alone), each with a **Show** switch and a name under the graph. Hiding a capture never deletes it.
- **Roles renamed** Before Plugin and After Plugin in every word the musician sees. The parameter IDs, ranges and saved values are unchanged.
- **No Session dropdown.** A new plugin takes the first free pair and, when its role is changed, joins the other half waiting alone, so pairs form without a choice. An opened project keeps its saved pairing. A **Pair With** box (Pair 1 to Pair 8) appears only when the pairing is ambiguous.
- **Removed** Measure Into, Show, Compare With and Name for the Next Measurement. Kept: Export CSV, Test Level, Range, Channel, the notes and the status. There are two captures (it was four slots); the saved-state version is 2, and a version 1 project opens with its first two filled slots as the captures.

## Tests

The pairing rule and its end-to-end behaviour, the Pair With box showing only when needed, the Start flow (first capture, replace, add, cancel, two captures, nothing replaced early), a failed measurement not replacing a capture, both curves drawn with distinct colours and line styles, show and hide, names, Clear All, the old wording and controls gone, version 1 and damaged saved states, and text fit, plain words and keyboard access at three window sizes.

## Limits to keep honest

- Not verified in a DAW, and not tried in Studio One with a UAD plugin in the chain (the issue's own acceptance scenario).
- Pairing is automatic only when both plugins load in the same program; some hosts load each in its own process.
- Two captures to compare, not more.
- The Test Level is still set on the Before Plugin, which sends the sweep.
