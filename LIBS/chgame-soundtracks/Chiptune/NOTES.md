# ATM Soundtracks implementation

This repository contains the soundtrack sketch and a pinned upstream CHGame
submodule. It does not change the shared audio library or use saves.
The debug handshake is `CHIP 1.1`; `H` reports track, paused state, volume,
mute mask and elapsed tracker ticks.

## Sources and port

ATMlib is pinned at `8fdabd2fd81082bff50e8af730cae4feff2cce99` from
<https://github.com/T-arg/ATMlib>. `vendor/ATMlib/` preserves its unmodified
source and BSD license. The generated port in `src/atm/` retains its command
interpreter, call stack, effects and play/pause behavior. AVR register setup
and assembly mixing are replaced by the sketch's CH32X035 backend.

Port-specific changes:

- Explicit 16-bit song offsets (`word` is 32 bits in CHGame's Arduino core).
- Bytewise unaligned offset reads; signed effect values on RISC-V.
- Fix upstream's `read_vle` pointer increment and byte effect parameter reads.
- Fix dangling-else volume clamps and bound frequency-table accesses.
- Recompute note increments for A440 at 31,250 samples/s. ATMlib's original
  table is specific to its AVR timer setup and would play out of tune here.
- Triangle replaces ATMlib's saw on the bass channel to approximate NES bass.
- Disable the unused SFX API; stack depth four suffices for these songs.

The song headers use four entry tracks and four self-looping voice tracks,
followed by a dictionary of repeated command-aligned phrases. Dictionary
phrases contain no nested calls: entry -> voice -> phrase is the deepest
call path. This keeps the tracker stack bounded independently of loop count.
Each voice lasts precisely as many ticks as the melody, including partial
final bars. Every repeat returns to the same phase in the arrangement.
Mario excludes the reference transcription's separate game-over coda.
The first three backing parts are arranged by bar in `tools/make_songs.py`.
The six additions import actual MIDI note intervals for supporting voices,
bass and percussion. `tools/import_classics.py` records the source URL,
SHA-256, sequencer, track selection, tempo and normalized lanes. Its companion
`tools/midi_reference.py` reads metrical Standard MIDI files without an added
Python dependency. To reproduce imports, download the six filenames named in
the importer's `REFERENCES` list into a directory, then run:

```sh
python3 Chiptune/tools/import_classics.py PATH_TO_REFERENCE_MIDIS
python3 Chiptune/tools/make_songs.py
```

Normalization uses 24 ticks per quarter, preserves rests, reduces polyphonic
parts to one note per voice, and shifts notes by octaves into C2..D7.
Where a MIDI arrangement changes instruments, selected source tracks are
combined; the most recent onset takes priority, then the specified upper or
lower chord note. Wily uses the tempo covering the greatest source duration
(153 BPM); all other imported references have constant tempo. Pitch bends,
sustain pedal and controller automation are not reproduced. Pokémon has no
noise part in the reference, so no invented percussion is added.
The references and
composer credits are retained in `tools/melodies.json` and `NOTICE`.

## Reproduce and verify

```sh
python3 Chiptune/tools/port_atmlib.py
python3 Chiptune/tools/make_songs.py
.venv/bin/python Chiptune/tools/verify_audio.py
.venv/bin/python Chiptune/tools/verify_player.py
python3 chiptune.py gif
python3 chiptune.py build
python3 chiptune.py export --no-build
python3 chiptune.py cart verify Chiptune/build/Chiptune.chgame
```

`verify_audio.py` compiles the actual C++ tracker/mixer with undefined-behavior
sanitization (also AddressSanitizer outside macOS). It writes complete mono
8-bit WAVs at 31,250 Hz into `out/audio/`: `0-full.wav` through `8-full.wav`,
plus melody-only versions. It checks every melody note and gate against
the editable transcriptions over three loops, every imported supporting lane,
tuning, PCM clipping, pause,
resume, silent volume, and multi-byte long-delay decoding. The first Tetris
E5 is also measured from actual PCM pulse edges, independently of the table.

`verify_player.py` operates the real sketch through simulator button input:
track wrap, frozen pause clock, resume, persistent melody mode, both volume
limits and restart. The visual script records the README GIF and exercises
all tracks. Simulator checks include the shared panel's torn-frame detection.

All six WAVs of the original three selections (full and lead only) remained
SHA-256 identical after phrase compression. The nine packed scores total
26,185 bytes. The expanded release build measured 38,920 bytes of image and
12,220 bytes static RAM (2,048-byte stack reserved separately). Rebuilds print
current sizes. Physical sound quality, timer deadlines under display/USB load,
interrupt latency and piezo output have not been measured on a board. WAVs
verify the digital engine, not the speaker's acoustics.

Version 1.1.0 was uploaded to CHGame rev0 on 2026-10-25: 38,920 bytes,
CRC32 `0xAB4DAA56`. The bootloader accepted the image as valid, received RUN,
and the uploader confirmed application USB enumeration. This verifies upload
and startup, not a physical listening comparison.
