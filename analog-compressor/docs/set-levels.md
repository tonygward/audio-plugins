# Set Levels (ACOMP-09)

An optional guided calibration. It proposes an **Input** for a few decibels of gain reduction on the louder peaks and an **Output** that keeps the compressed level close to the original. It is a Songsmith addition; the 1176 has nothing like it.

## What the musician does

1. Press **Set Levels** (under the Output knob, beside Input, which it also sets).
2. Play a loud, typical part of the song for 3 to 12 seconds. The button reads **Finish Listening**; capture also stops by itself at 12 seconds.
3. Press **Finish Listening**. The plugin shows the proposed Input and Output and the gain reduction measured on the louder peaks.
4. Press **Apply** or **Cancel**. Apply writes Input and Output as one Undo step. Doing nothing is fine: nothing changes until Apply.

## How it measures

- **Audio thread (`FET::Calibration::Capture`)**: while listening, `processBlock` copies the incoming sound (before Input drive) into a buffer allocated in `prepareToPlay` (12 seconds, two channels). No allocation, lock, logging or file access, and it never reads or writes a parameter. The write position is published with release/acquire ordering, so the message thread only ever reads samples that are finished.
- **Message thread (`FET::Calibration::analyse`)**: replays the capture through a private `FET::Engine` set to the ratio, All Buttons, Attack, Release and Ignore Below in use, followed by the same FET colour and output stage as the plugin. It bisects Input (14 steps, −24 to +48 dB) until the **90th percentile of the 100 ms peak gain reductions** is 3 dB (accepted range 2 to 4 dB). "Louder peaks" therefore means the loudest tenth of the capture: a few drum hits in a quiet passage count, one stray spike does not.
- **Output**: the difference between the dry loudness and the compressed loudness (RMS over the capture, loudest channel, with Input applied and Output at 0 dB), so the level-matched sound has the same RMS as the dry sound.
- Oversampling is not replayed: it changes the top end slightly, not the gain reduction or level. This is a design choice, not a published behaviour.

## Safeguards

| Situation | What happens |
|---|---|
| Silence (RMS below −80 dBFS) | "No sound was heard"; nothing to apply |
| Under 3 seconds | "Too short to judge"; nothing to apply |
| Target not reachable (Input at its limit) | Input is set to the limit, the shortfall is shown, Apply is optional |
| Output would need to go beyond its range | Output is clamped, the message says the level cannot be fully matched |
| Matched peak above −0.1 dBFS | The message adds a clipping warning; Output is not lowered automatically, because that would break the level match |
| Settings (Input, Output, ratio, All Buttons, Attack, Release, Ignore Below) change while listening, or between finishing and Apply | No proposal or nothing applied, with a message. This protects against host automation and the musician's own changes: Set Levels never writes over a newer value |
| Cancel, closing the window | Capture stops; no parameter is touched |

Input and Output are never changed during listening or normal playback, so a calibration cannot fight host automation. Apply uses a complete change gesture per parameter, so a host in write mode records it.

## Attack and Release tooltips

Attack: "how quickly compression starts when the sound gets louder. Turn clockwise to make it faster". Release: "how quickly compression lets go when the sound gets quieter. Turn clockwise to make it faster". The dial runs clockwise from 1 to 7 and `Engine::attackSeconds` / `releaseSeconds` fall from 800 µs to 20 µs and from 1.1 s to 50 ms, so the wording matches the DSP (tests: `Set Levels editor`).

## Tests (`CalibrationUnitTests.h`, `CalibrationEditorTests.h`)

- Sustained and transient sources land at 1.5 to 4.5 dB of measured reduction; the processor's own gain reduction agrees with the prediction within 1 dB.
- **Level-matched bypass comparison**: the proposal is applied, the same sound is played with and without bypass, and the RMS levels agree within 1.5 dB (sustained) and 2.5 dB (transients).
- Ratio and Ignore Below change the proposal; silence, short and too-quiet captures, clipping flags; capture stops when full and when told to; parameters stay fixed while listening.
- Editor: position under Output and clear of neighbours, apply is one Undo step, Cancel changes nothing, changed settings are not overwritten, wording and message length.

## Not yet verified

Not tested in a DAW (Studio One, Logic): behaviour when the transport stops while listening (the editor then simply keeps waiting until Finish Listening or Cancel is pressed; Finish with no audio reports "No sound was heard") and with host automation in write mode. Test tolerances above were chosen before a first hosted build and may need adjusting. No before-and-after screenshot was rendered because this run could not build.
