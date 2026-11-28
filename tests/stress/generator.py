"""Random LMMS project generator / mutator for stress testing.

The first version deliberately starts from real demo projects and applies
structure-aware mutations. This gives semantically valid projects (with
notes, patterns, mixer channels and automation) without re-implementing the
whole project schema.
"""

from __future__ import annotations

import copy
import json
import os
import random
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SEED_DIR = Path(__file__).resolve().parent / "seeds"
FIXTURE_DIR = Path(__file__).resolve().parent.parent / "plugins" / "fixtures"

DEFAULT_SEEDS = [
    REPO_ROOT / "data/projects/shorties/Crunk(Demo).mmp",
    REPO_ROOT / "data/projects/demos/StrictProduction-DearJonDoe.mmp",
    REPO_ROOT / "data/projects/tutorials/editing_note_volumes.mmp",
    REPO_ROOT / "tests/emptyproject.mmp",
]

# Real third-party plugins available on the development machine. Paths can be
# overridden with environment variables for other machines.
PLUGIN_DEFS = [
    {
        "id": "dexed",
        "format": "vst3",
        "name": "Dexed",
        "path": os.environ.get("LMMS_TEST_DEXED", str(Path.home() / ".vst3/Dexed.vst3")),
        "class": "abcdef019182faeb4447534244657864",
    },
    {
        "id": "zyn-add-sub-fx",
        "format": "vst2",
        "name": "ZynAddSubFX",
        "path": os.environ.get("LMMS_TEST_ZYN", str(Path.home() / ".vst/ZynAddSubFX.so")),
    },
    {
        "id": "surge-xt",
        "format": "lv2",
        "name": "Surge XT",
        "path": os.environ.get(
            "LMMS_TEST_SURGE", str(Path.home() / ".lv2/Surge XT.lv2")
        ),
        "uri": "https://surge-synthesizer.github.io/lv2/surge-xt",
    },
]

EFFECT_DEFS = [
    {
        "id": "surge-xt-effects-vst3",
        "format": "vst3",
        "name": "Surge XT Effects",
        "path": str(Path.home() / ".vst3/Surge XT Effects.vst3"),
        "class": "abcdef019182faeb566d624153465854",
    },
    {
        "id": "zyn-chorus-vst2",
        "format": "vst2",
        "name": "ZynChorus",
        "path": str(Path.home() / ".vst/ZynChorus.so"),
    },
    {
        "id": "surge-xt-effects-lv2",
        "format": "lv2",
        "name": "Surge XT Effects",
        "path": str(Path.home() / ".lv2/Surge XT Effects.lv2"),
        "uri": "https://surge-synth-team.org//plugins/Surge_XT_Effects",
    },
]

PROFILES = {
    "tiny": {"mutations": (2, 5), "bpm": (240, 600), "malformed": 0.05, "tracks": 1},
    "small": {"mutations": (5, 12), "bpm": (120, 600), "malformed": 0.08, "tracks": 1},
    "medium": {"mutations": (12, 40), "bpm": (60, 600), "malformed": 0.05, "tracks": 2},
    "large": {"mutations": (40, 80), "bpm": (60, 600), "malformed": 0.02, "tracks": 3},
    "extreme": {"mutations": (80, 150), "bpm": (20, 600), "malformed": 0.0, "tracks": 4},
}


@dataclass
class Case:
    case_id: str
    seed: int
    profile: str
    xml_bytes: bytes
    malformed: bool = False
    expect_audio: bool = True
    strict_audio: bool = False
    plugins: list[dict] = field(default_factory=list)
    effects: list[dict] = field(default_factory=list)
    notes: int = 0
    description: str = ""


def _load_seed(seed_file: Path) -> ET.ElementTree:
    parser = ET.XMLParser(target=ET.TreeBuilder())
    return ET.parse(seed_file, parser=parser)


def _all(root: ET.Element, tag: str) -> list[ET.Element]:
    return list(root.iter(tag))


def _parent_map(root: ET.Element) -> dict[ET.Element, ET.Element]:
    return {child: parent for parent in root.iter() for child in list(parent)}


def _set_attr(rng: random.Random, elem: ET.Element, name: str, value: str) -> None:
    elem.set(name, value)


def _mutate_head(root: ET.Element, rng: random.Random, profile: str) -> None:
    head = root.find("head")
    if head is None:
        return
    choices = [
        "bpm",
        "mastervol",
        "masterpitch",
        "timesig_numerator",
        "timesig_denominator",
    ]
    name = rng.choice(choices)
    cfg = PROFILES[profile]
    if name == "bpm":
        lo, hi = cfg["bpm"]
        if rng.random() < 0.1:
            value = rng.choice([1, 2, 9999, 100000, -120])
        else:
            value = rng.randint(lo, hi)
        head.set("bpm", str(value))
    elif name == "mastervol":
        head.set("mastervol", str(rng.choice([0, 1, 50, 100, 200, 1000, -100])))
    elif name == "masterpitch":
        head.set("masterpitch", str(rng.choice([-24, -12, -1, 0, 1, 12, 24, 128])))
    elif name == "timesig_numerator":
        head.set("timesig_numerator", str(rng.choice([1, 2, 3, 4, 7, 16, 64, 0, -4])))
    else:
        head.set("timesig_denominator", str(rng.choice([1, 2, 4, 8, 16, 32, 64, 0, 3])))


def _mutate_notes(root: ET.Element, rng: random.Random, aggressive: bool) -> None:
    notes = _all(root, "note")
    if not notes:
        return
    for _ in range(1 if not aggressive else rng.randint(1, 8)):
        note = rng.choice(notes)
        choice = rng.randrange(7)
        if choice == 0:
            note.set("key", str(rng.choice([-100, -1, 0, 1, 57, 127, 128, 300, 100000])))
        elif choice == 1:
            note.set("vol", str(rng.choice([-100, 0, 1, 100, 127, 1000])))
        elif choice == 2:
            note.set("pan", str(rng.choice([-1000, -100, 0, 100, 1000])))
        elif choice == 3:
            note.set("pos", str(rng.choice([-100000, -100, 0, 1, 1000, 100000])))
        elif choice == 4:
            note.set("len", str(rng.choice([-100000, -192, -1, 0, 1, 192, 100000])))
        elif choice == 5:
            # Duplicate a note, possibly into an invalid position.
            dup = copy.deepcopy(note)
            dup.set("pos", str(rng.randint(-1000, 100000)))
            parent = None
            for candidate in root.iter():
                if note in list(candidate):
                    parent = candidate
                    break
            if parent is not None:
                parent.append(dup)
                notes.append(dup)
        else:
            # Remove a note (keep at least one if possible).
            if len(notes) > 1:
                parent = None
                for candidate in root.iter():
                    if note in list(candidate):
                        parent = candidate
                        break
                if parent is not None:
                    parent.remove(note)
                    notes.remove(note)


def _mutate_patterns(root: ET.Element, rng: random.Random) -> None:
    patterns = _all(root, "pattern")
    if not patterns:
        return
    pattern = rng.choice(patterns)
    choice = rng.randrange(5)
    if choice == 0:
        pattern.set("len", str(rng.choice([-100000, 0, 1, 64, 192, 100000])))
    elif choice == 1:
        pattern.set("steps", str(rng.choice([0, 1, 4, 16, 64, 1000])))
    elif choice == 2:
        pattern.set("pos", str(rng.choice([-100000, 0, 1, 12345, 100000])))
    elif choice == 3:
        pattern.set("muted", rng.choice(["0", "1"]))
    else:
        # Duplicate the pattern's notes to increase density.
        notes = list(pattern.iter("note"))
        for _ in range(min(16, max(1, len(notes)))):
            if notes:
                dup = copy.deepcopy(rng.choice(notes))
                dup.set("pos", str(rng.randint(0, 10000)))
                pattern.append(dup)


def _mutate_tracks(root: ET.Element, rng: random.Random, max_dupes: int) -> None:
    containers = _all(root, "trackcontainer")
    if not containers:
        return
    container = rng.choice(containers)
    tracks = [c for c in list(container) if c.tag == "track"]
    if not tracks:
        return
    if rng.random() < 0.5 and len(tracks) > 1:
        track = rng.choice(tracks)
        dup = copy.deepcopy(track)
        dup.set("name", (track.get("name") or "track") + "-dup")
        container.append(dup)
    elif len(tracks) <= max_dupes:
        for _ in range(rng.randint(1, 3)):
            track = copy.deepcopy(rng.choice(tracks))
            track.set("name", (track.get("name") or "track") + f"-x{rng.randint(0, 9999)}")
            container.append(track)


def _mutate_mixer(root: ET.Element, rng: random.Random) -> None:
    channels = _all(root, "mixerchannel")
    if not channels:
        return
    channel = rng.choice(channels)
    choice = rng.randrange(4)
    if choice == 0:
        channel.set("volume", str(rng.choice([0, 0.5, 1, 2, 100, -1])))
    elif choice == 1:
        channel.set("pan", str(rng.choice([-1, 0, 1, -100, 100])))
    elif choice == 2:
        channel.set("muted", rng.choice(["0", "1"]))
    else:
        dup = copy.deepcopy(channel)
        dup.set("num", str(rng.randint(0, 10000)))
        dup.set("name", "dup-" + (channel.get("name") or "ch"))
        parent = _parent_map(root).get(channel)
        if parent is not None:
            parent.append(dup)


def _mutate_automation(root: ET.Element, rng: random.Random) -> None:
    times = _all(root, "time")
    if not times:
        return
    t = rng.choice(times)
    t.set("value", str(rng.choice([-100000, -1, 0, 1, 140, 100000])))
    t.set("pos", str(rng.choice([-100000, 0, 1, 100000])))


def _mutate_attributes(root: ET.Element, rng: random.Random) -> None:
    elems = [e for e in root.iter() if e.attrib]
    if not elems:
        return
    elem = rng.choice(elems)
    if rng.random() < 0.5:
        name = "stress_" + str(rng.randint(0, 999))
        elem.set(name, str(rng.choice(["", "0", "1", "nan", "inf", "x" * 1024])))
    else:
        name = rng.choice(list(elem.attrib))
        del elem.attrib[name]


def _plugin_xml(plugin: dict) -> ET.Element:
    if plugin["format"] == "vst3":
        instrument = ET.Element("instrument", {"name": "vst3instrument"})
        key = ET.SubElement(instrument, "key")
        ET.SubElement(key, "attribute", {"name": "file", "value": plugin["path"]})
        ET.SubElement(key, "attribute", {"name": "class", "value": plugin["class"]})
        ET.SubElement(
            instrument,
            "vst3instrument",
            {"file": plugin["path"], "class": plugin["class"]},
        )
        return instrument
    if plugin["format"] == "vst2":
        instrument = ET.Element("instrument", {"name": "vestige"})
        ET.SubElement(instrument, "vestige", {"plugin": plugin["path"]})
        return instrument
    if plugin["format"] == "lv2":
        instrument = ET.Element("instrument", {"name": "lv2instrument"})
        key = ET.SubElement(instrument, "key")
        ET.SubElement(key, "attribute", {"name": "uri", "value": plugin["uri"]})
        ET.SubElement(instrument, "lv2instrument")
        return instrument
    raise ValueError(plugin["format"])


def _effect_xml(effect: dict) -> ET.Element:
    if effect["format"] == "vst3":
        elem = ET.Element(
            "effect",
            {
                "name": "vst3effect",
                "on": "1",
                "wet": "1",
                "autoquit": "1",
                "file": effect["path"],
                "class": effect["class"],
            },
        )
        key = ET.SubElement(elem, "key")
        ET.SubElement(key, "attribute", {"name": "file", "value": effect["path"]})
        ET.SubElement(key, "attribute", {"name": "class", "value": effect["class"]})
        return elem
    if effect["format"] == "vst2":
        elem = ET.Element(
            "effect",
            {
                "name": "vsteffect",
                "on": "1",
                "wet": "1",
                "autoquit": "1",
                "file": effect["path"],
            },
        )
        key = ET.SubElement(elem, "key")
        ET.SubElement(key, "attribute", {"name": "file", "value": effect["path"]})
        return elem
    if effect["format"] == "lv2":
        elem = ET.Element(
            "effect", {"name": "lv2effect", "on": "1", "wet": "1", "autoquit": "1"}
        )
        key = ET.SubElement(elem, "key")
        ET.SubElement(key, "attribute", {"name": "uri", "value": effect["uri"]})
        return elem
    raise ValueError(effect["format"])


def _inject_effects(
    root: ET.Element, rng: random.Random, effects: list[dict]
) -> list[dict]:
    chains = [f for f in root.iter("fxchain")]
    if not chains or not effects:
        return []
    rng.shuffle(chains)
    chain = chains[0]
    effect = rng.choice(effects)
    for child in list(chain):
        if child.tag == "effect":
            chain.remove(child)
    chain.set("enabled", "1")
    chain.set("numofeffects", "1")
    chain.append(_effect_xml(effect))
    return [effect]


def _inject_plugins(
    root: ET.Element, rng: random.Random, plugins: list[dict]
) -> list[dict]:
    """Replace existing instruments with real third-party plugins.

    Only replace instruments in tracks that already contain notes, so the
    resulting project should produce audio with the plugin's default patch.
    """
    if not plugins:
        return []
    candidates: list[tuple[ET.Element, ET.Element]] = []
    for track in root.iter("track"):
        if not list(track.iter("note")):
            continue
        for it in track.iter("instrumenttrack"):
            inst = it.find("instrument")
            if inst is not None:
                candidates.append((it, inst))
    rng.shuffle(candidates)
    injected: list[dict] = []
    for _parent, inst in candidates:
        plugin = rng.choice(plugins)
        new_inst = _plugin_xml(plugin)
        parent = None
        for candidate in root.iter():
            if inst in list(candidate):
                parent = candidate
                break
        if parent is None:
            continue
        idx = list(parent).index(inst)
        parent.remove(inst)
        parent.insert(idx, new_inst)
        injected.append(plugin)
        break
    return injected


def _count_notes(root: ET.Element) -> int:
    return sum(1 for _ in root.iter("note"))


def _master_channel_silent(root: ET.Element) -> bool:
    for channel in root.iter("mixerchannel"):
        if channel.get("num") == "0" or channel.get("name") == "Master":
            if channel.get("muted") == "1":
                return True
            try:
                return float(channel.get("volume", "1")) <= 0.001
            except ValueError:
                return True
    return False


def _has_audible_notes(root: ET.Element) -> bool:
    """Return True if at least one note can reach the master output."""
    parents = _parent_map(root)
    for note in root.iter("note"):
        node = note
        audible = True
        while node is not None:
            if node.tag in ("pattern", "track") and node.get("muted") == "1":
                audible = False
                break
            node = parents.get(node)
        if audible:
            return True
    return False


def _write_tree(root: ET.Element, malformed: bool, rng: random.Random) -> bytes:
    ET.indent(root, space="  ")
    data = ET.tostring(root, encoding="utf-8", xml_declaration=True)
    if malformed:
        kind = rng.randrange(4)
        if kind == 0 and len(data) > 100:
            cut = rng.randint(1, max(1, len(data) // 3))
            data = data[:-cut]
        elif kind == 1:
            # Remove a random closing tag-ish chunk.
            pos = data.rfind(b"</")
            if pos > 0:
                data = data[:pos]
        elif kind == 2:
            data = data.replace(b"<head", b"<head><broken", 1)
        else:
            # Corrupt a random attribute quote.
            pos = data.find(b'="')
            if pos > 0:
                data = data[:pos] + b"==" + data[pos + 1 :]
    return data


def generate_case(
    seed: int,
    profile: str = "tiny",
    plugin_ratio: float = 0.0,
    seed_file: Path | None = None,
    plugins: list[dict] | None = None,
    force_plugin: str | None = None,
) -> Case:
    if profile not in PROFILES:
        raise ValueError(f"unknown profile {profile}")
    cfg = PROFILES[profile]
    rng = random.Random(seed)

    candidates = DEFAULT_SEEDS if seed_file is None else [Path(seed_file)]
    candidates = [p for p in candidates if p.exists()]
    if not candidates:
        raise FileNotFoundError("no seed projects found")
    seed_path = rng.choice(candidates)
    tree = _load_seed(seed_path)
    root = tree.getroot()

    injected: list[dict] = []
    if plugins is None:
        plugins = PLUGIN_DEFS
    if force_plugin:
        forced = [p for p in plugins if p["id"] == force_plugin]
        if not forced:
            raise ValueError(f"unknown plugin {force_plugin}")
        injected = _inject_plugins(root, rng, forced)
        if not injected:
            raise RuntimeError(f"no instrument track found for plugin {force_plugin}")
    elif plugin_ratio > 0 and rng.random() < plugin_ratio:
        injected = _inject_plugins(root, rng, plugins)

    injected_effects: list[dict] = []
    available_effects = [e for e in EFFECT_DEFS if Path(e["path"]).exists()]
    if plugin_ratio > 0 and available_effects and rng.random() < plugin_ratio:
        injected_effects = _inject_effects(root, rng, available_effects)

    # A real plugin needs notes; if the chosen seed had none, fall back to a
    # seed with notes.
    if injected and _count_notes(root) == 0:
        for fallback in candidates:
            tree = _load_seed(fallback)
            root = tree.getroot()
            if _count_notes(root) > 0:
                injected = _inject_plugins(root, rng, plugins)
                break

    mutation_count = rng.randint(*cfg["mutations"])
    for _ in range(mutation_count):
        choice = rng.choices(
            [
                "head",
                "notes",
                "patterns",
                "tracks",
                "mixer",
                "automation",
                "attributes",
                "attributes",
            ],
            weights=[4, 6, 3, 2, 2, 1, 1, 1],
        )[0]
        if choice == "head":
            _mutate_head(root, rng, profile)
        elif choice == "notes":
            _mutate_notes(root, rng, aggressive=profile in ("large", "extreme"))
        elif choice == "patterns":
            _mutate_patterns(root, rng)
        elif choice == "tracks":
            _mutate_tracks(root, rng, cfg["tracks"])
        elif choice == "mixer":
            _mutate_mixer(root, rng)
        elif choice == "automation":
            _mutate_automation(root, rng)
        else:
            _mutate_attributes(root, rng)

    # A missing root type makes the loader silently produce an empty song;
    # keep the project structurally valid while still fuzzing attributes.
    if root.get("type") is None:
        root.set("type", "song")

    # Keep runtime bounded: fast tempos for the small profiles.
    head = root.find("head")
    if head is not None:
        lo, hi = cfg["bpm"]
        try:
            bpm = int(float(head.get("bpm", "140")))
        except ValueError:
            bpm = 140
        if bpm < lo or bpm > hi:
            head.set("bpm", str(rng.randint(lo, hi)))

    notes = _count_notes(root)
    master_volume = 100
    if head is not None:
        try:
            master_volume = int(float(head.get("mastervol", "100")))
        except ValueError:
            master_volume = 100
    audible = (
        notes > 0
        and master_volume >= 10
        and not _master_channel_silent(root)
        and _has_audible_notes(root)
    )
    malformed = rng.random() < cfg["malformed"]
    data = _write_tree(root, malformed, rng)
    case_id = f"{profile}-{seed:06d}"

    return Case(
        case_id=case_id,
        seed=seed,
        profile=profile,
        xml_bytes=data,
        malformed=malformed,
        expect_audio=audible,
        # Mutations may legitimately silence a project (e.g. by removing a
        # clip length). Plugin audio is validated by probe_plugins.py with
        # unmutated fixtures instead.
        strict_audio=False,
        plugins=injected,
        effects=injected_effects,
        notes=notes,
        description=f"seed={seed_path.name} mutations={mutation_count}",
    )
