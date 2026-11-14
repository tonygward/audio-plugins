---
id: MAX-10
priority: medium
effort: low
dependencies: []
conflicts: []
---

# MAX-10: Smoother attack and a 100 ms default Release

Issue: [#273](https://github.com/tonygward/audio-plugins/issues/273). Implemented by hand from the DSP review (no Matrix run).

## Objective
Cleaner limiting in Songsmith Maximiser: less distortion on hard limiting and on sustained bass, without losing the brickwall guarantee or changing the latency.

## Scope
- **Attack shape:** replace the single box-car mean after the sliding minimum with two cascaded box-cars (lengths L1 + L2 − 2 = L). The gain moves in an S-curve inside the same look-ahead instead of a straight ramp with corners.
- **Default Release:** 1 ms → 100 ms for new instances (the current L1 manual's default, as quoted; the legacy manual's 1 ms suggestion gave about 17 % distortion on sustained bass). Saved sessions and starting points keep their own Release.

## Acceptance criteria
- No output sample ever exceeds Output Ceiling (all existing ceiling, impulse, automation and stress tests pass).
- Latency unchanged at every sample rate.
- The attack is monotonic, starts and finishes at under half the slope of a straight ramp, and lands exactly on the required gain on time.
- New instances default to 100 ms; parameter IDs, ranges and saved sessions are unchanged.

## Tests
- New: "MAX-10: the attack is an S-curve inside the look-ahead and still lands on time" at 44.1, 48 and 96 kHz.
- Updated: "Ranges and defaults" expects 100 ms.

## Build and release

Songsmith Maximiser only. Users will notice slightly cleaner loud peaks and a different default Release, so the changelog has a note.
