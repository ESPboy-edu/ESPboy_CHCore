#!/usr/bin/env python3
"""Render the real engine; check transcription timing, pitch, looping and silence."""
from pathlib import Path
import json
import os
import platform
import subprocess
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out/audio"
OUT.mkdir(parents=True, exist_ok=True)
exe = OUT / 'render'
sanitizer = 'undefined' if platform.system() == 'Darwin' else 'address,undefined'
subprocess.run(['clang++', '-std=c++17', '-O1', '-g', f'-fsanitize={sanitizer}',
                '-fno-omit-frame-pointer', f'-I{ROOT}', str(ROOT / 'tools/render.cpp'),
                str(ROOT / 'Synth.cpp'), str(ROOT / 'src/Songs.cpp'), '-o', str(exe)], check=True)
subprocess.run([str(exe), str(OUT)], check=True)
scores = json.loads((ROOT / 'tools/melodies.json').read_text())
for i, score in enumerate(scores):
    expected = []
    for midi, ticks in score['melody']:
        hold = max(1, round(ticks * score.get('gate', 0.96 if i == 2 else 0.9)))
        expected += [(midi - 35 if midi else 0, bool(midi) and j < hold) for j in range(ticks)]
    trace = np.loadtxt(OUT / f'{i}.tsv', dtype=int)
    assert len(trace) == 3 * len(expected)
    for t, note, volume, increment in trace[:, :4]:
        enote, sounding = expected[t % len(expected)]
        assert note == enote and bool(volume) == sounding, (score['title'], t, note, volume, enote, sounding)
        if sounding:
            hz = 440 * 2 ** ((note + 35 - 69) / 12)
            cents = 1200 * np.log2(increment * 31250 / 65536 / hz)
            assert abs(cents) < 7, (score['title'], t, cents)
    if 'lanes' in score:
        for ch, lane in enumerate(score['lanes']):
            expected_lane = []
            for midi, duration in lane:
                expected_lane += [(midi - 35 if midi else 0, bool(midi))] * duration
            for row in trace:
                t = row[0]
                note, vol, inc = row[1+ch*3:4+ch*3]
                enote, sounding = expected_lane[t % len(expected_lane)]
                assert note == enote and bool(vol) == sounding, (score['id'], ch, t, note, vol, enote)
    with wave.open(str(OUT / f'{i}-full.wav')) as wav:
        audio = np.frombuffer(wav.readframes(wav.getnframes()), dtype=np.uint8).astype(float) - 128
        assert wav.getframerate() == 31250 and np.std(audio) > 10
        assert audio.min() > -128 and audio.max() < 127
    # Independently measure first sustained E5 pulse (Tetris) in rendered PCM.
    if i == 0:
        with wave.open(str(OUT / '0-lead.wav')) as wav:
            pcm = np.frombuffer(wav.readframes(3125), dtype=np.uint8).astype(int)
        edges = np.flatnonzero((pcm[1:] < 128) & (pcm[:-1] >= 128))
        measured = 31250 / np.mean(np.diff(edges))
        assert abs(measured - 659.255) < 2, measured
    print(f'PASS {score["title"]}: notes and gates across 3 loops, A440 tuning, non-clipping PCM')
print(OUT)
