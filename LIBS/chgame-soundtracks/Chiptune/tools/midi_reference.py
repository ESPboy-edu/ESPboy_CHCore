#!/usr/bin/env python3
"""Small, dependency-free Standard MIDI File reader for score transcriptions.

Returns note intervals in source ticks, grouped by track and channel. Sustain
pedal and pitch bend are not interpreted: imported tracks target chip voices.
"""
from collections import defaultdict
from pathlib import Path
import struct


def read_midi(path):
    data = Path(path).read_bytes()
    assert data[:4] == b'MThd', 'Expected a Standard MIDI File'
    size = int.from_bytes(data[4:8], 'big')
    fmt, count, ppq = struct.unpack('>HHH', data[8:14])
    assert fmt in (0, 1) and not ppq & 0x8000, 'Only metrical MIDI formats 0/1 are supported'
    cursor, tracks, tempos = 8 + size, [], []
    for index in range(count):
        assert data[cursor:cursor+4] == b'MTrk'
        size = int.from_bytes(data[cursor+4:cursor+8], 'big')
        buf = data[cursor+8:cursor+8+size]
        cursor += 8 + size
        pos = tick = running = 0
        name, notes, active = '', defaultdict(list), defaultdict(list)

        def vle():
            nonlocal pos
            value = 0
            while True:
                byte = buf[pos]
                pos += 1
                value = value * 128 + (byte & 127)
                if byte < 128:
                    return value

        while pos < len(buf):
            tick += vle()
            status = buf[pos]
            if status >= 128:
                pos += 1
                if status < 240:
                    running = status
            else:
                status = running
            assert status, 'Missing MIDI running status'
            if status == 255:
                kind = buf[pos]
                pos += 1
                length = vle()
                payload = buf[pos:pos+length]
                pos += length
                if kind == 3:
                    name = payload.decode('latin1')
                elif kind == 81:
                    tempos.append((tick, int.from_bytes(payload, 'big')))
                continue
            if status in (240, 247):
                length = vle()
                pos += length
                running = 0
                continue
            kind, channel = status & 240, status & 15
            length = 1 if kind in (192, 208) else 2
            args = buf[pos:pos+length]
            pos += length
            if kind == 144 and args[1]:
                active[channel, args[0]].append((tick, args[1]))
            elif kind == 128 or kind == 144:
                if active[channel, args[0]]:
                    start, velocity = active[channel, args[0]].pop(0)
                    if tick > start:
                        notes[channel].append([start, tick, args[0], velocity])
        for (channel, pitch), pending in active.items():
            for start, velocity in pending:
                if tick > start:
                    notes[channel].append([start, tick, pitch, velocity])
        tracks.append(dict(index=index, name=name, notes={k: sorted(v) for k, v in notes.items()}, end=tick))
    return dict(ppq=ppq, tracks=tracks, tempos=sorted(set(tempos)))


if __name__ == '__main__':
    import sys
    for path in sys.argv[1:]:
        midi = read_midi(path)
        print(Path(path).name, 'PPQ', midi['ppq'], 'tempos', midi['tempos'][:12])
        for track in midi['tracks']:
            for channel, notes in track['notes'].items():
                print(' ', track['index'], 'ch', channel, repr(track['name']), len(notes),
                      'range', min(n[2] for n in notes), max(n[2] for n in notes),
                      'end beats', round(max(n[1] for n in notes) / midi['ppq'], 2))
