"""Generate a small valid Standard MIDI File and byte-level mutations."""

from __future__ import annotations

import random
import struct


def vlq(value: int) -> bytes:
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(out))


def make_midi() -> bytes:
    track = bytearray()
    track += vlq(0) + bytes([0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])  # 120 bpm
    track += vlq(0) + bytes([0xFF, 0x03, 0x05]) + b"Piano"
    for note in (60, 62, 64, 65, 67, 69, 71, 72):
        track += vlq(0) + bytes([0x90, note, 100])
        track += vlq(96) + bytes([0x80, note, 0])
    track += vlq(0) + bytes([0xFF, 0x2F, 0])
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
    return header + b"MTrk" + struct.pack(">I", len(track)) + bytes(track)


def mutate(data: bytes, rng: random.Random) -> bytes:
    buf = bytearray(data)
    for _ in range(rng.randint(1, 6)):
        op = rng.randrange(6)
        if op == 0 and len(buf) > 16:
            buf = buf[: rng.randint(1, len(buf) - 1)]
        elif op == 1 and len(buf) > 16:
            pos = rng.randrange(len(buf))
            buf[pos] ^= 1 << rng.randrange(8)
        elif op == 2 and len(buf) > 16:
            start = rng.randrange(len(buf))
            end = min(len(buf), start + rng.randint(1, 64))
            del buf[start:end]
        elif op == 3:
            pos = rng.randrange(len(buf) + 1)
            buf[pos:pos] = bytes(rng.randrange(256) for _ in range(rng.randint(1, 32)))
        elif op == 4 and len(buf) > 16:
            start = rng.randrange(len(buf))
            end = min(len(buf), start + rng.randint(1, 64))
            pos = rng.randrange(len(buf) + 1)
            buf[pos:pos] = buf[start:end]
        else:
            for needle in (b"MThd", b"MTrk", b"RIFF", b"RMID"):
                pos = buf.find(needle)
                if pos >= 0:
                    buf[pos : pos + 4] = bytes(rng.randrange(256) for _ in range(4))
                    break
    return bytes(buf)
