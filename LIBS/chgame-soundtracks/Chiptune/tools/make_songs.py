#!/usr/bin/env python3
"""Compile attributed melody transcriptions plus four-part arrangements to ATM."""
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCORES = json.loads((ROOT / "tools/melodies.json").read_text())
# Chord roots are MIDI notes; bass and offbeat chords follow the score's key.
# These supporting parts are CHGame arrangements, not original console dumps.
HARMONY = [
    [(45, 3), (45, 3), (52, 4), (45, 3), (50, 3), (45, 3), (52, 4), (45, 3),
     (45, 3), (52, 4), (45, 3), (52, 4), (45, 3), (52, 4), (45, 3), (52, 4)],
    [(48, 4), (55, 4), (48, 4), (53, 4), (48, 4), (55, 4), (48, 4), (53, 4),
     (48, 4), (55, 4), (48, 4), (53, 4), (48, 4), (55, 4), (48, 4), (55, 4)],
    [(46, 4), (44, 4), (42, 4), (41, 4), (41, 4), (41, 4),
     (46, 4), (44, 4), (42, 4), (41, 4), (39, 3), (37, 4), (36, 4), (41, 4),
     (46, 4), (44, 4), (42, 4), (41, 4), (42, 4), (41, 4), (42, 4), (41, 4),
     (39, 3), (37, 4), (36, 4), (41, 4)],
]

def delay(ticks):
    result = []
    while ticks:
        step = min(64, ticks)
        result.append(159 + step)
        ticks -= step
    return result

def event(midi, ticks, volume, gate=0.90):
    assert ticks > 0 and (midi == 0 or 36 <= midi <= 98)
    if not midi:
        return [64, 0, 0] + delay(ticks)
    hold = max(1, round(ticks * gate))
    return [64, volume, midi - 35] + delay(hold) + ([64, 0] + delay(ticks - hold) if ticks > hold else [])

def pack_tracks(parts, compress=True):
    """Replace repeated command-aligned phrases with flat ATM subroutine calls.

    Dictionaries contain only literal commands; they never call each other.
    Entry -> voice -> phrase is therefore the deepest stack, even after hours
    of playback. The uncompressed route supplies an independent test oracle.
    """
    lanes = []
    for part in parts:
        tokens, i = [], 0
        while i < len(part):
            size = 2 if part[i] == 64 else 1
            tokens.append(tuple(part[i:i+size]))
            i += size
        lanes.append(tokens)
    dictionary = []
    if compress:
        for _ in range(100):
            counts = Counter()
            # Calls split literal runs, so phrases never contain another call.
            for lane in lanes:
                for start in range(len(lane)):
                    for width in (4, 6, 8, 12, 16, 24, 32):
                        phrase = tuple(lane[start:start+width])
                        if len(phrase) != width or any(t[0] == 252 for t in phrase):
                            continue
                        counts[phrase] += 1
            candidates = []
            for phrase, count in counts.items():
                size = sum(map(len, phrase))
                saving = count * (size - 2) - size - 3
                if saving > 0:
                    candidates.append((saving, phrase))
            if not candidates:
                break
            best = None
            for _, phrase in sorted(candidates, reverse=True)[:32]:
                hits = 0
                for lane in lanes:
                    at = 0
                    while at <= len(lane) - len(phrase):
                        if tuple(lane[at:at+len(phrase)]) == phrase:
                            hits += 1
                            at += len(phrase)
                        else:
                            at += 1
                size = sum(map(len, phrase))
                saving = hits * (size - 2) - size - 3
                if saving > 0 and (best is None or saving > best[0]):
                    best = saving, phrase
            if best is None:
                break
            _, phrase = best
            target = 8 + len(dictionary)
            dictionary.append([b for token in phrase for b in token] + [254])
            for k, lane in enumerate(lanes):
                replaced, at = [], 0
                while at < len(lane):
                    if tuple(lane[at:at+len(phrase)]) == phrase:
                        replaced.append((252, target))
                        at += len(phrase)
                    else:
                        replaced.append(lane[at])
                        at += 1
                lanes[k] = replaced
    tracks = [[252, 4 + i, 159] for i in range(4)]
    tracks += [[b for token in lane for b in token] + [252, 4+i] for i, lane in enumerate(lanes)]
    tracks += dictionary
    offsets, base = [], 0
    for part in tracks:
        offsets += [base & 255, base >> 8]
        base += len(part)
    assert len(tracks) < 256 and base < 65536
    return [len(tracks)] + offsets + [0, 1, 2, 3] + sum(tracks, [])


def compile_song(index, score, compress=True):
    length = sum(t for _, t in score['melody'])
    if 'lanes' in score:
        assert len(score['lanes']) == 4
        parts = []
        for ch, lane in enumerate(score['lanes']):
            assert sum(t for _, t in lane) == length
            parts.append(sum((event(n, t, [38, 15, 30, 28][ch], score['gate']) for n, t in lane), []))
        return pack_tracks(parts, compress), length
    lead = []
    for midi, ticks in score['melody']:
        lead += event(midi, ticks, 38, 0.9 if index != 2 else 0.96)
    harmony, bass, drums = [], [], []
    # Each accompaniment lane has exactly the melody's tick count, including
    # incomplete final bars. This makes every subsequent loop stay aligned.
    for at in range(0, length, 12):
        span = min(12, length - at)
        root, third = HARMONY[index][(at // 96) % len(HARMONY[index])]
        beat = (at // 24) % 4
        offbeat = (at // 12) & 1
        bassnote = root if beat % 2 == 0 else root + 7
        bass += event(bassnote - 12 if bassnote >= 48 else bassnote, span, 30, 0.82)
        harmony += event(root + (third if beat % 2 == 0 else 7) if offbeat else 0,
                         span, 15, 0.60)
        # Short hats alternate with a longer backbeat. ATMlib halves noise vol.
        drums += event(60, min(span, 2 if offbeat else 4), 16 if offbeat else 28, 1)
        if span > (2 if offbeat else 4):
            drums += event(0, span - (2 if offbeat else 4), 0)
    return pack_tracks([lead, harmony, bass, drums], compress), length

header = ['// Generated by tools/make_songs.py; edit tools/melodies.json for notes.',
          '#pragma once', '#include <stdint.h>',
          'struct Song { const uint8_t *data; const char *title, *system, *composer, *game; uint16_t bpm, length; };',
          f'static constexpr unsigned SONG_COUNT = {len(SCORES)};',
          'extern const Song SONGS[SONG_COUNT];', '']
out = ['// Generated by tools/make_songs.py.', '#include "Songs.h"']
info = []
for i, score in enumerate(SCORES):
    data, ticks = compile_song(i, score, compress='--uncompressed' not in sys.argv)
    out.append(f'static const uint8_t SCORE_{i}[] = {{')
    out += ['  ' + ','.join(map(str, data[j:j+24])) + ',' for j in range(0, len(data), 24)]
    out.append('};')
    info.append((score, ticks, len(data)))
out.append('const Song SONGS[SONG_COUNT] = {')
for i, (s, ticks, size) in enumerate(info):
    out.append(f'  {{ SCORE_{i}, "{s["title"]}", "{s["system"]}", "{s["composer"]}", "{s.get("game", s["system"])}", {s["bpm"]}, {ticks} }},')
out += ['};', '']
(ROOT / 'src/Songs.h').write_text('\n'.join(header))
(ROOT / 'src/Songs.cpp').write_text('\n'.join(out))
for s, ticks, size in info:
    print(f'{s["title"]}: {size} bytes, {ticks} ticks, {ticks * 60 / (s["bpm"] * 24):.1f} seconds')
