#!/usr/bin/env python3
"""Run the concurrency stress binary, intended for a TSAN build."""

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
    parser.add_argument("--build", default="build-tsan")
    parser.add_argument("--binary", default=None)
    parser.add_argument("--mode", default="audio-model",
                        choices=("audio-model", "audio-editor", "automation", "mixer"))
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--timeout", type=float, default=None)
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.binary) if args.binary else Path(args.build) / "tests" / "stress" / "StressConcurrency"
    if not binary.exists():
        print(f"binary not found: {binary}", file=sys.stderr)
        return 2

    run_id = args.run_id or time.strftime("concurrency-%Y%m%d-%H%M%S")
    out_dir = Path("/tmp/opencode/lmms-stress/runs") / run_id
    out_dir.mkdir(parents=True, exist_ok=True)
    timeout = args.timeout or args.seconds + 30
    env = dict(os.environ)
    env.setdefault("TSAN_OPTIONS", "halt_on_error=0:second_deadlock_stack=1")
    env.setdefault("ASAN_OPTIONS", "detect_leaks=1:abort_on_error=1")

    cmd = [str(binary), "--mode", args.mode, "--seconds", str(args.seconds)]
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
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = proc.communicate(timeout=10)

    text = (stdout + stderr).decode("utf-8", errors="replace")
    hits = [marker for marker in SANITIZER_MARKERS if marker in text]
    result = {
        "run_id": run_id,
        "mode": args.mode,
        "seconds": args.seconds,
        "exit_code": proc.returncode,
        "timed_out": timed_out,
        "wall_s": round(time.monotonic() - started, 1),
        "sanitizer_hits": hits,
        "stdout": stdout.decode("utf-8", errors="replace")[-4000:],
        "stderr": stderr.decode("utf-8", errors="replace")[-16000:],
    }
    (out_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({k: result[k] for k in result if k not in ("stdout", "stderr")}, indent=2))
    return 0 if not timed_out and proc.returncode == 0 and not hits else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
