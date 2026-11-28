#!/usr/bin/env python3
"""Fuzz MIDI/RIFF import through the real GUI import path."""

from __future__ import annotations

import argparse
import json
import os
import random
import signal
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

STRESS_ROOT = Path(__file__).resolve().parent
REPO_ROOT = STRESS_ROOT.parents[1]
sys.path.insert(0, str(STRESS_ROOT))

from runner import SANITIZER_MARKERS  # noqa: E402

CONFIG = """<?xml version="1.0"?>
<lmms version="1.3.0-alpha.2.266" configversion="3">
  <app configured="1"/>
  <audioengine audiodev="Dummy (no sound output)" mididev="Dummy"/>
  <paths defaultsf2="/tmp/lmms-stress-dummy.sf2"/>
</lmms>
"""


def vlq(n: int) -> bytes:
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


def make_midi() -> bytes:
    track = bytearray()
    track += vlq(0) + bytes([0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])  # tempo 120
    track += vlq(0) + bytes([0xFF, 0x03, 0x05]) + b"Piano"
    for i, note in enumerate([60, 62, 64, 65, 67, 69, 71, 72]):
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


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--cases", type=int, default=100)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=12)
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    run_id = args.run_id or time.strftime("midi-%Y%m%d-%H%M%S")
    run_dir = Path("/tmp/opencode/lmms-stress/runs") / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    failures_dir = STRESS_ROOT / "failures" / run_id
    failures_dir.mkdir(parents=True, exist_ok=True)
    metrics = (run_dir / "fuzz.jsonl").open("w", encoding="utf-8")

    base = make_midi()
    rng = random.Random(args.seed_start)
    failures = 0
    started = time.monotonic()
    for i in range(args.cases):
        case_dir = Path(tempfile.mkdtemp(prefix=f"midi-{i:06d}-", dir=run_dir))
        data = base if i == 0 else mutate(base, rng)
        midi_path = case_dir / "case.mid"
        midi_path.write_bytes(data)
        conf_path = case_dir / "lmms.conf"
        conf_path.write_text(CONFIG, encoding="utf-8")
        home = case_dir / "home"
        (home / "Documents" / "lmms").mkdir(parents=True, exist_ok=True)
        env = dict(os.environ)
        env.update(
            {
                "QT_QPA_PLATFORM": "offscreen",
                "HOME": str(home),
                "XDG_CONFIG_HOME": str(case_dir / "config"),
                "XDG_DATA_HOME": str(case_dir / "data"),
                "XDG_CACHE_HOME": str(case_dir / "cache"),
                "ASAN_OPTIONS": "detect_leaks=1:abort_on_error=1",
                "LSAN_OPTIONS": f"suppressions={STRESS_ROOT / 'lsan.supp'}",
            }
        )
        cmd = [str(binary), "--config", str(conf_path), "--import", str(midi_path), "-e"]
        timed_out = False
        t0 = time.monotonic()
        proc = subprocess.Popen(
            cmd,
            cwd=str(REPO_ROOT),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
        )
        try:
            stdout, stderr = proc.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = proc.communicate(timeout=10)
        wall = time.monotonic() - t0
        text = (stdout + stderr).decode("utf-8", errors="replace")
        hits = [m for m in SANITIZER_MARKERS if m in text]
        signal_no = -proc.returncode if proc.returncode < 0 else None
        status = "pass"
        problems = []
        if timed_out:
            status = "timeout"
            problems.append(f"timeout {args.timeout}s")
        elif signal_no is not None:
            status = "crash"
            problems.append(f"signal {signal_no}")
        elif hits:
            status = "sanitizer"
            problems.append("markers: " + ", ".join(hits))
        rec = {
            "case": i,
            "status": status,
            "exit_code": proc.returncode,
            "wall_s": round(wall, 3),
            "problems": problems,
            "hits": hits,
            "case_path": str(case_dir),
        }
        metrics.write(json.dumps(rec) + "\n")
        metrics.flush()
        if status != "pass":
            failures += 1
            dest = failures_dir / f"midi-{i:06d}"
            dest.mkdir(parents=True, exist_ok=True)
            (dest / "case.mid").write_bytes(data)
            (dest / "stderr.log").write_text(text[-16000:], encoding="utf-8")
            (dest / "meta.json").write_text(json.dumps(rec, indent=2), encoding="utf-8")
            print(f"[{i+1}/{args.cases}] {status} {problems} -> {dest}", flush=True)
        elif (i + 1) % 25 == 0:
            print(f"[{i+1}/{args.cases}] pass {wall:.2f}s", flush=True)

    summary = {
        "run_id": run_id,
        "binary": str(binary),
        "cases": args.cases,
        "failures": failures,
        "elapsed_s": round(time.monotonic() - started, 1),
        "failures_dir": str(failures_dir),
    }
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    metrics.close()
    print(json.dumps(summary, indent=2), flush=True)
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
