"""Minimal project fixtures for real plugins and effects."""

from __future__ import annotations

import random
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

STRESS_ROOT = Path(__file__).resolve().parents[1]
if str(STRESS_ROOT) not in sys.path:
    sys.path.insert(0, str(STRESS_ROOT))

from generator import (  # noqa: E402
    REPO_ROOT,
    _effect_xml,
    _inject_plugins,
    _load_seed,
    _write_tree,
)

FIXTURE_ROOT = Path(__file__).resolve().parent / "fixtures"
DEFAULT_SEED = REPO_ROOT / "data/projects/shorties/Crunk(Demo).mmp"


def fixture_path(plugin: dict, kind: str) -> Path:
    fmt = plugin.get("format", "unknown")
    slug = plugin.get("id", "unnamed").replace("/", "_")
    return FIXTURE_ROOT / kind / fmt / f"{slug}.mmp"


def _ladspa_effect_xml(effect: dict) -> ET.Element:
    elem = ET.Element(
        "effect",
        {
            "name": "ladspaeffect",
            "on": "1",
            "wet": "1",
            "autoquit": "1",
        },
    )
    ET.SubElement(elem, "ladspacontrols", {"ports": "0"})
    key = ET.SubElement(elem, "key")
    # LMMS stores the library file name (not the full path) plus the
    # descriptor label. The library is found through LADSPA_PATH.
    ET.SubElement(
        key,
        "attribute",
        {"name": "file", "value": effect.get("file", Path(effect["path"]).name)},
    )
    ET.SubElement(
        key,
        "attribute",
        {"name": "plugin", "value": effect.get("label", Path(effect["path"]).stem)},
    )
    return elem


def _fasten_tempo(root: ET.Element) -> None:
    head = root.find("head")
    if head is not None:
        head.set("bpm", "600")


def instrument_fixture(plugin: dict, seed: Path | None = None) -> bytes:
    tree = _load_seed(seed or DEFAULT_SEED)
    root = tree.getroot()
    injected = _inject_plugins(root, random.Random(0), [plugin])
    if not injected:
        raise RuntimeError(f"no instrument track found for {plugin.get('id')}")
    _fasten_tempo(root)
    return _write_tree(root, False, random.Random(0))


def effect_fixture(effect: dict, seed: Path | None = None) -> bytes:
    if effect.get("format") == "carla":
        local = Path(effect["fixture"]) if effect.get("fixture") else fixture_path(
            effect, "effects"
        )
        if not local.exists():
            raise FileNotFoundError(
                f"Carla fixture missing: {local} (create it once via the GUI)"
            )
        return local.read_bytes()

    tree = _load_seed(seed or DEFAULT_SEED)
    root = tree.getroot()
    chains = [chain for chain in root.iter("fxchain")]
    if not chains:
        raise RuntimeError("seed project has no fxchain")
    chain = chains[0]
    for child in list(chain):
        if child.tag == "effect":
            chain.remove(child)
    chain.set("enabled", "1")
    chain.set("numofeffects", "1")
    if effect.get("format") == "ladspa":
        chain.append(_ladspa_effect_xml(effect))
    elif effect.get("format") in ("vst2", "vst3", "lv2"):
        chain.append(_effect_xml(effect))
    else:
        raise ValueError(f"unsupported effect format {effect.get('format')}")
    _fasten_tempo(root)
    return _write_tree(root, False, random.Random(0))


def write_fixture(plugin: dict, kind: str, data: bytes | None = None) -> Path:
    path = fixture_path(plugin, kind)
    path.parent.mkdir(parents=True, exist_ok=True)
    if data is None:
        data = instrument_fixture(plugin) if kind == "instruments" else effect_fixture(plugin)
    path.write_bytes(data)
    return path


def load_or_create(plugin: dict, kind: str) -> bytes:
    if plugin.get("fixture"):
        return Path(plugin["fixture"]).read_bytes()
    path = fixture_path(plugin, kind)
    if path.exists():
        return path.read_bytes()
    return instrument_fixture(plugin) if kind == "instruments" else effect_fixture(plugin)


def clear_local_fixtures() -> None:
    """Remove generated fixtures, but never GUI-saved Carla racks."""

    if not FIXTURE_ROOT.exists():
        return
    carla_root = FIXTURE_ROOT / "effects" / "carla"
    for child in sorted(FIXTURE_ROOT.rglob("*.mmp")):
        if carla_root in child.parents:
            continue
        child.unlink()
