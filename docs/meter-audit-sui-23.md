# Far-right meter audit (SUI-23)

Reference: the Songsmith Compressor's right-hand column. Every plugin now uses `shared/Source/SongsmithMeterColumn.h`
(`Songsmith::UI::MeterColumn`), which has the Compressor's geometry: 16 unit margin, the shared Output meter 48 units wide at
the far edge, and, for a plugin that turns the level down, the Gain Reduction bar 26 units wide in an 88 unit slot 14 units to
its left, with its number below. `test/suite-meters.test.mjs` finds every plugin folder and fails if one has no Output meter, or a
dynamics plugin has no Gain Reduction meter, so a new plugin cannot skip it.

| Plugin | Dynamics? | Output meter at far right | Gain Reduction beside it | Status after SUI-23 |
|---|---|---|---|---|
| Songsmith Compressor | yes | yes (the reference) | yes (the reference) | Unchanged |
| Songsmith Maximiser | yes | yes | yes | Moved to the far right (design width 860 to 888) |
| Songsmith EQ | no | yes, right of the graph | not applicable | Already compliant, unchanged |
| Songsmith Passive EQ | no | yes | not applicable | Margin 20 to 16 units, as the others |
| Songsmith Bus Compressor | yes | yes (new) | yes (new) | Column added (design width 800 to 980) |
| Songsmith Optical Leveller | yes | yes (new) | yes (new) | Column added (design width 760 to 940) |
| Songsmith Plugin Analyzer | no (a measuring tool that never changes the level) | yes | not applicable | Built with the column (ANALYZER-01) |
| Songsmith Analog Compressor | yes | yes (new) | yes (new) | Column added (design width 960 to 1140) |

## Choices

- **Existing faceplate meters stay.** The Bus Compressor's lamp row, the Optical Leveller's needle and the Analog Compressor's needle
  are part of each plugin's character and keep their own modes (the needles also show Output). The far-right column is the same
  in every plugin so a musician always finds Output and Gain Reduction in one place.
- **No reduction that is not applied.** The Analog Compressor's engine keeps following the sound while bypassed (ACOMP-18) and its
  needle still does. The far-right bar reads 0 while Bypass is on, because nothing is being turned down. The Optical Leveller and Bus
  Compressor already report 0 in Bypass.
- **Equalisers show Output only.** A test checks that they have no Gain Reduction meter.
- **Real values.** Output is the per-channel sample peak of what leaves the plugin, published from the audio thread without locks
  or allocation (the same method as the Songsmith EQ and Passive EQ). It is not true peak and the tooltips say so.
- Window sizes: each design width is wider by the column and its gap, still resizable from 75 % to 300 % (the Analog Compressor
  keeps its existing 100 % to 200 % range).
- DSP, parameters, presets and saved state are unchanged.

## Not verified

Not tried in a real DAW. No listening or host-resize check yet.
