"""X11 window helpers for the external GUI stress driver."""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path


def _run(cmd: list[str], timeout: float = 5.0, display: str = ":99") -> str:
    try:
        return subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout, check=False,
            env={**os.environ, "DISPLAY": display},
        ).stdout
    except (OSError, subprocess.TimeoutExpired):
        return ""


def available() -> bool:
    return all(shutil.which(name) for name in ("wmctrl", "xwininfo"))


def window_count(display: str = ":99") -> int:
    if not shutil.which("wmctrl"):
        return 0
    text = _run(["wmctrl", "-l"], timeout=5, display=display)
    return len([line for line in text.splitlines() if line.strip()])


def window_titles(display: str = ":99") -> list[str]:
    if not shutil.which("wmctrl"):
        return []
    titles = []
    for line in _run(["wmctrl", "-l"], display=display).splitlines():
        parts = line.split(None, 3)
        if len(parts) == 4:
            titles.append(parts[3])
    return titles


def client_count(display: str = ":99") -> int:
    if not shutil.which("xlsclients"):
        return 0
    text = _run(["xlsclients", "-display", display])
    return len([line for line in text.splitlines() if line.strip()])


def screenshot(path: Path, display: str = ":99") -> str | None:
    if not shutil.which("import"):
        return None
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        result = subprocess.run(
            ["import", "-display", display, "-window", "root", str(path)],
            capture_output=True,
            text=True,
            timeout=20,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    return str(path) if result.returncode == 0 and path.exists() else None


def dump_window_tree(path: Path, display: str = ":99") -> None:
    if not shutil.which("xwininfo"):
        return
    text = _run(["xwininfo", "-display", display, "-root", "-tree"], timeout=10, display=display)
    path.write_text(text, encoding="utf-8")
