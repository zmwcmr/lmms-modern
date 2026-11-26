#!/usr/bin/env python3
"""LMMS stability stress runner (end-to-end random rendering).

Examples:
  python3 tests/stress/run.py --build build --profile tiny --cases 50 --seed-start 1
  python3 tests/stress/run.py --build build --profile small --cases 200 --plugin-ratio 0.5
  python3 tests/stress/run.py --build build --force-plugin dexed --cases 10
"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
import time
from pathlib import Path

from generator import DEFAULT_SEEDS, PLUGIN_DEFS, PROFILES, Case, REPO_ROOT, generate_case
from runner import RunResult, collect_failure, run_case

STRESS_ROOT = Path(__file__).resolve().parent
DEFAULT_WORK = Path("/tmp/opencode/lmms-stress")


def resolve_binary(build: str) -> Path:
    path = Path(build)
    if path.is_dir():
        path = path / "lmms"
    if not path.exists():
        raise FileNotFoundError(f"lmms binary not found: {path}")
    return path.resolve()


def summarize(results: list[RunResult]) -> dict:
    total = len(results)
    by_status: dict[str, int] = {}
    for r in results:
        by_status[r.status] = by_status.get(r.status, 0) + 1
    walls = sorted(r.wall_s for r in results)
    rss = [r.max_rss_kb for r in results if r.max_rss_kb]
    return {
        "total": total,
        "by_status": by_status,
        "wall_p50": round(statistics.median(walls), 3) if walls else 0,
        "wall_p95": round(walls[int(len(walls) * 0.95) - 1], 3) if walls else 0,
        "wall_max": round(walls[-1], 3) if walls else 0,
        "rss_max_kb": max(rss) if rss else 0,
        "rss_p95_kb": sorted(rss)[int(len(rss) * 0.95) - 1] if rss else 0,
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", default="build", help="build directory or lmms binary")
    parser.add_argument("--profile", default="tiny", choices=sorted(PROFILES))
    parser.add_argument("--profiles", default=None, help="comma separated profiles")
    parser.add_argument("--cases", type=int, default=50)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--plugin-ratio", type=float, default=0.0)
    parser.add_argument("--force-plugin", default=None, help="plugin id, e.g. dexed")
    parser.add_argument("--seed-file", default=None)
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument("--keep-wav", action="store_true")
    parser.add_argument("--stop-after-failures", type=int, default=0)
    parser.add_argument("--soak-hours", type=float, default=0.0)
    parser.add_argument("--replay", action="store_true")
    parser.add_argument("--list-plugins", action="store_true")
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args(argv)

    if args.list_plugins:
        for p in PLUGIN_DEFS:
            exists = Path(p["path"]).exists()
            print(f"{p['id']:20s} {p['format']:5s} exists={exists} {p['path']}")
        return 0

    binary = resolve_binary(args.build)
    run_id = args.run_id or time.strftime("%Y%m%d-%H%M%S")
    run_dir = DEFAULT_WORK / "runs" / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    failures_dir = STRESS_ROOT / "failures" / run_id
    results_path = run_dir / "runs.jsonl"

    profiles = args.profiles.split(",") if args.profiles else [args.profile]
    for p in profiles:
        if p not in PROFILES:
            parser.error(f"unknown profile {p}")

    deadline: float | None = None
    if args.replay:
        regressions = sorted((STRESS_ROOT / "corpus" / "regressions").glob("*.mmp"))
        if not regressions:
            print("no regression cases found")
            return 0
        cases = []
        for path in regressions:
            data = path.read_bytes()
            cases.append(
                Case(
                    case_id=f"replay-{path.stem}",
                    seed=0,
                    profile="replay",
                    xml_bytes=data,
                    expect_audio=False,
                    strict_audio=False,
                    description=f"replay {path.name}",
                )
            )
    else:
        cases = []
        # Soak mode generates cases on demand so that a multi-hour run does
        # not have to keep every project in memory before rendering starts.
        deadline = (
            time.monotonic() + args.soak_hours * 3600 if args.soak_hours else None
        )
        if deadline is None:
            for i in range(args.cases):
                profile = profiles[i % len(profiles)]
                cases.append(
                    generate_case(
                        seed=args.seed_start + i,
                        profile=profile,
                        plugin_ratio=args.plugin_ratio,
                        seed_file=Path(args.seed_file) if args.seed_file else None,
                        force_plugin=args.force_plugin,
                    )
                )

    total_hint = len(cases) if not args.soak_hours else "soak"
    print(
        f"run_id={run_id} binary={binary} cases={total_hint} "
        f"profiles={profiles} plugin_ratio={args.plugin_ratio}",
        flush=True,
    )

    results: list[RunResult] = []
    failures = 0
    started = time.monotonic()
    seed = args.seed_start
    idx = 0
    with results_path.open("a", encoding="utf-8") as out:
        while True:
            if deadline is not None:
                if time.monotonic() >= deadline:
                    break
                profile = profiles[idx % len(profiles)]
                case = generate_case(
                    seed=seed,
                    profile=profile,
                    plugin_ratio=args.plugin_ratio,
                    seed_file=Path(args.seed_file) if args.seed_file else None,
                    force_plugin=args.force_plugin,
                )
                seed += 1
            else:
                if idx >= len(cases):
                    break
                case = cases[idx]

            idx += 1
            result = run_case(
                case,
                binary=binary,
                work_dir=run_dir,
                timeout=args.timeout,
                keep_wav=args.keep_wav,
            )
            results.append(result)
            out.write(json.dumps(result.to_json()) + "\n")
            out.flush()
            if result.status != "pass":
                failures += 1
                dest = collect_failure(result, failures_dir)
                print(
                    f"[{idx}/{total_hint}] {result.case_id} {result.status} "
                    f"{result.problems} -> {dest}",
                    flush=True,
                )
                if args.stop_after_failures and failures >= args.stop_after_failures:
                    print("stopping after failure limit", flush=True)
                    break
            elif idx % 10 == 0 or idx == len(cases):
                print(
                    f"[{idx}/{total_hint}] {result.case_id} pass "
                    f"{result.wall_s:.2f}s rss={result.max_rss_kb}KB",
                    flush=True,
                )

    summary = summarize(results)
    summary.update(
        {
            "run_id": run_id,
            "binary": str(binary),
            "plugins": args.force_plugin or args.plugin_ratio,
            "elapsed_s": round(time.monotonic() - started, 1),
            "failures_dir": str(failures_dir),
        }
    )
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2), flush=True)
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
