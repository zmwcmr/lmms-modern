#!/usr/bin/env python3
"""Render projects containing N instances of the same real plugin."""

from __future__ import annotations

import argparse
import copy
import json
import random
import sys
from pathlib import Path

PLUGIN_DIR = Path(__file__).resolve().parent
STRESS_ROOT = PLUGIN_DIR.parent
if str(STRESS_ROOT) not in sys.path:
    sys.path.insert(0, str(STRESS_ROOT))

from generator import (  # noqa: E402
    REPO_ROOT,
    Case,
    _inject_plugins,
    _load_seed,
    _write_tree,
)
from runner import run_case  # noqa: E402
from plugins import inventory  # noqa: E402

SEED = REPO_ROOT / "data/projects/shorties/Crunk(Demo).mmp"


def build_project(plugin: dict, count: int) -> bytes:
    tree = _load_seed(SEED)
    root = tree.getroot()
    if not _inject_plugins(root, random.Random(0), [plugin]):
        raise RuntimeError(f"no instrument track for {plugin['id']}")
    parents = {child: parent for parent in root.iter() for child in list(parent)}
    target = None
    for element in root.iter("instrument"):
        if element.get("name") in ("vst3instrument", "lv2instrument", "vestige"):
            node = element
            while node is not None and node.tag != "track":
                node = parents.get(node)
            target = node
            break
    if target is None:
        raise RuntimeError("could not locate plugin track")
    parent = parents[target]
    for i in range(1, count):
        duplicate = copy.deepcopy(target)
        duplicate.set("name", f"{plugin['name']}-{i}")
        parent.append(duplicate)
    root.find("head").set("bpm", "600")
    return _write_tree(root, False, random.Random(0))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--plugin", required=True)
    parser.add_argument("--counts", default="1,4,8,16")
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--keep-wav", action="store_true")
    parser.add_argument("--run-id", default="multi-instance")
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
    counts = [int(value) for value in args.counts.split(",") if value]
    work = Path("/tmp/opencode/lmms-stress/runs") / args.run_id
    work.mkdir(parents=True, exist_ok=True)

    results = []
    for count in counts:
        data = build_project(plugin, count)
        case = Case(
            case_id=f"{plugin['id']}-x{count}",
            seed=0,
            profile="multi",
            xml_bytes=data,
            expect_audio=False,
            strict_audio=False,
            plugins=[plugin],
            notes=count,
            description=f"{count} instances",
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
                "plugin": plugin["id"],
                "count": count,
                "status": result.status,
                "wall_s": round(result.wall_s, 3),
                "max_rss_kb": result.max_rss_kb,
                "max_fds": result.max_fds,
                "problems": result.problems,
            }
        )
        print(
            f"{plugin['id']:20s} n={count:3d} {result.status:6s} "
            f"{result.wall_s:7.2f}s rss={result.max_rss_kb / 1024:7.0f}MB "
            f"fds={result.max_fds}",
            flush=True,
        )

    output = PLUGIN_DIR / f"multi-{plugin['id']}.json"
    output.write_text(json.dumps(results, indent=2), encoding="utf-8")
    print(f"results written to {output}", flush=True)
    return 0 if all(r["status"] == "pass" for r in results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
