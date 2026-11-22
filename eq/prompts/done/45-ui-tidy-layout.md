---
id: EQ-45
priority: medium
effort: medium
dependencies: []
conflicts: []
---

# EQ-45: a tidier EQ layout (Piano centred, Graph Range over the dB scale, Gain Scale removed)

Issue: songsmithstudio#17. Implemented by hand with Claude Code (no Matrix run). Merged in #360 and shipped in 0.5.76.

## Objective
Put each EQ control beside what it affects, and make the bottom rows read cleanly.

## Scope
- **Piano** toggle: bottom middle, centred in the shared bottom strip under the piano-note strip (every prompt and doc that said "far right" was updated).
- **Oversampling** and **Accurate Highs** reordered in the bottom row.
- **Graph Range** sits over the dB scale it changes.
- **Gain Scale** control removed from the interface. The parameter is kept as a hidden parameter, so sessions and automation keep loading (design principle 1: never remove a parameter).
- The shared Output panel (songsmithstudio#16, part 1) sits in the graph row, so the EQ's smallest window is 1160 wide and 480 + 28 high.

## Acceptance criteria
- Piano is centred under the piano-note strip at 75, 100 and 200 %.
- Graph Range sits over the dB scale, with no overlap at the smallest window.
- No Gain Scale control is visible; the Gain Scale parameter still exists and older sessions load unchanged.
- `EQTests` pass (14822 checks).

## Build and release

Songsmith EQ only. Released as 0.5.76 with exactly the two installers. Not yet verified in a DAW.
