#!/usr/bin/env python3
"""Numeric edge-case fuzzing for project attributes.

Mutates tempo, meter, note positions/lengths, clip lengths and mixer volumes
with NaN/Inf/overflow values. The loader must not crash, hang or divide by
zero.
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
VALUES = [
    "2147483647",
    "-2147483648",
    "4294967295",
    "99999999999999999999",
    "-99999999999999999999",
    "1e308",
    "-1e308",
    "nan",
    "NaN",
    "inf",
    "-inf",
    "0",
    "-1",
]


def mutate(root: ET.Element, rng: random.Random) -> None:
    targets = [
        root.find("head"),
        next(root.iter("note"), None),
        next(root.iter("pattern"), None),
        next(root.iter("bbtco"), None),
        next(root.iter("instrumenttrack"), None),
        next(root.iter("mixerchannel"), None),
        next(root.iter("automationpattern"), None),
        next(root.iter("time"), None),
    ]
    for elem in targets:
        if elem is None:
            continue
        attrs = list(elem.attrib)
        rng.shuffle(attrs)
        for attr in attrs[: rng.randint(1, min(4, len(attrs)))]:
            if rng.random() < 0.35:
                elem.set(attr, rng.choice(VALUES))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--cases", type=int, default=100)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    run_id = args.run_id or time.strftime("numbers-%Y%m%d-%H%M%S")
    run_dir = Path("/tmp/opencode/lmms-stress/runs") / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    failures_dir = STRESS_ROOT / "failures" / run_id
    failures_dir.mkdir(parents=True, exist_ok=True)
    metrics = (run_dir / "fuzz.jsonl").open("w", encoding="utf-8")

    rng = random.Random(args.seed_start)
    failures = 0
    started = time.monotonic()
    for i in range(args.cases):
        tree = ET.parse(SEED_PROJECT)
        root = tree.getroot()
        mutate(root, rng)
        data = ET.tostring(root, encoding="utf-8", xml_declaration=True)
        case_dir = Path(tempfile.mkdtemp(prefix=f"num-{i:06d}-", dir=run_dir))
        project = case_dir / "case.mmp"
        project.write_bytes(data)
        wav = case_dir / "case.wav"
        env = default_env(case_dir)
        cmd = [str(binary), "render", str(project), "-o", str(wav), "-f", "wav"]
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
        if status == "pass":
            try:
                wav.unlink()
            except OSError:
                pass
        if status != "pass":
            failures += 1
            dest = failures_dir / f"num-{i:06d}"
            dest.mkdir(parents=True, exist_ok=True)
            (dest / "case.mmp").write_bytes(data)
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
