# Test fixtures: real MP3 files of a known signal

Four short MP3 files used by `AudioAnalyzerTests` ("real encoded files of a known signal"). They are synthetic: a 997 Hz sine at -20 dBFS (amplitude 0.1) in both channels, 44.1 kHz, 4 seconds. They contain no one else's music, so there is nothing to license.

| File | Encoding | ffmpeg's EBU R128 reading of the decoded file |
|---|---|---|
| `sine-997hz-minus20db-cbr128.mp3` | constant 128 kilobits per second | -20.5 LUFS, true peak -20.4 dBFS |
| `sine-997hz-minus20db-cbr192.mp3` | constant 192 kilobits per second | -20.3 LUFS, true peak -20.3 dBFS |
| `sine-997hz-minus20db-cbr320.mp3` | constant 320 kilobits per second | -20.0 LUFS, true peak -20.0 dBFS |
| `sine-997hz-minus20db-vbr.mp3` | variable bitrate (LAME quality 2), averages about 34 kilobits per second | -20.0 LUFS, true peak -19.9 dBFS |

Before encoding the signal reads -20.0 LUFS and -20.0 dBTP (EBU Tech 3341, the level case). A lossy file is not bit for bit the same, and the lower bitrates decode a little under their source, so each file's reference is what an independent decoder and meter read for the decoded file: **ffmpeg 9.1** (`libmp3lame`), `-af ebur128=peak=true`, the integrated loudness and true peak from its final summary, rounded to a tenth of a decibel. The tests require the Audio Analyzer to agree within 0.15 dB. (The app's own readings are, for example, -20.450 LUFS and -20.421 dBTP for the 128 file.)

## How they were made

```
SRC="aevalsrc=0.1*sin(2*PI*997*t)|0.1*sin(2*PI*997*t):s=44100:d=4"
ffmpeg -f lavfi -i "$SRC" -c:a libmp3lame -b:a 128k sine-997hz-minus20db-cbr128.mp3     # and 192k, 320k
ffmpeg -f lavfi -i "$SRC" -c:a libmp3lame -q:a 2    sine-997hz-minus20db-vbr.mp3
ffmpeg -i <file>.mp3 -af ebur128=peak=true -f null -                                   # the reference reading
```

Use `aevalsrc`, not `sine`: ffmpeg's `sine` source starts at -18 dBFS, and turning a mono source into stereo takes another 3 dB off (the first attempt at these files was -41 dBFS for that reason).

The files are about 340 KB in total. Do not replace them without updating the references above.
