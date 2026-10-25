#!/usr/bin/env python3
"""Exercise actual player controls through the shared simulator protocol."""
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'CHGame/tools/chsim'))
from chsim import build
from chdrivelib import Driver, SimTransport, mask_of
count = len(json.loads((ROOT / 'tools/melodies.json').read_text()))

t = SimTransport(build(str(ROOT)))
d = Driver(t, 'CHIP')
try:
    d.handshake()
    d.cmd('L1')
    d.frames(2)

    def tap(key):
        d.buttons(mask_of(key))
        d.frames(3)
        d.buttons(0)
        d.frames(1)

    def state():
        return list(map(int, d.query('H', 'CHIPSTATE').split()[1:]))

    assert state()[:4] == [0, 0, 3, 0]
    tap('LEFT')
    assert state()[0] == count - 1
    tap('RIGHT')
    assert state()[0] == 0
    for expected_track in range(1, count):
        tap('RIGHT')
        assert state()[0] == expected_track
    tap('RIGHT')
    assert state()[0] == 0
    tap('A')
    paused = state()
    assert paused[1] == 1
    d.frames(120)
    assert state() == paused
    tap('A')
    d.frames(10)
    assert state()[1] == 0 and state()[4] > paused[4]
    tap('B')
    assert state()[3] == 14
    tap('RIGHT')
    assert state()[0] == 1 and state()[3] == 14
    tap('B')
    assert state()[3] == 0
    for _ in range(6):
        tap('DOWN')
    assert state()[2] == 0
    for _ in range(6):
        tap('UP')
    assert state()[2] == 4
    before = state()[4]
    tap('START')
    assert state()[4] < before
finally:
    t.close()
assert not t.bugs, t.bugs
print('PASS: track wrap, pause clock, resume, melody mode persistence, volume bounds, restart; no torn frames')
