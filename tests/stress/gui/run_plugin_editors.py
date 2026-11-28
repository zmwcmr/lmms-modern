#!/usr/bin/env python3
"""Run every entry in plugin_editor_scenarios.json as a separate process."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

GUI_DIR = Path(__file__).resolve().parent


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--scenarios", default=str(GUI_DIR / "plugin_editor_scenarios.json"))
    parser.add_argument("--xvfb", action="store_true")
    parser.add_argument("--run-id", default="plugin-editors")
    args = parser.parse_args(argv)

    scenarios = json.loads(Path(args.scenarios).read_text(encoding="utf-8"))["scenarios"]
    failures = 0
    for scenario in scenarios:
        cmd = [
            sys.executable,
            str(GUI_DIR / "plugin_editors.py"),
            "--build", args.build,
            "--plugin", scenario["plugin"],
            "--format", scenario["format"],
            "--seconds", str(scenario.get("seconds", 5)),
            "--run-id", f"{args.run_id}-{scenario['id']}",
        ]
        if args.xvfb:
            cmd.append("--xvfb")
        print(f"=== {scenario['id']}: {' '.join(cmd)}", flush=True)
        result = subprocess.run(cmd, check=False)
        if result.returncode != 0:
            failures += 1
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
