# All Buttons (step 4)

All Buttons means pressing all four ratio buttons in at once ("British mode"). In the plugin it is its own switch (`allButtons`), so the selected ratio comes back when it is switched off. In the editor:

- **All** presses all four buttons, and all five light up.
- Pressing any single ratio button leaves All Buttons and selects that ratio.

Each press is one automation gesture.

## What the manual says (sourced)

UM p. 9 ("Front Panel", Ratio) and p. 22 ("About All-Button Mode"):

1. "distortion increases radically due to a lag time on the attack of initial transients (a phenomenon which might be described as a 'reverse look-ahead')"
2. "The ratio goes to somewhere between 12:1 and 20:1"
3. "the bias points change all over the circuit, thus changing the attack and release times as well". UM p. 9 adds "constant changes in the attack and release times, as well as a change in the bias points".
4. "the meter will go wild, often resting at maximum"
5. Users describe the perceived distortion as stronger at low frequencies (UM "Applications" section, drum room). This comes out of the model's fast gain modulation on low notes. It is not added separately.

No source gives curve data, timing data or distortion figures for this mode, so every number below is **tuned**.

## The model (tuned)

| Behaviour | Source | Implementation (`FETEngine.h`, `FETColour.h`, `NeedleMeter.h`) |
|---|---|---|
| Ratio 12:1-20:1 | (2) | Feedback gain `k = 17` (nominal 18:1), with a 4 dB soft knee. **Measured** over tones stepped in 2 dB steps from +10 to +22 dB above threshold: an effective **12-20:1** (tested), and no step looser than 8:1. |
| Bias shift | (3) | The threshold drops to -21 dBFS, below every single button. At -18, -12 and -6 dBFS input it gives more than 1 dB extra GR compared with 20:1 (tested). |
| The "constantly shifting compression curve" | (3) and the UM p. 22 text "The unique and constantly shifting compression curve" | **Over-compression:** above 24 dB of input overshoot, each extra dB of input adds 1.2 dB of GR, so very hot input comes out *quieter* (tested at +30 → +40 dB). |
| Attack lag on initial transients | (1) | Attack is up to 21x slower while GR is under about 4 dB: `x (1 + 20 e^(-GR/4))`. Once the GR is established it returns to the knob's time. On a synthetic drum hit (1 ms click + 80 Hz body), the transient sticks out **more than 2 dB further** above the compressed body than at 20:1 with the same knobs (tested). |
| Changing release | (3) | Release speeds up with depth: `x 1 / (1 + GR/12)`. Release from deep GR is faster than from shallow GR (tested). |
| Distortion "increases radically" | (1) | The FET distortion term is multiplied by 8, smoothed over 20 ms when switching. At a matched output level, THD is **more than 2x** that of 20:1 (tested). |
| The meter "goes wild, often resting at maximum" | (4) | In GR mode the needle reads `-(1.5 GR + 3)` while there is any GR, so it sits further left and pins at -20 sooner. |

Switching All Buttons or ratios during playback is click-free. GR is a continuous state, and only the curve and the coefficients change (tested on DC: largest sample step < 0.01).

## Listening checklist (level-matched)

1. **Drum room / overheads:** Input +18, attack 3, release 7. All vs 20:1, with Output matched by ear or on a meter. The hits crack through, then the room sucks in and blooms back. The low end gets dirtier.
2. **Bass DI:** All vs 4:1 at the same Input. The bass sounds dirtier and more forward, and each note's attack pokes out.
3. **Vocal:** All with the attack slow. Loud words get "in your face" and gritty. Listen for pumping on held notes. That's expected here, but it shouldn't click.
4. **Switching:** toggle All and the ratio buttons during playback: no clicks.
5. **Meter:** in GR mode the needle swings much further than with a single button, and rests near the left end on loud passages.
