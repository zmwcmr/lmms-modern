#!/usr/bin/env python3
"""Run MIDI/RIFF import in a separate LMMS process with a dialog watchdog."""

from __future__ import annotations

import argparse
import json
import os
import random
import signal
import subprocess
import sys
import time
from pathlib import Path

IMPORT_DIR = Path(__file__).resolve().parent
STRESS_ROOT = IMPORT_DIR.parent
GUI_DIR = STRESS_ROOT / "gui"
for path in (IMPORT_DIR, STRESS_ROOT, GUI_DIR):
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))

from dialog_watchdog import DialogWatchdog, save_events  # noqa: E402
from midi_corpus import make_midi, mutate  # noqa: E402
from runner import SANITIZER_MARKERS, default_env  # noqa: E402

CONFIG = """<?xml version="1.0"?>
<lmms version="1.3.0-alpha.2.266" configversion="3">
  <app configured="1"/>
  <audioengine audiodev="Dummy (no sound output)" mididev="Dummy"/>
  <paths defaultsf2="/tmp/lmms-dummy.sf2"/>
</lmms>
"""


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--cases", type=int, default=20)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=15)
    parser.add_argument("--display", default=None, help="e.g. :99 for Xvfb")
    parser.add_argument("--run-id", default="midi-import")
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    run_dir = Path("/tmp/opencode/lmms-stress/runs") / args.run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    base = make_midi()
    rng = random.Random(args.seed_start)
    results = []

    for index in range(args.cases):
        case_dir = run_dir / f"case-{index:04d}"
        case_dir.mkdir(parents=True, exist_ok=True)
        midi = base if index == 0 else mutate(base, rng)
        midi_path = case_dir / "case.mid"
        midi_path.write_bytes(midi)
        config_path = case_dir / "lmms.conf"
        config_path.write_text(CONFIG, encoding="utf-8")
        env = default_env(case_dir)
        env["QT_QPA_PLATFORM"] = "xcb" if args.display else "offscreen"
        if args.display:
            env["DISPLAY"] = args.display
        watchdog = DialogWatchdog(args.display or ":99")
        if args.display:
            watchdog.start()
        cmd = [
            str(binary),
            "--config",
            str(config_path),
            "--import",
            str(midi_path),
            "-e",
        ]
        started = time.monotonic()
        proc = subprocess.Popen(
            cmd,
            cwd=str(STRESS_ROOT.parents[1]),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
        )
        timed_out = False
        try:
            stdout, stderr = proc.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = proc.communicate(timeout=10)
        events = watchdog.stop() if args.display else []
        if events:
            save_events(events, case_dir / "dialogs.json")
        text = (stdout + stderr).decode("utf-8", errors="replace")
        hits = [marker for marker in SANITIZER_MARKERS if marker in text]
        result = {
            "case": index,
            "exit_code": proc.returncode,
            "signal": -proc.returncode if proc.returncode < 0 else None,
            "timed_out": timed_out,
            "wall_s": round(time.monotonic() - started, 3),
            "dialogs": events,
            "sanitizer_hits": hits,
            "stdout": stdout.decode("utf-8", errors="replace")[-4000:],
            "stderr": stderr.decode("utf-8", errors="replace")[-8000:],
        }
        results.append(result)
        status = "pass"
        if timed_out:
            status = "dialog_timeout" if events else "timeout"
        elif result["signal"] is not None:
            status = "crash"
        elif hits:
            status = "sanitizer"
        print(
            f"[{index + 1}/{args.cases}] {status} rc={proc.returncode} "
            f"dialogs={len(events)} {result['wall_s']:.2f}s",
            flush=True,
        )

    (run_dir / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    hard = [
        r for r in results
        if r["signal"] is not None or r["timed_out"] or r["sanitizer_hits"]
    ]
    return 0 if not hard else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
