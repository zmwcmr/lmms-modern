#!/usr/bin/env python3
"""Fuzz sample decoding through the real render path.

A demo project is rewritten to point an AudioFileProcessor at a corrupted
copy of a bundled sample. The loader/decoder must fail gracefully: no crash,
no sanitizer report and no hang.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import signal
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path

STRESS_ROOT = Path(__file__).resolve().parent
REPO_ROOT = STRESS_ROOT.parents[1]
sys.path.insert(0, str(STRESS_ROOT))

from runner import SANITIZER_MARKERS, default_env  # noqa: E402

SEED_PROJECT = REPO_ROOT / "data/projects/shorties/Crunk(Demo).mmp"
SAMPLES = [
    REPO_ROOT / "data/samples/drums/kick01.ogg",
    REPO_ROOT / "data/samples/shapes/additive.wav",
]


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
            end = min(len(buf), start + rng.randint(1, 512))
            del buf[start:end]
        elif op == 3:
            pos = rng.randrange(len(buf) + 1)
            buf[pos:pos] = bytes(rng.randrange(256) for _ in range(rng.randint(1, 128)))
        elif op == 4 and len(buf) > 16:
            start = rng.randrange(len(buf))
            end = min(len(buf), start + rng.randint(1, 256))
            pos = rng.randrange(len(buf) + 1)
            buf[pos:pos] = buf[start:end]
        else:
            # Corrupt a common header field.
            for needle in (b"OggS", b"RIFF", b"WAVE", b"fmt "):
                pos = buf.find(needle)
                if pos >= 0:
                    buf[pos : pos + len(needle)] = bytes(
                        rng.randrange(256) for _ in range(len(needle))
                    )
                    break
    return bytes(buf)


def make_project(sample_path: Path, rng: random.Random) -> bytes:
    tree = ET.parse(SEED_PROJECT)
    root = tree.getroot()
    for afp in root.iter("audiofileprocessor"):
        afp.set("src", str(sample_path))
        break
    for track in root.iter("track"):
        if list(track.iter("audiofileprocessor")):
            track.set("name", "fuzz-sample")
            break
    head = root.find("head")
    if head is not None:
        head.set("bpm", "600")
    return ET.tostring(root, encoding="utf-8", xml_declaration=True)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--cases", type=int, default=100)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=20)
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    run_id = args.run_id or time.strftime("samples-%Y%m%d-%H%M%S")
    run_dir = Path("/tmp/opencode/lmms-stress/runs") / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    failures_dir = STRESS_ROOT / "failures" / run_id
    failures_dir.mkdir(parents=True, exist_ok=True)
    metrics = (run_dir / "fuzz.jsonl").open("w", encoding="utf-8")

    seed_samples = [(p, p.read_bytes()) for p in SAMPLES if p.exists()]
    rng = random.Random(args.seed_start)
    failures = 0
    started = time.monotonic()
    for i in range(args.cases):
        seed_path, seed_data = rng.choice(seed_samples)
        data = mutate(seed_data, rng)
        case_dir = Path(tempfile.mkdtemp(prefix=f"sample-{i:06d}-", dir=run_dir))
        sample_path = case_dir / ("sample" + seed_path.suffix)
        sample_path.write_bytes(data)
        project_path = case_dir / "case.mmp"
        project_path.write_bytes(make_project(sample_path, rng))
        wav_path = case_dir / "case.wav"
        env = default_env(case_dir)
        cmd = [str(binary), "render", str(project_path), "-o", str(wav_path), "-f", "wav"]
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
            "sample": seed_path.name,
            "case_path": str(case_dir),
        }
        metrics.write(json.dumps(rec) + "\n")
        metrics.flush()
        if status != "pass":
            failures += 1
            dest = failures_dir / f"sample-{i:06d}"
            dest.mkdir(parents=True, exist_ok=True)
            (dest / "case.mmp").write_bytes(project_path.read_bytes())
            (dest / sample_path.name).write_bytes(data)
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
