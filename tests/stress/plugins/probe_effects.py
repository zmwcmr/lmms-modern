#!/usr/bin/env python3
"""Probe real third-party effects by inserting them into a demo fxchain."""

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
    parser.add_argument("--run-id", default="effect-probe")
    parser.add_argument("--effect", default=None)
    args = parser.parse_args(argv)

    binary = Path(args.build)
    if binary.is_dir():
        binary = binary / "lmms"
    binary = binary.resolve()

    plugins = inventory.discover(include_optional=True)
    effects = inventory.by_role(plugins, "effect")
    if args.effect:
        effects = [p for p in effects if p.get("id") == args.effect]

    work = Path("/tmp/opencode/lmms-stress/runs") / args.run_id
    work.mkdir(parents=True, exist_ok=True)
    results = []
    for effect in effects:
        print(f"--- {effect['id']} ({effect['format']})", flush=True)
        try:
            data = fixtures.load_or_create(effect, "effects")
        except Exception as exc:  # noqa: BLE001
            print(f"    fixture skipped: {exc}", flush=True)
            results.append({"effect": effect, "status": "skipped", "reason": str(exc)})
            continue
        case = Case(
            case_id=f"effect-{effect['id']}",
            seed=0,
            profile="effect",
            xml_bytes=data,
            expect_audio=False,
            strict_audio=False,
            effects=[effect],
            notes=1,
            description=f"effect fixture {effect['id']}",
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
                "effect": effect,
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
            f"rms={result.wav.get('rms')} problems={result.problems}",
            flush=True,
        )

    output = PLUGIN_DIR / "effect-results.json"
    output.write_text(json.dumps({"results": results}, indent=2), encoding="utf-8")
    print(f"results written to {output}", flush=True)

    hard = [r for r in results if r.get("status") in ("crash", "timeout", "sanitizer")]
    return 1 if hard else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
