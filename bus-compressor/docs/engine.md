# Songsmith Bus Compressor: engine reference

What the engine does, which parts follow published hardware behaviour, which are Songsmith design choices, and the test that checks each. The engine is `Source/BusEngine.h`; the tests are in `Tests/EngineTests.cpp`.

## Hardware reference

The reference is the stereo bus compressor on the SSL 4000 G series console, as described in the manufacturers' published material and reviews (for example [Universal Audio's SSL 4000 G Bus Compressor manual](https://help.uaudio.com/hc/en-us/articles/30847649785748-SSL-4000-G-Bus-Compressor-Manual) and [Solid State Logic's 500 series G Comp user guide](https://www.solidstatelogic.com/assets/uploads/downloads/SSL_500_Series_G_Comp_Module_User_Guide.pdf)). Songsmith Bus Compressor is an original design, not a copy, and is not affiliated with or endorsed by Solid State Logic.

**Source status.** The Universal Audio page refused automated access when this was written, so the step values below come from search-result summaries of those documents, not from reading them in full. Published figures also differ from one SSL product to another (some units have different release times and threshold ranges). Check them against the manual of the unit you care about before relying on any of them.

## Published behaviour used as targets

| Behaviour | Value | Test |
|---|---|---|
| Ratio steps | 2:1, 4:1, 10:1 | `The published steps` |
| Attack steps | 0.1, 0.3, 1, 3, 10, 30 ms | `The published steps` |
| Release steps | 0.1, 0.3, 0.6, 1.2 s, and Auto | `The published steps` |
| Stereo linked, one gain for both channels | yes | `Stereo linking` |
| Feed-forward gain reduction with a gain reduction meter | yes | `The meter reading` |

## Songsmith design choices

These are not measurements of any unit. Each is chosen to behave sensibly and is checked by a test.

| Choice | Value | Test |
|---|---|---|
| The Threshold dial's span: -15 to +10, with 0 at -18 dBFS (the suite's reference level) | -33 to -8 dBFS | `The static curve` |
| Make-up Gain range | 0 to 15 dB | `Make-up Gain adds exactly its amount` |
| Detector | the louder channel's peak, rising at once and letting go over 10 ms | `A steady signal settles on the static curve` |
| Knee | a smooth 4 dB knee centred on the threshold | `The static curve` |
| Attack and release | a smooth approach in decibels. The time is how long the reduction takes to cover 63 % of the way | `Attack`, `Release` |
| Auto release | quick (0.1 s) after a short hit; slows towards 1.2 s as the compressor has been working harder for longer (a held reduction of 6 dB or more is the slowest) | `Auto release` |
| Latency | none, and no look-ahead, so a very fast peak can pass before the attack acts, as on the hardware | `Identity` (host latency) |
| Sidechain filter | not modelled | none |
| Colour from the console's circuits | not modelled | none |

## Detector, stereo linking and release in one place

- **Detector:** one detector for both channels. It follows the louder channel's peak, so the stereo image holds. A damaged sample (not a number or infinite) is ignored by the detector.
- **Stereo linking:** fully linked. Both channels always get the same gain. A test feeds a loud left and a quiet right channel and checks the balance between them does not change.
- **Release:** a fixed release lets the reduction go in the stated time. Auto chooses the time from how hard and how long the compressor has been working, using a slow memory of the reduction (1.5 s).

## Other checks

- Block-size independence, so the host's buffer size never changes the sound.
- Every setting combination with damaged input stays finite and goes silent in silence.
- Make-up Gain changes are smoothed over 20 ms.
- Every factory starting point is level-matched at a peak level of -12 dBFS (within 1 dB).
