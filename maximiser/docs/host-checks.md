# Songsmith Maximiser: host checks

These checks need a person with a DAW. CI covers what it can: build, unit and stress tests, auval, and install / upgrade / uninstall on Windows and macOS. Tick each box and note the host version and OS.

Before you start:
- Install from the CI artifacts:
  - Windows: `Songsmith-Maximiser-Windows-setup`
  - macOS: `Songsmith-Maximiser-macOS-pkg`
- Open the editor. It should say **Songsmith Maximiser** at the top left.
- For audio checks, use the **Final export** starting point unless a step says otherwise.

## Studio One (Windows and macOS, VST3)

| # | Check | How | Expected |
|---|---|---|---|
| 1 | Scan | Rescan plug-ins (Options → Locations → VST Plug-ins → Reset Blocklist, then restart) | Listed under Songsmith Studio as **Songsmith Maximiser**, next to Songsmith EQ, not replacing it |
| 2 | Stereo | Insert on a stereo track and play | Audio passes; Atten meter moves when you lower Threshold |
| 3 | Mono | Insert on a mono track | Mono in / mono out, no channel doubling or silence |
| 4 | Automation | Write automation for Threshold, Output Ceiling and Release, and toggle Peak Mode and Auto Release | Plays back smoothly with no clicks. With **Link** on, automating Threshold doesn't move Output Ceiling |
| 5 | Recall | Set non-default values (Link on, window resized, Dither 16-bit), save, close and reopen the song | Everything comes back, including Link and the window size |
| 6 | Offline render | Export a mixdown with the maximiser on the main bus | The rendered file matches real-time playback, and its peak is ≤ Output Ceiling (check with Studio One's or another true-peak meter) |
| 7 | Latency compensation | Duplicate a track, then put the maximiser (at defaults, Threshold 0) on one copy, polarity-invert the other and sum | Near-silence. The latency is 79 samples at 44.1 kHz and 84 at 48 kHz, and is the same in both peak modes |
| 8 | Multiple instances | Insert 8 or more instances on different tracks and play | No dropouts, each editor independent, CPU reasonable |
| 9 | Bypass | Use Studio One's bypass, and the plug-in's own Bypass button, during playback | No clicks and no timing jump (bypass is delay-compensated) |
| 10 | Starting points | Load each starting point from the menu | Values change, Dither stays off, Bypass and Link stay as they were. Undo in Studio One reverts each parameter |

## Logic Pro (macOS, AU)

| # | Check | How | Expected |
|---|---|---|---|
| 1 | Scan | Open Logic, then Plug-in Manager | **Songsmith Maximiser** passes validation (AU `aufx Mxlm Tnyw`) |
| 2 | Stereo and mono | Insert on stereo and mono channel strips | Both layouts offered and working |
| 3 | Automation | Read and Touch automation on Threshold, Output Ceiling, Release, Peak Mode, Auto Release and Dither | Smooth, no clicks |
| 4 | Recall | Save the project, quit Logic, reopen | All settings, Link and the window size restored |
| 5 | Offline render | Bounce (offline) with the maximiser on Stereo Out | The bounce matches real time; peak ≤ ceiling on Logic's True Peak meter in True peak mode |
| 6 | Latency compensation | Same null test as Studio One #7 | Near-silence |
| 7 | Multiple instances | 8 or more instances | No dropouts |
| 8 | Window size | Resize the editor, close it and reopen | Same size, everything scaled, nothing clipped |

## Listening and loudness checks

- **Level-matched A/B:** Threshold sets the gain, so match levels before you judge the sound. With Link on, move Threshold and Output Ceiling down together until bypass and active are equally loud, then compare.
- **Pumping and distortion:** on a dense mix, compare manual Release 1 ms, manual 10 ms and Auto Release. Listen for pumping, and for bass distortion on sustained notes.
- **True-peak reading:** in True peak mode with Output Ceiling at −1 dB, a true-peak meter on the output (Studio One's, Logic's Loudness Meter, or Youlean) should read at most −1.0 dBTP. The worst case measured in the tests is +0.28 dB, on full-band white noise. Real mixes should stay within 0.1 dB.
- **Loudness meter (needs a person with a DAW; not yet verified in any host):** play a finished mix through the maximiser from the start, in real time, and compare its **Integrated** reading at the end with a reference meter on the same output (the DAW's own loudness meter, or a separate meter such as Youlean). The two should agree within about 0.5 LUFS; note the host, its meter and both readings. Also check that:
  - pressing play from a stop starts a new measurement, and pausing then resuming does not;
  - **Reset** and a click on the Integrated number start again;
  - the readings keep counting with the editor closed, and are not saved with the session;
  - an offline bounce does not need the meter (it only measures real-time playback, so its reading after a fast offline render is not a file analysis).
- **Dither:** with Dither 16-bit and a fade-out exported to 16 bits, the fade should decay into steady, hiss-like noise, with no grainy truncation distortion.

## Comparison with a Waves L1 (not done: no L1 available)

No Waves L1 instance was available on the build machines, so this comparison hasn't been run. The procedure, for anyone who has one:

1. Use the same source, the same settings (Threshold −6 dB, Output Ceiling −1 dB, Release 1 ms, then 5 ms), the L1's IDR off, and Songsmith in Sample peak mode with Auto Release off.
2. Render both and level-match by integrated loudness (LUFS) before listening.
3. Compare four things:
   - **Transient handling:** look at the attack shape on drum hits. Songsmith ramps the gain linearly over its 1.5 ms look-ahead.
   - **Ceiling:** sample peak and true peak of each render.
   - **Gain reduction:** the maximum and average attenuation.
   - **Latency:** reported in samples. Songsmith reports look-ahead + 12 samples.
4. Repeat with Songsmith's True peak and Auto Release, against the UltraMaximizer's True Peak and ARC.
