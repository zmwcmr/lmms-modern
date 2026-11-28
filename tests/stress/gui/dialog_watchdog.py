"""Close unexpected modal dialogs through the X11 window manager."""

from __future__ import annotations

import os
import shutil
import subprocess
import threading
import time
from pathlib import Path

# Keep these narrow: the main window title contains words like "project"
# and "LMMS", so a broad match would send Escape to the editor itself.
DIALOG_WORDS = (
    "error",
    "warning",
    "could not",
    "couldn't",
    "couldn’t",
    "failed",
    "import file",
    "working directory",
    "setup",
    "missing",
    "not found",
)


class DialogWatchdog(threading.Thread):
    def __init__(self, display: str = ":99", interval: float = 0.5):
        super().__init__(daemon=True)
        self.display = display
        self.interval = interval
        self.stop_event = threading.Event()
        self.events: list[dict] = []
        self.tools = all(shutil.which(name) for name in ("wmctrl", "xdotool"))

    def _windows(self) -> list[str]:
        if not self.tools:
            return []
        try:
            text = subprocess.run(
                ["wmctrl", "-l"],
                capture_output=True,
                text=True,
                timeout=5,
                check=False,
                env={**os.environ, "DISPLAY": self.display},
            ).stdout
        except (OSError, subprocess.TimeoutExpired):
            return []
        return [line for line in text.splitlines() if line.strip()]

    def run(self) -> None:
        while not self.stop_event.wait(self.interval):
            for line in self._windows():
                lowered = line.lower()
                if not any(word in lowered for word in DIALOG_WORDS):
                    continue
                parts = line.split(None, 3)
                if len(parts) != 4:
                    continue
                window_id, title = parts[0], parts[3]
                if any(event["window"] == window_id for event in self.events):
                    continue
                event = {"time": time.time(), "window": window_id, "title": title}
                self.events.append(event)
                try:
                    env = {**os.environ, "DISPLAY": self.display}
                    subprocess.run(
                        ["xdotool", "windowactivate", "--sync", window_id],
                        check=False,
                        timeout=5,
                        env=env,
                    )
                    subprocess.run(
                        ["xdotool", "key", "--window", window_id, "Escape"],
                        check=False,
                        timeout=5,
                        env=env,
                    )
                    time.sleep(0.2)
                    event["dismissed"] = not any(
                        window_id in line for line in self._windows()
                    )
                except (OSError, subprocess.TimeoutExpired):
                    event["dismissed"] = False

    def stop(self) -> list[dict]:
        self.stop_event.set()
        self.join(timeout=2)
        return self.events


def save_events(events: list[dict], path: Path) -> None:
    import json

    path.write_text(json.dumps(events, indent=2), encoding="utf-8")
