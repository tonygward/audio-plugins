# DAW checks: Studio One and Logic (step 5)

These checks need a real host. Before release, run them in **Studio One** (VST3, Windows and macOS) and **Logic Pro** (AU, macOS). Record the host version, the OS and pass or fail for each line in the release PR.

Load the plugin from an installer build: the CI artifacts `Songsmith-Analog-Compressor-Windows-setup` and `Songsmith-Analog-Compressor-macOS-pkg`.

## Executed automatically (CI and AnalogCompressorTests, every push)

| Check | Where |
|---|---|
| VST3 + Standalone build (Windows x64); VST3 + AU + Standalone build (macOS universal) | CI `analog-compressor` job |
| `auval -v aufx Sfc1 Tnyw` on the built component, and again on the component the `.pkg` installed | CI macOS job |
| Mono/mono and stereo/stereo accepted; mismatched and surround layouts rejected; both process | AnalogCompressorTests "Bus layouts" |
| Every parameter round-trips through the saved state; missing or out-of-range values fall back or clamp; v1 sessions load | AnalogCompressorTests "State", "Oversampling and latency" |
| Interface size, meter mode and preset name are recalled | AnalogCompressorTests "Editor (vector)", "Presets" |
| Bypass is bit-exact (or delayed by exactly the reported latency) and cross-fades without combing | AnalogCompressorTests "Default transparency", "Oversampling and latency" |
| Reported latency matches the signal (0/4/6 samples) | AnalogCompressorTests "Oversampling and latency" |
| Automation gestures: one begin/end per button press and per preset parameter change | AnalogCompressorTests "Editor (vector)", "Presets" |
| Variable block sizes give identical output (offline render vs realtime buffer sizes) | AnalogCompressorTests "Engine behaviour" |
| Several instances are independent (each test builds its own processor; no shared state except the read-only fonts) | AnalogCompressorTests (design) |
| Isolated install, upgrade and uninstall on Windows and macOS; Songsmith EQ and both plugins' presets untouched | `analog-compressor/installer/test-installer.ps1`, `analog-compressor/installer/macos/test-installer.sh` |

## Pending: by hand in each host

These are **not** executed by CI. They're listed per host, to tick off.

| # | Check | Studio One (Win) | Studio One (Mac) | Logic (AU) |
|---|---|---|---|---|
| 1 | **Scan:** listed under Songsmith Studio, not blocklisted. Logic's Plug-in Manager shows it as "successfully validated". | ☐ | ☐ | ☐ |
| 2 | **Mono insertion:** on a mono track it opens, passes audio and compresses | ☐ | ☐ | ☐ |
| 3 | **Stereo insertion:** on a stereo track, the image stays centred while compressing (linked) | ☐ | ☐ | ☐ |
| 4 | **Automation:** write Input, Ratio, All Buttons and GR Off moves. They play back, and the editor follows them | ☐ | ☐ | ☐ |
| 5 | **Recall:** set everything away from the defaults, plus size 150 %, meter +8 and a starting point. Save, close, reopen: everything comes back | ☐ | ☐ | ☐ |
| 6 | **Bypass:** the host's bypass and the plugin's Bypass stay in sync. No click. With oversampling on, delay compensation keeps the track aligned with a duplicate | ☐ | ☐ | ☐ |
| 7 | **Offline render:** export a mixdown with heavy compression. It sounds the same as realtime playback (null test against a realtime bounce, if the host offers one) | ☐ | ☐ | ☐ |
| 8 | **Multiple instances:** 8+ instances on different tracks with different settings. Each keeps its own settings and meter, and the CPU is reasonable (~2.5 % each at 4x) | ☐ | ☐ | ☐ |
| 9 | **Presets:** the host's preset/program list shows the 4 starting points. Saving a user preset from the editor and reloading it works | ☐ | ☐ | ☐ |
| 10 | **Coexistence:** with Songsmith EQ installed too, both scan and both load in the same song | ☐ | ☐ | ☐ |
| 11 | **Low Cut:** write automation of Low Cut between Off and 100 Hz on a kick or bass track. It plays back, the editor follows, there are no clicks, and the Gain Reduction meter moves while the bass you hear stays the same. Save, close, reopen: the setting returns | ☐ | ☐ | ☐ |

Also by hand: the listening notes in [detector-low-cut.md](detector-low-cut.md), the listening checklists in [engine.md](engine.md), [colour.md](colour.md) and [all-buttons.md](all-buttons.md), and the editor checks in [smoke-test.md](smoke-test.md).
