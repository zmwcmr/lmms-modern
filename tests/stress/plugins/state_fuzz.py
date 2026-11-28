#!/usr/bin/env python3
"""Fuzz real-plugin project state (VST3 component/controller, VST2 chunks)."""

from __future__ import annotations

import argparse
import base64
import json
import random
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

PLUGIN_DIR = Path(__file__).resolve().parent
STRESS_ROOT = PLUGIN_DIR.parent
if str(STRESS_ROOT) not in sys.path:
    sys.path.insert(0, str(STRESS_ROOT))

from generator import Case  # noqa: E402
from runner import run_case  # noqa: E402
from plugins import fixtures, inventory  # noqa: E402

SIZES = [0, 1, 4, 16, 256, 65536]
EXTREME = ["-1", "0", "1", "2147483647", "99999999999999999999", "nan", "inf"]


def mutate(data: bytes, rng: random.Random) -> bytes:
    tree = ET.ElementTree(ET.fromstring(data))
    root = tree.getroot()
    elements = [
        element
        for element in root.iter()
        if element.tag in ("vst3instrument", "vst3effect", "vestige", "lv2instrument", "lv2effect")
    ]
    if not elements:
        raise RuntimeError("fixture has no plugin element")
    element = rng.choice(elements)
    for attr in ("componentstate", "controllerstate"):
        if rng.random() < 0.7:
            payload = bytes(rng.randrange(256) for _ in range(rng.choice(SIZES)))
            element.set(attr, base64.b64encode(payload).decode("ascii"))
    if rng.random() < 0.7:
        element.set("chunk", base64.b64encode(
            bytes(rng.randrange(256) for _ in range(rng.choice(SIZES)))
        ).decode("ascii"))
    if rng.random() < 0.3:
        element.set("program", rng.choice(EXTREME))
    if rng.random() < 0.3:
        element.set("unit", rng.choice(EXTREME))
    if rng.random() < 0.2:
        element.set("file", "/nonexistent/plugin")
    if rng.random() < 0.2:
        element.set("class", "0" * 32)
    return ET.tostring(root, encoding="utf-8", xml_declaration=True)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default="build")
    parser.add_argument("--plugin", default="dexed")
    parser.add_argument("--kind", choices=("instrument", "effect"), default="instrument")
    parser.add_argument("--cases", type=int, default=50)
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--run-id", default="state-fuzz")
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
    data = fixtures.load_or_create(
        plugin, "instruments" if args.kind == "instrument" else "effects"
    )

    work = Path("/tmp/opencode/lmms-stress/runs") / args.run_id
    work.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed_start)
    failures = 0
    for i in range(args.cases):
        case_data = mutate(data, rng)
        case = Case(
            case_id=f"state-{plugin['id']}-{i:04d}",
            seed=args.seed_start + i,
            profile="state",
            xml_bytes=case_data,
            expect_audio=False,
            strict_audio=False,
            plugins=[plugin],
            effects=[plugin] if args.kind == "effect" else [],
            notes=1,
        )
        result = run_case(
            case,
            binary=binary,
            work_dir=work,
            timeout=args.timeout,
            keep_wav=False,
        )
        if result.status not in ("pass", "fail"):
            failures += 1
        print(
            f"{i:4d} {result.status:6s} {result.wall_s:6.2f}s "
            f"rss={result.max_rss_kb}KB {result.problems}",
            flush=True,
        )

    print(json.dumps({"cases": args.cases, "hard_failures": failures}, indent=2))
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
