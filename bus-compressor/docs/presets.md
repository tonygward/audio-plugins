# Songsmith Bus Compressor: where the starting points come from

The five factory starting points, written down before the plugin is called release-ready. They are Songsmith starting points, not copies of anyone's settings. Names follow the suite rule: each is named for the job it does, and keeps hardware names, tutorial titles and educators' names out of the title.

## Sources used

- Joe Gilder's bus-compression tutorials, which the prompt names as the starting point. **They have not been watched.** The only account reached was a [ProSoundWeb summary of his mix bus video](https://www.prosoundweb.com/a-different-approach-to-mix-bus-compression/), second-hand and from search results: a fairly standard mix bus starting point of about 2:1, 30 ms attack and 100 ms release, with a little make-up gain, and release treated as a "tone knob". Mix Bus Glue uses those three settings. Any further change from the tutorials themselves is still to be recorded here, with the source.
- [Waves' guide to the SSL bus compressor](https://www.waves.com/how-to-use-ssl-bus-compressor-classic-mix-glue): set the ratio first, lower the threshold slowly until reduction starts, and up to about 4 dB of reduction is usually enough. The starting points stay at or below about 6 dB.
- [Nail The Mix, SSL bus compressor settings](https://www.nailthemix.com/?p=9125): one engineer's rock mix settings of attack about 10 ms, release 0.3 s and 4:1. This informs Drum Bus Punch only loosely, and the vocal group and finished mix settings are Songsmith's own choices from general practice.

## The settings

Make-up Gain matches the reduction at a peak level of -12 dBFS, so the starting points do not simply sound louder.

| Starting point | Threshold | Ratio | Attack | Release | Make-up Gain | Why |
|---|---|---|---|---|---|---|
| Mix Bus Glue | +2 | 2:1 | 30 ms | 0.1 s | +2.0 dB | about 2 dB of reduction, the second-hand Gilder starting point |
| Finished Mix Gentle Glue | +4 | 2:1 | 30 ms | Auto | +1.0 dB | barely moves, for a mix that is already finished |
| Drum Bus Punch | 0 | 4:1 | 30 ms | 0.3 s | +4.5 dB | a slow attack lets each hit through before the compressor acts |
| Drum Bus Heavy Control | -1 | 10:1 | 10 ms | 0.1 s | +6.0 dB | firm control of very uneven drums |
| Vocal Group Glue | 0 | 2:1 | 3 ms | 0.3 s | +3.0 dB | blends backing vocals |

## What the tests check

- Every starting point sets every sound parameter, leaves Bypass alone, and is a host program.
- Each is level-matched within 1 dB at a peak of -12 dBFS and does between 0.5 and 8 dB of reduction there (engine tests).
- Names are short, in Title Case, and free of hardware and educator names; each has a listening cue that stands on its own.
- Every word on screen, including the listening cues, passes the plain-words check.

## Before this is release-ready

- [ ] Load the plugin in a real host (VST3 in a DAW, Audio Unit in Logic Pro) and check that it opens, resizes, saves and reloads a session, and responds to automation.
- [ ] Listen on a mix bus and a drum bus with the plugin switched in and out at the same listening level, and write short listening notes here. Timing and gain reduction are measured by automated tests; the listening is not done.
- [ ] Watch Joe Gilder's bus-compression tutorials and record any change to a starting point here, with the source.
- [ ] Check the step values and ranges in [engine.md](engine.md) against the manual of the SSL unit you want to follow, and decide whether a sidechain filter or Auto release behaviour should change.
