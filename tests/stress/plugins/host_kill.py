#!/usr/bin/env python3
"""Kill plugin host processes during a render and verify LMMS survives."""

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

PLUGIN_DIR = Path(__file__).resolve().parent
STRESS_ROOT = PLUGIN_DIR.parent
if str(STRESS_ROOT) not in sys.path:
    sys.path.insert(0, str(STRESS_ROOT))

from runner import default_env  # noqa: E402
from plugins import fixtures, inventory  # noqa: E402

HOST_NAMES = {
    "vst3": ("lmms-vst3-host",),
    "vst2": ("NativeLinuxRemoteVstPlugin64", "RemoteVstPlugin64"),
    "lv2": ("lmms-lv2-host",),
}


def _cmdline(pid: int) -> str:
    try:
        return Path(f"/proc/{pid}/cmdline").read_bytes().replace(b"\0", b" ").decode(
            "utf-8", errors="replace"
        )
    except OSError:
        return ""


def _children(pid: int) -> list[int]:
    result: list[int] = []
    try:
        for tid in os.listdir(f"/proc/{pid}/task"):
            try:
                text = Path(f"/proc/{pid}/task/{tid}/children").read_text(
                    encoding="ascii", errors="ignore"
                )
                result.extend(int(value) for value in text.split())
            except OSError:
                pass
    except OSError:
        pass
    return result


def _tree(pid: int) -> list[int]:
    result = [pid]
    stack = [pid]
    while stack:
        current = stack.pop()
        for child in _children(current):
            if child not in result:
                result.append(child)
                stack.append(child)
    return result


def find_host(pid: int, names: tuple[str, ...]) -> tuple[int, str] | None:
    for candidate in _tree(pid):
        text = _cmdline(candidate)
        for name in names:
            if name in text:
                return candidate, name
    return None


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--plugin", default="dexed")
    parser.add_argument("--mode", choices=sorted(HOST_NAMES), default="vst3")
    parser.add_argument("--delay", type=float, default=1.0)
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--run-id", default="host-kill")
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
    data = fixtures.load_or_create(plugin, "instruments")

    work = Path("/tmp/opencode/lmms-stress/runs") / args.run_id
    work.mkdir(parents=True, exist_ok=True)
    case_dir = Path(tempfile.mkdtemp(prefix="host-kill-", dir=work))
    case_path = case_dir / "case.mmp"
    wav_path = case_dir / "case.wav"
    case_path.write_bytes(data)
    env = default_env(case_dir)
    if args.mode == "vst3":
        env["LMMS_VST3_REMOTE_DSP"] = "1"
    elif args.mode == "lv2":
        env["LMMS_LV2_REMOTE"] = "1"

    cmd = [
        str(binary),
        "render",
        str(case_path),
        "-o",
        str(wav_path),
        "-f",
        "wav",
    ]

    proc = subprocess.Popen(
        cmd,
        cwd=str(STRESS_ROOT.parents[1]),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        start_new_session=True,
    )
    host = None
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline and proc.poll() is None:
        host = find_host(proc.pid, HOST_NAMES[args.mode])
        if host and time.monotonic() >= deadline - args.timeout + args.delay:
            break
        time.sleep(0.05)

    killed = None
    if host:
        killed = {"pid": host[0], "name": host[1]}
        try:
            os.kill(host[0], signal.SIGKILL)
        except ProcessLookupError:
            pass
    else:
        print(f"host not found for mode {args.mode}", flush=True)

    timed_out = False
    try:
        stdout, stderr = proc.communicate(timeout=max(5, args.timeout))
    except subprocess.TimeoutExpired:
        timed_out = True
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = proc.communicate(timeout=10)

    result = {
        "mode": args.mode,
        "plugin": plugin["id"],
        "host": killed,
        "exit_code": proc.returncode,
        "signal": -proc.returncode if proc.returncode < 0 else None,
        "timed_out": timed_out,
        "stdout": stdout.decode("utf-8", errors="replace")[-4000:],
        "stderr": stderr.decode("utf-8", errors="replace")[-8000:],
    }
    (case_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({k: result[k] for k in result if k not in ("stdout", "stderr")}, indent=2))

    # A host crash is expected. LMMS must not crash or hang.
    if timed_out or result["signal"] is not None:
        return 1
    return 0 if result["exit_code"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
