#!/usr/bin/env python3
"""Open/close real plugin editors under Xvfb and optionally kill editor hosts."""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

GUI_DIR = Path(__file__).resolve().parent
STRESS_ROOT = GUI_DIR.parent
if str(STRESS_ROOT) not in sys.path:
    sys.path.insert(0, str(STRESS_ROOT))
if str(GUI_DIR) not in sys.path:
    sys.path.insert(0, str(GUI_DIR))

import window_checks  # noqa: E402
from gui_scenarios import XvfbSession  # noqa: E402
from plugins import fixtures, inventory  # noqa: E402
from runner import default_env  # noqa: E402


def _popen(cmd: list[str], env: dict, cwd: Path) -> subprocess.Popen:
    return subprocess.Popen(
        cmd,
        cwd=str(cwd),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        start_new_session=True,
    )


def _stop(proc: subprocess.Popen) -> tuple[int, int, str]:
    if proc.poll() is None:
        try:
            os.killpg(proc.pid, signal.SIGINT)
        except ProcessLookupError:
            pass
        try:
            stdout, stderr = proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = proc.communicate(timeout=10)
    else:
        stdout, stderr = proc.communicate()
    return proc.returncode, -proc.returncode if proc.returncode < 0 else 0, stderr.decode(
        "utf-8", errors="replace"
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--plugin", default="dexed")
    parser.add_argument("--format", default="vst3", choices=("vst2", "vst3", "lv2"))
    parser.add_argument("--seconds", type=float, default=5)
    parser.add_argument("--xvfb", action="store_true")
    parser.add_argument("--display", default=None, help="use an existing X server")
    parser.add_argument("--run-id", default="plugin-editors")
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    plugins = inventory.load_inventory()
    try:
        plugin = inventory.by_id(plugins, args.plugin)
    except KeyError:
        print(f"plugin not in inventory: {args.plugin}", file=sys.stderr)
        return 2
    if plugin.get("format") != args.format:
        print(
            f"warning: {plugin['id']} is {plugin.get('format')}, not {args.format}",
            file=sys.stderr,
        )
    data = fixtures.load_or_create(plugin, "instruments")

    run_dir = Path("/tmp/opencode/lmms-stress/gui") / args.run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    project = run_dir / "project.mmp"
    project.write_bytes(data)
    (run_dir / "lmms.conf").write_text(
        "<?xml version=\"1.0\"?>\n"
        "<lmms version=\"1.3.0-alpha.2.266\" configversion=\"3\">\n"
        "  <app configured=\"1\"/>\n"
        "  <audioengine audiodev=\"Dummy (no sound output)\" mididev=\"Dummy\"/>\n"
        "  <paths defaultsf2=\"/tmp/lmms-dummy.sf2\"/>\n"
        "</lmms>\n",
        encoding="utf-8",
    )
    result = {
        "plugin": plugin["id"],
        "format": args.format,
        "seconds": args.seconds,
        "mode": "editor" if args.format == "vst3" else "project-load",
        "windows": [],
        "exit_code": None,
        "problems": [],
    }

    def run_session(display: str | None) -> None:
        env = default_env(run_dir)
        if display:
            env["DISPLAY"] = display
            env["QT_QPA_PLATFORM"] = "xcb"
        else:
            env["QT_QPA_PLATFORM"] = "offscreen"

        if args.format == "vst3":
            editor = binary.parent / "plugins" / "vst3-editor-test"
            if not editor.exists():
                result["problems"].append(f"missing editor helper: {editor}")
                return
            env["VST3_EDITOR_TEST_SECONDS"] = str(max(1, int(args.seconds)))
            cmd = [str(editor), plugin["path"], plugin.get("class", "")]
            proc = _popen(cmd, env, binary.parent)
            time.sleep(args.seconds)
            if display:
                window_checks.screenshot(run_dir / "editor.png", display)
                result["windows"] = window_checks.window_titles(display)
            try:
                proc.wait(timeout=args.seconds + 30)
            except subprocess.TimeoutExpired:
                pass
            code, sig, stderr = _stop(proc)
            result["exit_code"] = code
            result["signal"] = sig
            result["stderr"] = stderr[-8000:]
        else:
            # VST2 and LV2 editors need a click inside the GUI. This lane
            # only verifies that the project loads and the plugin survives
            # a full GUI startup; the editor itself is exercised through
            # the Xvfb/xdotool lane.
            proc = _popen(
                [str(binary), "--config", str(run_dir / "lmms.conf"), str(project)],
                env,
                binary.parent,
            )
            if display:
                # Bounded random clicks/keys: a double-click on the track
                # can open the plug-in editor window, which the window
                # checks then observe.
                subprocess.Popen(
                    [
                        sys.executable,
                        str(GUI_DIR / "xdotool_driver.py"),
                        "--window", "LMMS",
                        "--seconds", str(max(1, int(args.seconds))),
                    ],
                    env=env,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
            time.sleep(args.seconds)
            if display:
                result["windows"] = window_checks.window_titles(display)
                window_checks.screenshot(run_dir / "editor.png", display)
            code, sig, stderr = _stop(proc)
            result["exit_code"] = code
            result["signal"] = sig
            result["stderr"] = stderr[-8000:]

    if args.xvfb:
        with XvfbSession(":99"):
            run_session(":99")
    elif args.display:
        run_session(args.display)
    else:
        run_session(None)

    if result.get("signal"):
        result["problems"].append(f"signal {result['signal']}")
    if args.format == "vst3" and result.get("exit_code") == 3:
        # vst3-editor-test uses 3 for "plugin has no editor".
        result["editor_unavailable"] = True
    allowed = (0, 3, None)
    if result.get("exit_code") not in allowed:
        result["problems"].append(f"exit code {result['exit_code']}")
    (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({k: result[k] for k in ("plugin", "format", "exit_code", "problems")}, indent=2))
    return 0 if not result["problems"] else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
