#!/usr/bin/env python3
"""Normalize six attributed VGMusic references into editable four-voice scores.

Run with a directory containing the named MIDI files. Downloads stay outside
the source tree; only normalized note/rhythm facts and provenance are saved.
"""
from pathlib import Path
from collections import Counter
import hashlib
import json
import re
import sys
from midi_reference import read_midi

ROOT = Path(__file__).resolve().parents[1]
# Each lane supplies source track indices and the surviving register when a
# MIDI part contains chords. The two pulse voices split chords where useful.
REFERENCES = [
    dict(id='wily', title='DR WILY STAGE 1', game='MEGA MAN 2', system='NES 1988',
         composer='TAKASHI TATEISHI', file='Mega_Man_2_Dr_Wiley.mid', kind='nes',
         sequencer='Uncredited in VGMusic catalogue', lanes=[([1], 'high'), ([2], 'low'), ([3], 'low'), ([4], 'drums')]),
    dict(id='moon', title='THE MOON', game='DUCKTALES', system='NES 1989',
         composer='HIROSHIGE TONOMURA', file='dctmoon.mid', kind='nes',
         sequencer='DurfarC', lanes=[([1], 'high'), ([2], 'high'), ([3], 'low'), ([4], 'drums')]),
    dict(id='vampire', title='VAMPIRE KILLER', game='CASTLEVANIA', system='NES 1986',
         composer='K. YAMASHITA / S. TERASHIMA', file='CV1-_Vampire_Killer.mid', kind='nes',
         sequencer='Jorge D. Fuentes', lanes=[([1], 'high'), ([2], 'high'), ([3], 'low'), ([6], 'drums')]),
    dict(id='brinstar', title='BRINSTAR', game='METROID', system='NES 1986',
         composer='HIROKAZU TANAKA', file='Metroid_-_Brinstar.mid', kind='nes',
         sequencer='Jay Reichard', lanes=[([3], 'high'), ([2], 'high'), ([1], 'low'), ([4], 'drums')]),
    dict(id='greens', title='GREEN GREENS', game="KIRBYS DREAM LAND", system='GAME BOY 1992',
         composer='JUN ISHIKAWA', file='Greengreens.mid', kind='gameboy',
         sequencer='Heat Man', lanes=[([2, 3, 5], 'high'), ([2, 4, 7], 'low'), ([1], 'low'), ([6], 'drums')]),
    dict(id='trainer', title='TRAINER BATTLE', game='POKEMON RED / BLUE', system='GAME BOY 1996',
         composer='JUNICHI MASUDA', file='PkmRB-Battle2.mid', kind='gameboy',
         sequencer='Joao "Johnnyz" Buaes', lanes=[([2], 'high'), ([1], 'high'), ([3], 'low'), ([], 'drums')]),
]


def lane_events(notes, length, ppq, mode):
    """Quantize actual note endpoints, then reduce chords to one chip voice."""
    cells = [[] for _ in range(length)]
    for start, end, pitch, velocity in notes:
        a = round(start * 24 / ppq)
        b = min(length, max(a + 1, round(end * 24 / ppq)))
        if mode != 'drums':
            while pitch < 36:
                pitch += 12
            while pitch > 98:
                pitch -= 12
        else:
            # Retain percussion hit timing; chip noise has no melodic pitch.
            pitch = 60
            b = min(b, a + (4 if velocity >= 100 else 2))
        for tick in range(a, b):
            cells[tick].append((pitch, start, velocity))
    result = []
    for choices in cells:
        # Latest onset avoids an old sustained note swallowing a new phrase.
        if choices:
            latest = max(c[1] for c in choices)
            pitches = [c[0] for c in choices if c[1] == latest]
            pitch = min(pitches) if mode == 'low' else max(pitches)
        else:
            pitch = 0
        if result and result[-1][0] == pitch:
            result[-1][1] += 1
        else:
            result.append([pitch, 1])
    return result


def main(folder):
    path = ROOT / 'tools/melodies.json'
    scores = json.loads(path.read_text())[:3]
    for ref in REFERENCES:
        source = Path(folder) / ref['file']
        midi = read_midi(source)
        length = round(max(n[1] for t in midi['tracks'] for ns in t['notes'].values() for n in ns) * 24 / midi['ppq'])
        # Use the tempo covering the most source ticks (Wily has tiny repeated
        # tempo messages; the other five references use one constant tempo).
        coverage = Counter()
        tempos = midi['tempos'] or [(0, 500000)]
        for i, (at, tempo) in enumerate(tempos):
            end = tempos[i+1][0] if i+1 < len(tempos) else max(t['end'] for t in midi['tracks'])
            coverage[tempo] += end - at
        bpm = round(60000000 / coverage.most_common(1)[0][0])
        lanes = []
        for indices, mode in ref['lanes']:
            notes = [n for index in indices for ns in midi['tracks'][index]['notes'].values() for n in ns]
            lanes.append(lane_events(notes, length, midi['ppq'], mode))
        record = {k: ref[k] for k in ('id', 'title', 'game', 'system', 'composer', 'sequencer')}
        record.update(bpm=bpm, source=f'https://www.vgmusic.com/music/console/nintendo/{ref["kind"]}/{ref["file"]}',
                      sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                      source_ppq=midi['ppq'], source_lanes=ref['lanes'],
                      gate=1.0, melody=lanes[0], lanes=lanes)
        scores.append(record)
        print(ref['title'], bpm, 'BPM,', length, 'ticks, events', [len(l) for l in lanes])
    # Keep each note/duration pair on one line for readable score diffs.
    formatted = json.dumps(scores, indent=2)
    formatted = re.sub(r'\[\s+(\d+),\s+(\d+)\s+\]', r'[\1, \2]', formatted)
    path.write_text(formatted + '\n')


if __name__ == '__main__':
    main(sys.argv[1])
