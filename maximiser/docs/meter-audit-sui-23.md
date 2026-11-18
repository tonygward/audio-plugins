# Far-right meter audit (SUI-23)

Reference: the Songsmith Compressor's right-hand column (16 unit margin, shared Output meter 48 units wide at the far
edge, Gain Reduction bar 26 units wide in an 88 unit slot 14 units to its left, number below the bar).

| Plugin | Dynamics? | Output meter at far right | Gain Reduction beside it | Status |
|---|---|---|---|---|
| Songsmith Compressor | yes | yes (reference) | yes (reference) | Reference |
| Songsmith Maximiser | yes | yes | yes | Changed in SUI-23 |
| Songsmith EQ | no | yes, right of the graph | not applicable | Already compliant |
| Songsmith Passive EQ | no | yes, 20 units from the edge (reference uses 16) | not applicable | Compliant apart from a 4 unit margin difference |
| Songsmith Bus Compressor | yes | **no** | horizontal bar across the top, not a right-hand column | **Not done** |
| Songsmith Optical Leveller | yes | **no** | its own centre meter, not a right-hand column | **Not done** |
| Songsmith Analog Compressor | yes | **no** | needle meter in the centre of the faceplate | **Not done** |

## Maximiser change

- Layout: Input, Threshold, Link, Output Ceiling, Release and its settings, then the Integrated LUFS column, then the
  meter column. Design width 860 to 888 units (resize limits kept at 75 % to 300 %).
- The Output meter is now the shared `Songsmith::UI::OutputMeterDisplay` (Left and Right bars, CLIP, held peak). The
  processor feeds the shared `OutputMeterState` from the same block peaks it already measured; the audio itself is untouched.
- Gain Reduction keeps the Maximiser's `VerticalMeter` reduction mode, which draws the same as the Compressor's, and gains the
  Compressor's number readout.
- The separate "Output" caption is gone, as in the Compressor; the meter carries an accessible title and tooltip.

## Not yet verified

Nothing was built or run in this session (file tools only). No before/after screenshots exist yet. Not verified in a DAW.

## Remaining work

Bus Compressor, Optical Leveller and Analog Compressor each need a per-channel output meter state in the processor, a wider
design size, and a Compressor-style column in the editor. Their bespoke meters (horizontal bar, optical and needle faces)
are part of their faceplates, so each should be a separate follow-up task rather than a blind edit.
