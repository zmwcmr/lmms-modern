#!/usr/bin/env python3
"""Probe real third-party plugins with a minimal render.

Generates one fixture project per plugin (based on a demo song), renders it
with the local LMMS build and records load/render/audio metrics.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

STRESS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(STRESS_ROOT))

from generator import (  # noqa: E402
    Case,
    PLUGIN_DEFS,
    REPO_ROOT,
    _inject_plugins,
    _load_seed,
    _write_tree,
)
from runner import run_case  # noqa: E402

INVENTORY = Path(__file__).resolve().parent / "inventory.local.json"

EXTRA_PLUGINS = [
    {
        "id": "surge-xt-vst3",
        "format": "vst3",
        "name": "Surge XT",
        "path": str(Path.home() / ".vst3/Surge XT.vst3"),
        "class": "abcdef019182faeb566d624153675854",
        "requires_content": False,
    },
    {
        "id": "ob-xf-vst3",
        "format": "vst3",
        "name": "OB-Xf",
        "path": str(Path.home() / ".vst3/OB-Xf.vst3"),
        "class": "abcdef019182faeb535354784f425846",
        "requires_content": False,
    },
    {
        "id": "sfizz-vst3",
        "format": "vst3",
        "name": "sfizz",
        "path": str(Path.home() / ".vst3/sfizz.vst3"),
        "class": "e8fab71815ed46e38b5983101e12993f",
        "requires_content": True,
    },
    {
        "id": "pianoteq-vst2",
        "format": "vst2",
        "name": "Pianoteq 8 STAGE",
        "path": str(Path.home() / ".vst/Pianoteq 8 STAGE.so"),
        "requires_content": True,
    },
]

SEED = REPO_ROOT / "data/projects/shorties/Crunk(Demo).mmp"


def plugin_list(include_optional: bool) -> list[dict]:
    plugins = []
    for plugin in PLUGIN_DEFS + EXTRA_PLUGINS:
        if not Path(plugin["path"]).exists():
            continue
        if plugin.get("requires_content") and not include_optional:
            continue
        plugins.append(plugin)
    return plugins


def make_fixture(plugin: dict) -> Case:
    tree = _load_seed(SEED)
    root = tree.getroot()
    injected = _inject_plugins(root, __import__("random").Random(0), [plugin])
    if not injected:
        raise RuntimeError("could not inject plugin")
    data = _write_tree(root, False, __import__("random").Random(0))
    return Case(
        case_id=f"plugin-{plugin['id']}",
        seed=0,
        profile="plugin",
        xml_bytes=data,
        expect_audio=not plugin.get("requires_content", False),
        strict_audio=not plugin.get("requires_content", False),
        plugins=[plugin],
        notes=1,
        description=f"plugin fixture {plugin['id']}",
    )


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--include-optional", action="store_true")
    parser.add_argument("--keep-wav", action="store_true")
    parser.add_argument("--run-id", default="plugin-probe")
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()
    work = Path("/tmp/opencode/lmms-stress/runs") / args.run_id
    work.mkdir(parents=True, exist_ok=True)

    results = []
    for plugin in plugin_list(args.include_optional):
        print(f"--- {plugin['id']} ({plugin['format']})")
        try:
            case = make_fixture(plugin)
        except Exception as exc:  # noqa: BLE001
            print(f"    fixture failed: {exc}")
            continue
        result = run_case(
            case,
            binary=binary,
            work_dir=work,
            timeout=args.timeout,
            keep_wav=args.keep_wav,
        )
        results.append(
            {
                "plugin": plugin,
                "status": result.status,
                "wall_s": round(result.wall_s, 3),
                "max_rss_kb": result.max_rss_kb,
                "wav": result.wav,
                "problems": result.problems,
                "stderr_hits": result.stderr_hits,
                "case_path": result.case_path,
            }
        )
        print(
            f"    {result.status} {result.wall_s:.2f}s rss={result.max_rss_kb}KB "
            f"rms={result.wav.get('rms')} peak={result.wav.get('peak')} "
            f"problems={result.problems}"
        )

    INVENTORY.write_text(
        json.dumps({"seed": str(SEED), "results": results}, indent=2),
        encoding="utf-8",
    )
    print(f"inventory written to {INVENTORY}")

    hard_failures = [
        r for r in results if r["status"] in ("crash", "timeout", "sanitizer")
    ]
    return 1 if hard_failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
