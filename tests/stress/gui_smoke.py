#!/usr/bin/env python3
"""GUI start/load/quit stress for LMMS (offscreen, isolated config).

Each iteration starts the real GUI, optionally loads a generated project,
lets it settle, sends SIGINT (the supported graceful-shutdown path) and
requires the process to exit with code 3 within a bounded time.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

STRESS_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(STRESS_ROOT))

from generator import REPO_ROOT, generate_case  # noqa: E402

CONFIG = """<?xml version="1.0"?>
<lmms version="1.3.0-alpha.2.266" configversion="3">
  <app configured="1"/>
  <audioengine audiodev="Dummy (no sound output)" mididev="Dummy" samplerate="44100" framesperaudiobuffer="256"/>
</lmms>
"""


def make_env(case_dir: Path) -> dict[str, str]:
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
    return env


def run_once(binary: Path, case_dir: Path, project: Path | None, settle: float, timeout: float) -> dict:
    conf = case_dir / "lmms.conf"
    conf.write_text(CONFIG, encoding="utf-8")
    cmd = [str(binary), "--config", str(conf)]
    if project is not None:
        cmd.append(str(project))
    env = make_env(case_dir)
    started = time.monotonic()
    proc = subprocess.Popen(
        cmd,
        cwd=str(REPO_ROOT),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        start_new_session=True,
    )
    # Wait for the configured settle time, then ask for a graceful shutdown.
    deadline = started + settle
    while time.monotonic() < deadline and proc.poll() is None:
        time.sleep(0.05)
    started_up = proc.poll() is None
    if started_up:
        try:
            proc.send_signal(signal.SIGINT)
        except ProcessLookupError:
            pass
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

    wall = time.monotonic() - started
    text = (stdout + stderr).decode("utf-8", errors="replace")
    hits = [
        marker
        for marker in (
            "AddressSanitizer",
            "LeakSanitizer",
            "UndefinedBehaviorSanitizer",
            "Segmentation fault",
            "ASSERT",
        )
        if marker in text
    ]
    status = "pass"
    problems = []
    if not started_up and proc.returncode not in (0, 3):
        status = "startup_failure"
        problems.append(f"exit {proc.returncode} before settle")
    if timed_out:
        status = "timeout"
        problems.append(f"did not exit within {timeout}s")
    elif proc.returncode not in (0, 3):
        status = "fail"
        problems.append(f"exit {proc.returncode}")
    if hits:
        status = "sanitizer"
        problems.append("markers: " + ", ".join(hits))

    return {
        "status": status,
        "exit_code": proc.returncode,
        "wall_s": round(wall, 3),
        "started_up": started_up,
        "timed_out": timed_out,
        "problems": problems,
        "hits": hits,
        "stdout_tail": stdout.decode("utf-8", errors="replace")[-4000:],
        "stderr_tail": stderr.decode("utf-8", errors="replace")[-8000:],
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--profile", default="tiny")
    parser.add_argument("--settle", type=float, default=4.0)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--plugin-ratio", type=float, default=0.0)
    parser.add_argument("--project", default=None, help="fixed project instead of generated")
    parser.add_argument("--no-project", action="store_true")
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    run_id = args.run_id or time.strftime("gui-%Y%m%d-%H%M%S")
    run_dir = Path("/tmp/opencode/lmms-stress/gui") / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    results = []

    for i in range(args.cases):
        case_dir = Path(tempfile.mkdtemp(prefix=f"gui-{i:04d}-", dir=run_dir))
        project: Path | None = None
        if args.project:
            project = Path(args.project)
        elif not args.no_project:
            case = generate_case(
                seed=args.seed_start + i,
                profile=args.profile,
                plugin_ratio=args.plugin_ratio,
            )
            project = case_dir / "project.mmp"
            project.write_bytes(case.xml_bytes)
        result = run_once(binary, case_dir, project, args.settle, args.timeout)
        result["case"] = i
        result["project"] = str(project) if project else None
        results.append(result)
        if result["status"] != "pass":
            (case_dir / "stderr.log").write_text(result["stderr_tail"], encoding="utf-8")
            (case_dir / "stdout.log").write_text(result["stdout_tail"], encoding="utf-8")
            print(f"[{i+1}/{args.cases}] {result['status']} {result['problems']} -> {case_dir}")
        elif (i + 1) % 5 == 0:
            print(f"[{i+1}/{args.cases}] pass {result['wall_s']:.2f}s")

    summary = {
        "run_id": run_id,
        "binary": str(binary),
        "total": len(results),
        "by_status": {
            s: sum(1 for r in results if r["status"] == s)
            for s in sorted({r["status"] for r in results})
        },
        "wall_p50": sorted(r["wall_s"] for r in results)[len(results) // 2] if results else 0,
    }
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if summary["by_status"].get("pass", 0) == len(results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
