#!/usr/bin/env python3
"""Drive GuiStress one scenario per process, offscreen or under Xvfb."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

GUI_DIR = Path(__file__).resolve().parent
if str(GUI_DIR) not in sys.path:
    sys.path.insert(0, str(GUI_DIR))

import gui_checks  # noqa: E402
import window_checks  # noqa: E402
from dialog_watchdog import DialogWatchdog, save_events  # noqa: E402

DEFAULT_SCENARIOS = [
    "startup_quit",
    "track_crud",
    "editors",
    "undo_redo",
    "mixer_effects",
    "resize",
    "keyboard",
    "random_actions",
    "load_play_save",
]


class XvfbSession:
    def __init__(self, display: str = ":99"):
        self.display = display
        self.xvfb: subprocess.Popen | None = None
        self.wm: subprocess.Popen | None = None

    def __enter__(self):
        if not shutil.which("Xvfb"):
            raise RuntimeError(
                "Xvfb not found; install xorg-x11-server-Xvfb"
            )
        self.xvfb = subprocess.Popen(
            [shutil.which("Xvfb"), self.display, "-screen", "0", "1920x1080x24", "-nolisten", "tcp"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(0.5)
        if shutil.which("openbox"):
            self.wm = subprocess.Popen(
                [shutil.which("openbox")],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                env={**os.environ, "DISPLAY": self.display},
            )
        return self

    def __exit__(self, *_exc):
        for proc in (self.wm, self.xvfb):
            if proc is not None and proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    proc.kill()


def resolve_binary(build: str) -> Path:
    path = Path(build)
    if path.is_dir():
        candidates = [
            path / "tests" / "stress" / "GuiStress",
            path / "tests" / "GuiStress",
            path / "GuiStress",
        ]
        path = next((candidate for candidate in candidates if candidate.exists()),
                    candidates[0])
    return path.resolve()


def run_scenario(
    binary: Path,
    scenario: str,
    project: str | None,
    output: Path,
    timeout: float,
    display: str | None,
) -> gui_checks.GuiResult:
    result = gui_checks.GuiResult(scenario=scenario)
    env = dict(os.environ)
    env["LMMS_STRESS_HOME"] = str(output.parent / "home")
    if display:
        env["DISPLAY"] = display
        env["QT_QPA_PLATFORM"] = "xcb"
    else:
        env["QT_QPA_PLATFORM"] = "offscreen"

    result.windows_before = window_checks.window_count(display or ":99")
    result.clients_before = window_checks.client_count(display or ":99")
    watchdog = DialogWatchdog(display or ":99")
    if display:
        watchdog.start()

    cmd = [str(binary), "--scenario", scenario, "--output", str(output)]
    if project:
        cmd += ["--project", project]
    started = time.monotonic()
    proc = subprocess.Popen(
        cmd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        start_new_session=True,
    )
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        result.timed_out = True
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = proc.communicate(timeout=10)

    events = watchdog.stop() if display else []
    if events:
        save_events(events, output.parent / "dialogs.json")
        result.modal_seen = any(
            not event.get("dismissed", False) for event in events
        )
    result.exit_code = proc.returncode
    result.signal = -proc.returncode if proc.returncode < 0 else None
    result.wall_s = round(time.monotonic() - started, 3)
    result.stdout = stdout.decode("utf-8", errors="replace")[-8000:]
    result.stderr = stderr.decode("utf-8", errors="replace")[-16000:]
    result.windows_after = window_checks.window_count(display or ":99")
    result.clients_after = window_checks.client_count(display or ":99")
    if display:
        result.screenshot = window_checks.screenshot(
            output.parent / "screenshot.png", display
        )
        window_checks.dump_window_tree(output.parent / "windows.txt", display)
    gui_checks.validate(result)
    return result


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--binary", default=None)
    parser.add_argument("--scenarios", default=",".join(DEFAULT_SCENARIOS))
    parser.add_argument("--project", default=None)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--xvfb", action="store_true")
    parser.add_argument("--display", default=":99")
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.binary) if args.binary else resolve_binary(args.build)
    if not binary.exists():
        print(f"GuiStress binary not found: {binary}", file=sys.stderr)
        return 2
    run_id = args.run_id or time.strftime("gui-%Y%m%d-%H%M%S")
    run_dir = Path("/tmp/opencode/lmms-stress/gui") / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    scenarios = [s for s in args.scenarios.split(",") if s]
    if args.project is None:
        demo = GUI_DIR.parents[2] / "data/projects/shorties/Crunk(Demo).mmp"
        if demo.exists():
            args.project = str(demo)

    results = []
    if args.xvfb:
        with XvfbSession(args.display):
            for scenario in scenarios:
                out = run_dir / scenario
                out.mkdir(parents=True, exist_ok=True)
                result = run_scenario(
                    binary, scenario, args.project, out / "out.mmp",
                    args.timeout, args.display
                )
                results.append(result)
                print(
                    f"{scenario:16s} {result.exit_code} {result.wall_s:6.1f}s "
                    f"windows={result.windows_before}->{result.windows_after} "
                    f"{result.problems}",
                    flush=True,
                )
    else:
        for scenario in scenarios:
            out = run_dir / scenario
            out.mkdir(parents=True, exist_ok=True)
            result = run_scenario(
                binary, scenario, args.project, out / "out.mmp",
                args.timeout, None
            )
            results.append(result)
            print(
                f"{scenario:16s} {result.exit_code} {result.wall_s:6.1f}s "
                f"{result.problems}",
                flush=True,
            )

    summary = {
        "run_id": run_id,
        "binary": str(binary),
        "total": len(results),
        "failures": sum(1 for result in results if result.problems),
        "results": [result.to_dict() for result in results],
    }
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps({k: summary[k] for k in ("run_id", "total", "failures")}, indent=2))
    return 0 if summary["failures"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
