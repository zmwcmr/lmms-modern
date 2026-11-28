#!/usr/bin/env python3
"""Run the single-process soak helper in chunks, with a local RSS summary."""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

STRESS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(STRESS_ROOT))

from runner import SANITIZER_MARKERS  # noqa: E402


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--project", required=True)
    parser.add_argument("--hours", type=float, default=8)
    parser.add_argument("--chunk-minutes", type=float, default=30)
    parser.add_argument("--mix", default="load-render-save",
                        choices=("load-save", "render", "load-render-save"))
    parser.add_argument("--rss-limit-mb", type=int, default=0)
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        candidates = [
            binary / "tests" / "stress" / "SingleProcessSoak",
            binary / "tests" / "SingleProcessSoak",
        ]
        binary = next((candidate for candidate in candidates if candidate.exists()),
                      candidates[0])
    binary = binary.resolve()
    if not binary.exists():
        print(f"soak binary not found: {binary}", file=sys.stderr)
        return 2

    run_id = args.run_id or time.strftime("soak-%Y%m%d-%H%M%S")
    out_dir = Path("/tmp/opencode/lmms-stress/soak") / run_id
    out_dir.mkdir(parents=True, exist_ok=True)
    metrics = (out_dir / "chunks.jsonl").open("w", encoding="utf-8")
    deadline = time.monotonic() + args.hours * 3600
    chunk_seconds = args.chunk_minutes * 60
    chunk_index = 0
    failures = 0

    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        seconds = min(chunk_seconds, remaining)
        if seconds < 5:
            break
        env = dict(os.environ)
        env.setdefault("ASAN_OPTIONS", "detect_leaks=1:abort_on_error=0")
        env.setdefault(
            "LSAN_OPTIONS",
            f"suppressions={STRESS_ROOT / 'lsan.supp'}",
        )
        cmd = [
            str(binary),
            "--seconds", str(seconds),
            "--project", args.project,
            "--mix", args.mix,
        ]
        if args.rss_limit_mb:
            cmd += ["--rss-limit-mb", str(args.rss_limit_mb)]
        started = time.monotonic()
        proc = subprocess.Popen(
            cmd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
        )
        timed_out = False
        try:
            stdout, stderr = proc.communicate(timeout=seconds + 120)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = proc.communicate(timeout=10)
        text = (stdout + stderr).decode("utf-8", errors="replace")
        hits = [marker for marker in SANITIZER_MARKERS if marker in text]
        record = {
            "chunk": chunk_index,
            "seconds": seconds,
            "exit_code": proc.returncode,
            "timed_out": timed_out,
            "wall_s": round(time.monotonic() - started, 1),
            "sanitizer_hits": hits,
            "stdout_tail": stdout.decode("utf-8", errors="replace")[-4000:],
            "stderr_tail": stderr.decode("utf-8", errors="replace")[-8000:],
        }
        metrics.write(json.dumps(record) + "\n")
        metrics.flush()
        print(
            f"chunk {chunk_index}: rc={proc.returncode} wall={record['wall_s']}s "
            f"asan={len(hits)}",
            flush=True,
        )
        if timed_out or proc.returncode != 0 or hits:
            failures += 1
            break
        chunk_index += 1

    metrics.close()
    summary = {
        "run_id": run_id,
        "chunks": chunk_index,
        "failures": failures,
        "hours_requested": args.hours,
        "mix": args.mix,
    }
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
