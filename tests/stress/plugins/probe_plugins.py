#!/usr/bin/env python3
"""Probe real third-party plugin instruments with a minimal render."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

PLUGIN_DIR = Path(__file__).resolve().parent
STRESS_ROOT = PLUGIN_DIR.parent
if str(STRESS_ROOT) not in sys.path:
    sys.path.insert(0, str(STRESS_ROOT))

from generator import Case  # noqa: E402
from runner import run_case  # noqa: E402
from plugins import fixtures, inventory  # noqa: E402


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--include-optional", action="store_true")
    parser.add_argument("--keep-wav", action="store_true")
    parser.add_argument("--run-id", default="plugin-probe")
    parser.add_argument("--write-inventory", action="store_true")
    parser.add_argument("--plugin", default=None, help="only probe one plugin id")
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()

    all_plugins = inventory.discover(
        include_optional=args.include_optional or args.plugin is not None
    )
    plugins = all_plugins
    if args.plugin:
        plugins = [p for p in plugins if p.get("id") == args.plugin]
    instruments = inventory.by_role(plugins, "instrument")
    if args.write_inventory:
        inventory.write_inventory(all_plugins)

    work = Path("/tmp/opencode/lmms-stress/runs") / args.run_id
    work.mkdir(parents=True, exist_ok=True)
    results = []
    for plugin in instruments:
        print(f"--- {plugin['id']} ({plugin['format']})", flush=True)
        try:
            data = fixtures.load_or_create(plugin, "instruments")
        except Exception as exc:  # noqa: BLE001
            print(f"    fixture skipped: {exc}", flush=True)
            results.append({"plugin": plugin, "status": "skipped", "reason": str(exc)})
            continue
        case = Case(
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
            f"problems={result.problems}",
            flush=True,
        )

    output = PLUGIN_DIR / "inventory.results.json"
    output.write_text(
        json.dumps({"results": results}, indent=2),
        encoding="utf-8",
    )
    print(f"results written to {output}", flush=True)

    hard = [r for r in results if r.get("status") in ("crash", "timeout", "sanitizer")]
    return 1 if hard else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
