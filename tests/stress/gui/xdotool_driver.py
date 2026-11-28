#!/usr/bin/env python3
"""Random but bounded X11 input for LMMS GUI/plugin windows."""

from __future__ import annotations

import argparse
import random
import shutil
import subprocess
import time


def _run(cmd: list[str], timeout: float = 5) -> str:
    try:
        return subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout, check=False
        ).stdout
    except (OSError, subprocess.TimeoutExpired):
        return ""


def find_window(name: str) -> str | None:
    output = _run(["xdotool", "search", "--name", name]).split()
    return output[0] if output else None


def geometry(window: str) -> tuple[int, int]:
    text = _run(["xdotool", "getwindowgeometry", "--shell", window])
    values = {}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    try:
        return int(values.get("WIDTH", "800")), int(values.get("HEIGHT", "600"))
    except ValueError:
        return 800, 600


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--window", default="LMMS")
    parser.add_argument("--seconds", type=float, default=10)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--clicks", type=int, default=100)
    args = parser.parse_args()
    if not shutil.which("xdotool"):
        print("xdotool not found; skipping")
        return 2

    window = find_window(args.window)
    if window is None:
        print(f"window not found: {args.window}")
        return 3
    width, height = geometry(window)
    rng = random.Random(args.seed)
    keys = [
        "space", "Left", "Right", "Up", "Down", "Home", "End",
        "ctrl+z", "ctrl+shift+z", "ctrl+s",
    ]
    deadline = time.monotonic() + args.seconds
    clicks = 0
    while time.monotonic() < deadline and clicks < args.clicks:
        action = rng.randrange(4)
        if action == 0:
            x = rng.randrange(max(1, width // 8), max(2, width - width // 8))
            y = rng.randrange(max(1, height // 8), max(2, height - height // 8))
            _run(["xdotool", "mousemove", "--window", window, str(x), str(y)])
            _run(["xdotool", "click", "1"])
            clicks += 1
        elif action == 1:
            _run(["xdotool", "key", "--window", window, rng.choice(keys)])
        elif action == 2:
            _run(["xdotool", "mousemove", "--window", window, "0", "0"])
            _run(["xdotool", "click", "1"])
            clicks += 1
        else:
            time.sleep(0.05)
    print(f"window={window} clicks={clicks} seconds={args.seconds}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
