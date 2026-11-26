#!/usr/bin/env python3
"""Byte-level fuzzing of LMMS project files through the real CLI loader."""

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
from pathlib import Path

STRESS_ROOT = Path(__file__).resolve().parent
REPO_ROOT = STRESS_ROOT.parents[1]
sys.path.insert(0, str(STRESS_ROOT))

from runner import SANITIZER_MARKERS, default_env  # noqa: E402

SEEDS = [
    REPO_ROOT / "data/projects/shorties/Crunk(Demo).mmp",
    REPO_ROOT / "data/projects/demos/StrictProduction-DearJonDoe.mmp",
    REPO_ROOT / "tests/emptyproject.mmp",
]


def mutate(data: bytes, rng: random.Random) -> bytes:
    buf = bytearray(data)
    for _ in range(rng.randint(1, 8)):
        op = rng.randrange(7)
        if op == 0 and len(buf) > 32:
            # truncate
            buf = buf[: rng.randint(1, len(buf) - 1)]
        elif op == 1 and len(buf) > 32:
            # delete a span
            start = rng.randrange(len(buf))
            end = min(len(buf), start + rng.randint(1, 4096))
            del buf[start:end]
        elif op == 2 and len(buf) > 32:
            # replace a span
            start = rng.randrange(len(buf))
            end = min(len(buf), start + rng.randint(1, 1024))
            buf[start:end] = bytes(rng.randrange(256) for _ in range(end - start))
        elif op == 3:
            # insert random bytes
            pos = rng.randrange(len(buf) + 1)
            buf[pos:pos] = bytes(rng.randrange(256) for _ in range(rng.randint(1, 256)))
        elif op == 4 and len(buf) > 32:
            # duplicate a span
            start = rng.randrange(len(buf))
            end = min(len(buf), start + rng.randint(1, 2048))
            pos = rng.randrange(len(buf) + 1)
            buf[pos:pos] = buf[start:end]
        elif op == 5:
            # XML-specific corruption
            pos = rng.randrange(len(buf))
            buf[pos] = rng.choice(b"<>&\"'/%")
        else:
            # change a version/attribute marker
            needle = rng.choice(
                [b'version="1.0"', b'configversion="', b'creatorversion="', b'type="song"']
            )
            pos = buf.find(needle)
            if pos >= 0:
                buf[pos : pos + len(needle)] = bytes(rng.randrange(256) for _ in range(len(needle)))
    return bytes(buf)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--cases", type=int, default=300)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=15)
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    run_id = args.run_id or time.strftime("fuzz-%Y%m%d-%H%M%S")
    run_dir = Path("/tmp/opencode/lmms-stress/runs") / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    failures_dir = STRESS_ROOT / "failures" / run_id
    failures_dir.mkdir(parents=True, exist_ok=True)
    metrics = (run_dir / "fuzz.jsonl").open("w", encoding="utf-8")

    seed_bytes = [p.read_bytes() for p in SEEDS if p.exists()]
    rng = random.Random(args.seed_start)
    failures = 0
    started = time.monotonic()
    for i in range(args.cases):
        seed = rng.choice(seed_bytes)
        data = mutate(seed, rng)
        case_dir = Path(tempfile.mkdtemp(prefix=f"fuzz-{i:06d}-", dir=run_dir))
        case_path = case_dir / "case.mmp"
        wav_path = case_dir / "case.wav"
        case_path.write_bytes(data)
        env = default_env(case_dir)
        cmd = [str(binary), "render", str(case_path), "-o", str(wav_path), "-f", "wav"]
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
            "case_path": str(case_path),
        }
        metrics.write(json.dumps(rec) + "\n")
        metrics.flush()
        if status != "pass":
            failures += 1
            dest = failures_dir / f"fuzz-{i:06d}"
            dest.mkdir(parents=True, exist_ok=True)
            (dest / "case.mmp").write_bytes(data)
            (dest / "stderr.log").write_text(text[-16000:], encoding="utf-8")
            (dest / "meta.json").write_text(json.dumps(rec, indent=2), encoding="utf-8")
            print(f"[{i+1}/{args.cases}] {status} {problems} -> {dest}", flush=True)
        elif (i + 1) % 50 == 0:
            print(f"[{i+1}/{args.cases}] pass {wall_s_text(wall)}", flush=True)

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


def wall_s_text(value: float) -> str:
    return f"{value:.2f}s"


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
