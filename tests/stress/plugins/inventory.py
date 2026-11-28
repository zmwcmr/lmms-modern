"""Plugin inventory helpers for the local LMMS stress harness.

The inventory is intentionally local: plugin binaries and state files are
never copied into the repository.  A machine-specific inventory is generated
from an allowlist of plugin directories and can be overridden with
environment variables.
"""

from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parents[3]
STRESS_ROOT = Path(__file__).resolve().parents[1]
PLUGIN_ROOT = Path(__file__).resolve().parent
# The generated inventory is machine-specific and intentionally not tracked.
DEFAULT_INVENTORY = PLUGIN_ROOT / "inventory.generated.json"
# Older revisions stored render results in this file; keep reading it for
# compatibility without overwriting the recorded evidence.
LEGACY_INVENTORY = PLUGIN_ROOT / "inventory.local.json"
ALLOWLIST_ROOT = Path("/tmp/opencode/lmms-plugins")


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _sha256_tree(path: Path) -> str:
    if path.is_file():
        return _sha256_file(path)
    digest = hashlib.sha256()
    for child in sorted(p for p in path.rglob("*") if p.is_file()):
        digest.update(child.relative_to(path).as_posix().encode())
        digest.update(b"\0")
        digest.update(_sha256_file(child).encode())
        digest.update(b"\0")
    return digest.hexdigest()


def _stat_key(path: Path) -> dict:
    try:
        st = path.stat()
    except OSError:
        return {}
    return {"size": st.st_size, "mtime_ns": st.st_mtime_ns}


def default_plugins() -> list[dict]:
    """Built-in/known plugins used by the previous stress harness."""

    home = Path.home()
    return [
        {
            "id": "dexed",
            "format": "vst3",
            "name": "Dexed",
            "path": str(home / ".vst3/Dexed.vst3"),
            "class": "abcdef019182faeb4447534244657864",
            "role": "instrument",
        },
        {
            "id": "surge-xt",
            "format": "lv2",
            "name": "Surge XT",
            "path": str(home / ".lv2/Surge XT.lv2"),
            "uri": "https://surge-synthesizer.github.io/lv2/surge-xt",
            "role": "instrument",
        },
        {
            "id": "surge-xt-vst3",
            "format": "vst3",
            "name": "Surge XT",
            "path": str(home / ".vst3/Surge XT.vst3"),
            "class": "abcdef019182faeb566d624153675854",
            "role": "instrument",
        },
        {
            "id": "ob-xf-vst3",
            "format": "vst3",
            "name": "OB-Xf",
            "path": str(home / ".vst3/OB-Xf.vst3"),
            "class": "abcdef019182faeb535354784f425846",
            "role": "instrument",
        },
        {
            "id": "sfizz-vst3",
            "format": "vst3",
            "name": "sfizz",
            "path": str(home / ".vst3/sfizz.vst3"),
            "class": "e8fab71815ed46e38b5983101e12993f",
            "role": "instrument",
            "requires_content": True,
        },
        {
            "id": "zyn-add-sub-fx",
            "format": "vst2",
            "name": "ZynAddSubFX",
            "path": str(home / ".vst/ZynAddSubFX.so"),
            "role": "instrument",
        },
        {
            "id": "pianoteq-vst2",
            "format": "vst2",
            "name": "Pianoteq 8 STAGE",
            "path": str(home / ".vst/Pianoteq 8 STAGE.so"),
            "role": "instrument",
            "requires_content": True,
        },
        {
            "id": "surge-xt-effects-vst3",
            "format": "vst3",
            "name": "Surge XT Effects",
            "path": str(home / ".vst3/Surge XT Effects.vst3"),
            "class": "abcdef019182faeb566d624153465854",
            "role": "effect",
        },
        {
            "id": "zyn-chorus-vst2",
            "format": "vst2",
            "name": "ZynChorus",
            "path": str(home / ".vst/ZynChorus.so"),
            "role": "effect",
        },
        {
            "id": "surge-xt-effects-lv2",
            "format": "lv2",
            "name": "Surge XT Effects",
            "path": str(home / ".lv2/Surge XT Effects.lv2"),
            "uri": "https://surge-synth-team.org//plugins/Surge_XT_Effects",
            "role": "effect",
        },
    ]


def _ladspa_labels(path: Path) -> list[str]:
    """Read LADSPA descriptor labels from a shared library.

    No plugin code is run and no audio is processed: only the descriptor
    function is queried, exactly like LMMS does while scanning.
    """

    import ctypes

    class Descriptor(ctypes.Structure):
        _fields_ = [
            ("UniqueID", ctypes.c_ulong),
            ("Label", ctypes.c_char_p),
        ]

    try:
        library = ctypes.CDLL(str(path))
        descriptor = library.ladspa_descriptor
    except (OSError, AttributeError):
        return []
    descriptor.restype = ctypes.POINTER(Descriptor)
    descriptor.argtypes = [ctypes.c_ulong]
    labels: list[str] = []
    for index in range(4096):
        entry = descriptor(index)
        if not entry:
            break
        label = entry.contents.Label
        if label:
            labels.append(label.decode("utf-8", errors="replace"))
    return labels


def discover_ladspa() -> list[dict]:
    """Find LADSPA libraries in the standard search directories."""

    roots = [
        Path.home() / ".ladspa",
        Path("/usr/lib/ladspa"),
        Path("/usr/lib64/ladspa"),
        Path("/usr/local/lib/ladspa"),
        Path("/usr/local/lib64/ladspa"),
    ]
    found: list[dict] = []
    seen: set[str] = set()
    for root in roots:
        if not root.is_dir():
            continue
        for path in sorted(root.glob("*.so")):
            key = str(path.resolve())
            if key in seen:
                continue
            seen.add(key)
            labels = _ladspa_labels(path)
            if not labels:
                labels = [path.stem]
            for label in labels:
                found.append(
                    {
                        "id": "ladspa-" + path.stem + "-" + label,
                        "format": "ladspa",
                        "name": label,
                        "label": label,
                        "file": path.name,
                        "path": str(path),
                        "role": "effect",
                    }
                )
    return found


def discover_carla() -> list[dict]:
    """Discover GUI-saved Carla rack fixtures.

    Carla rack state cannot be synthesized reliably; one project per rack has
    to be saved through the LMMS GUI first. Until that fixture exists the
    plugin is simply absent from the inventory.
    """

    fixture_root = Path(__file__).resolve().parent / "fixtures" / "effects" / "carla"
    if not fixture_root.is_dir():
        return []
    return [
        {
            "id": "carla-" + path.stem,
            "format": "carla",
            "name": path.stem,
            "path": str(path),
            "fixture": str(path),
            "role": "effect",
        }
        for path in sorted(fixture_root.glob("*.mmp"))
    ]


def discover(include_optional: bool = False) -> list[dict]:
    plugins = default_plugins() + discover_ladspa() + discover_carla()
    for plugin in plugins:
        path = Path(plugin["path"])
        plugin["exists"] = path.exists()
        plugin.setdefault("requires_content", False)
        plugin.setdefault("requires_license", False)
        plugin.setdefault("produces_audio", plugin.get("role") == "instrument")
        plugin.update(_stat_key(path))
        if path.exists():
            try:
                plugin["sha256"] = _sha256_tree(path)
            except OSError:
                plugin["sha256"] = ""
        else:
            plugin["sha256"] = ""
    if include_optional:
        return [p for p in plugins if p["exists"]]
    return [
        p
        for p in plugins
        if p["exists"] and not p.get("requires_content") and not p.get("requires_license")
    ]


def by_role(plugins: Iterable[dict], role: str) -> list[dict]:
    return [p for p in plugins if p.get("role") == role]


def by_id(plugins: Iterable[dict], plugin_id: str) -> dict:
    for plugin in plugins:
        if plugin.get("id") == plugin_id:
            return plugin
    raise KeyError(plugin_id)


def write_inventory(plugins: list[dict], path: Path = DEFAULT_INVENTORY) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(
            {
                "generated_by": "tests/stress/plugins/inventory.py",
                "formats": ["vst2", "vst3", "lv2", "ladspa", "carla"],
                "plugins": plugins,
            },
            indent=2,
        ),
        encoding="utf-8",
    )


def _read_inventory(path: Path) -> list[dict]:
    if not path.exists():
        return []
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict):
        plugins = data.get("plugins")
        if plugins is None and isinstance(data.get("results"), list):
            # Backwards compatibility with the first probe runner, which
            # stored render results instead of a plugin inventory.
            plugins = [
                entry["plugin"]
                for entry in data["results"]
                if isinstance(entry, dict) and isinstance(entry.get("plugin"), dict)
            ]
    else:
        plugins = data
    return list(plugins) if plugins else []


def load_inventory(path: Path = DEFAULT_INVENTORY) -> list[dict]:
    plugins = _read_inventory(path)
    if not plugins and path == DEFAULT_INVENTORY:
        plugins = _read_inventory(LEGACY_INVENTORY)
    if plugins:
        return plugins
    return discover(include_optional=True)


def make_allowlist(plugins: list[dict], root: Path = ALLOWLIST_ROOT) -> dict:
    """Create per-format symlink directories for reproducible scans."""

    if root.exists():
        shutil.rmtree(root)
    dirs = {}
    for fmt in ("vst", "vst2", "vst3", "lv2", "ladspa"):
        target = root / fmt
        target.mkdir(parents=True, exist_ok=True)
        dirs[fmt] = target
    for plugin in plugins:
        fmt = plugin["format"]
        if fmt == "vst2":
            fmt = "vst"
        target_dir = dirs.get(fmt)
        source = Path(plugin["path"])
        if target_dir is None or not source.exists():
            continue
        link = target_dir / source.name
        if link.exists() or link.is_symlink():
            continue
        try:
            link.symlink_to(source)
        except OSError:
            pass
    return {key: str(value) for key, value in dirs.items()}


def plugin_env(allowlist: dict | None = None) -> dict[str, str]:
    env = {}
    if allowlist is None:
        return env
    env["VST_PATH"] = allowlist.get("vst", "")
    env["VST2_PATH"] = allowlist.get("vst", "")
    env["VST3_PATH"] = allowlist.get("vst3", "")
    env["LV2_PATH"] = ":".join(
        [allowlist.get("lv2", ""), "/usr/lib/lv2", "/usr/local/lib/lv2"]
    )
    env["LADSPA_PATH"] = allowlist.get("ladspa", "")
    return env
