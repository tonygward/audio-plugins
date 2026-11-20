# Songsmith Plugin Analyzer

A VST3 plugin that **measures what another plugin does to the sound at each frequency**. Put one copy (the **Reference**) before the plugin you want to measure and one copy (the **Result**) after it. Press Start while your recording software plays, and the Result draws the measured gain in decibels against frequency, from 20 Hz to 20 kHz.

Use it to check an equaliser's curve, to compare a hardware-inspired plugin with a Songsmith plugin, or to keep a record of a curve you want to match. It is a measuring tool, not an effect: it leaves the sound untouched except while a measurement is running.

![The Result showing two measurements over each other, with the Songsmith Passive EQ curve in green and a second plugin in orange](docs/images/plugin-analyzer-curve.png)

*Real screenshots at 200 %, rendered by the test executable (`PluginAnalyzerTests --screenshot <file> --scale 2 --state curve|reference|problem`). The curves are made by test filters, not by a real plugin.*

## Using it

1. Put **Songsmith Plugin Analyzer** on the track **before** the plugin you want to measure. Set **Role** to **Reference**.
2. Put a second copy **after** that plugin. Set **Role** to **Result**.
3. Give both the same **Session**. Use a different session for each pair, so two measurements in one project never mix.
4. Turn off anything in the chain that changes with level (a compressor stage, saturation, an automatic gain), so you measure the equaliser curve alone.
5. Start playback in your recording software, then press **Start** on the Result. It takes about a quarter of a minute: two test sweeps, then the end of each.
6. Read the curve. Choose **Measure Into** to keep a measurement in one of four slots, **Name for the Next Measurement** to label it, and **Compare With** to draw a second one over it. **Export CSV...** saves the curves as a table.

While a measurement runs, the Reference replaces the sound on that track with the test sweep. At other times both copies pass the sound through untouched, and they add no delay.

![A Reference, before a measurement](docs/images/plugin-analyzer-reference.png)

![A Reference with nothing to pair with, and what to do about it](docs/images/plugin-analyzer-problem.png)

## The controls

| Control | What it does |
|---|---|
| **Role** | **Reference** sends the test sweep. **Result** records what arrives and draws the curve. |
| **Session** | One of eight. A Reference and a Result with the same session belong together. |
| **Channel** | Which channel the Result measures: Left, Right, or Both Combined. |
| **Test Level** | How loud the sweep is (−36 to −6 dB). Quieter is gentler on the plugin; louder keeps noise out of the curve. The default is −18 dB. |
| **Start**, **Stop**, **Reset** | Begin a measurement, end it early (nothing is kept), or empty the slot the next measurement would fill. |
| **Measure Into**, **Show**, **Compare With** | Four named slots. Choose which one the next measurement fills, which is drawn, and which is drawn over it. |
| **Range** | How many decibels the graph shows above and below 0 dB. |
| **Export CSV...** | Saves the curves as a table: frequency in hertz, then one column of decibels per measurement, with notes about the measurement at the top. |
| **Bypass** | Nothing is measured and the sound passes through untouched. |

The Output meter at the right shows the sweep leaving the Reference, and what arrives at the Result. If its clip warning lights during a measurement, lower **Test Level**.

## What it measures, and what it cannot

- It measures the **size of the response** (the gain at each frequency), not the phase. The curve is smoothed to a sixth of an octave.
- It lines up the Reference and the Result in time first, so a plugin's delay does not matter. It reports the delay it found.
- A **fixed equaliser** is measured accurately. In the tests, known filters (flat gain, high-pass, bell boost and cut, low shelf) read within 0.5 dB from 30 Hz to 18 kHz, at several block sizes and sample rates, and with noise at −70 dBFS in the path.
- A **compressor, saturation or anything that changes with level** is measured at the test level only. The curve is shown, but it is not a fixed curve for that plugin.
- It never claims what it cannot know. It says so, and shows no curve, when nothing arrived, the sweep arrived very quietly, the sound reached the top of the scale, the processing changed between the two sweeps, or the sound could not be lined up with the sweep.
- It does not match a hardware unit from knob positions, and it does not host other plugins itself.

## Limits

- Both copies must be loaded **in the same program**. Some recording software loads each plugin in its own separate process; there the Result never finds a Reference, and says so.
- It is a VST3 plugin only. It has no Audio Unit and no standalone app, because it needs two copies inside a host.
- It has been tested with a simulated host (a Reference and a Result processing blocks of 64 to 4,096 samples, with delay). **Not yet verified in a DAW.**

## Saving

The Role, Session, Channel, Test Level and Bypass settings, the slot names, and the measured curves are saved with your project. Slot curves are restored when you reopen it, so you can keep a measurement for later without exporting it.

## Building and testing

```powershell
cmake -S plugin-analyzer -B plugin-analyzer/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE
cmake --build plugin-analyzer/build --config Release
.\plugin-analyzer\build\PluginAnalyzerTests_artefacts\Release\PluginAnalyzerTests.exe
```

It is part of the Songsmith Plugins installers on Windows and macOS, as its own tick box. Its parameter IDs, plugin code (`Anl1`), manufacturer code and package identifiers must never change, so saved projects keep loading.
