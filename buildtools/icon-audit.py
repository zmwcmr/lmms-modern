#!/usr/bin/env python3
"""Audit, lint and preview LMMS theme icons.

The audit and lint commands only need the Python standard library. The sheet
command additionally needs Inkscape and Pillow; it renders previews at the
requested sizes so 16 px legibility can be judged without opening every file.

Examples:
    buildtools/icon-audit.py audit --theme default
    buildtools/icon-audit.py lint data/themes/default/save.svg
    buildtools/icon-audit.py lint buildtools/icon-pilot/*.svg
    buildtools/icon-audit.py sheet --theme default --only play,record,save
"""

import argparse
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
THEMES = REPO / "data" / "themes"

SVG_NS = "{http://www.w3.org/2000/svg}"
DRAWABLE = {"path", "rect", "circle", "ellipse", "line", "polygon", "polyline", "use"}
FORBIDDEN = {
    "text", "image", "filter", "lineargradient", "radialgradient", "mask",
    "pattern", "foreignobject", "style", "script",
}
ALLOWED_COLORS = {"none", "currentcolor", "#ffffff", "#ff4757"}
OPACITY_ATTRS = ("opacity", "fill-opacity", "stroke-opacity")


def local_name(tag):
    return tag.rsplit("}", 1)[-1].lower()


def svg_files(path):
    path = Path(path)
    if path.is_dir():
        return sorted(path.rglob("*.svg"))
    return [path]


def parse_svg(path):
    root = ET.parse(path).getroot()
    if local_name(root.tag) != "svg":
        raise ValueError("root element is not <svg>")
    return root


def parse_length(value):
    if value is None:
        return None
    value = value.strip()
    match = re.match(r"^([0-9.+-]+)\s*(mm|cm|in|px|pt|pc)?$", value)
    if not match:
        return None
    number = float(match.group(1))
    unit = match.group(2) or "px"
    factor = {
        "px": 1.0, "pt": 96.0 / 72.0, "pc": 16.0,
        "mm": 96.0 / 25.4, "cm": 96.0 / 2.54, "in": 96.0,
    }[unit]
    return round(number * factor, 2)


def nominal_size(root):
    width = parse_length(root.get("width"))
    height = parse_length(root.get("height"))
    if width is not None and height is not None:
        return (width, height)
    box = parse_viewbox(root)
    if box:
        return (round(box[2], 2), round(box[3], 2))
    return None


def parse_viewbox(root):
    value = root.get("viewBox")
    if not value:
        return None
    numbers = [float(n) for n in re.findall(r"[-+]?[0-9]*\.?[0-9]+", value)]
    return tuple(numbers) if len(numbers) == 4 else None


def drawables(root):
    return [el for el in root.iter() if local_name(el.tag) in DRAWABLE]


def style_values(style):
    values = {}
    for part in (style or "").split(";"):
        if ":" not in part:
            continue
        key, value = part.split(":", 1)
        values[key.strip().lower()] = value.strip()
    return values


def lint_file(path, strict=True):
    errors = []
    try:
        root = parse_svg(path)
    except (ET.ParseError, ValueError) as error:
        return [f"{path}: cannot parse SVG: {error}"]

    if strict:
        if root.get("width") is not None or root.get("height") is not None:
            errors.append(f"{path}: root must not set width/height; use viewBox only")
        box = parse_viewbox(root)
        if box is None:
            errors.append(f"{path}: missing or invalid viewBox")
        elif box != (0.0, 0.0, 24.0, 24.0):
            errors.append(f"{path}: viewBox must be '0 0 24 24', got {root.get('viewBox')!r}")

    if not drawables(root):
        errors.append(f"{path}: no drawable elements")

    for el in root.iter():
        tag = local_name(el.tag)
        if tag in FORBIDDEN:
            errors.append(f"{path}: forbidden element <{tag}>")
        for attr in ("fill", "stroke", "stop-color"):
            value = el.get(attr)
            if value and value.strip().lower() not in ALLOWED_COLORS:
                errors.append(f"{path}: hard-coded {attr}={value!r}")
        values = style_values(el.get("style"))
        for attr in ("fill", "stroke", "stop-color"):
            value = values.get(attr)
            if value and value.strip().lower() not in ALLOWED_COLORS:
                errors.append(f"{path}: hard-coded style {attr}={value!r}")
        for attr in OPACITY_ATTRS:
            for value in (el.get(attr), values.get(attr)):
                if value is None:
                    continue
                try:
                    number = float(value)
                except ValueError:
                    errors.append(f"{path}: invalid {attr}={value!r}")
                    continue
                if strict and abs(number - 1.0) > 1e-6:
                    errors.append(f"{path}: {attr}={value!r}; use one-ink shapes and negative space")
        if tag == "path" and not (el.get("d") or "").strip():
            errors.append(f"{path}: empty <path>")

    return errors


def read_text(paths):
    chunks = []
    for path in paths:
        try:
            chunks.append(path.read_text(encoding="utf-8", errors="ignore"))
        except OSError:
            pass
    return "\n".join(chunks)


def source_references(theme):
    paths = []
    for folder in ("src", "include", "plugins"):
        base = REPO / folder
        if base.exists():
            paths.extend(p for p in base.rglob("*") if p.suffix in {".cpp", ".h", ".css"})
    for style in THEMES.glob("*/style.css"):
        paths.append(style)
    text = read_text(paths)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)

    referenced = set(re.findall(r'getIconPixmap\(\s*"([^"]+)"', text))
    for name in re.findall(r'resources:([A-Za-z0-9_.-]+)', text):
        referenced.add(re.sub(r"\.(svg|png|css)$", "", name, flags=re.IGNORECASE))
    referenced.discard("style")
    referenced.discard("background_artwork")
    return referenced


def plugin_assets():
    names = set()
    base = REPO / "plugins"
    if base.exists():
        for path in base.rglob("*"):
            if path.suffix.lower() in {".png", ".svg"}:
                names.add(path.stem)
    return names


def theme_names(theme):
    folder = THEMES / theme
    if not folder.is_dir():
        return set()
    return {p.stem for p in folder.iterdir() if p.suffix.lower() in {".svg", ".png"}}


def audit(theme):
    folder = THEMES / theme
    if not folder.is_dir():
        print(f"error: theme not found: {folder}", file=sys.stderr)
        return 1

    files = sorted(folder.glob("*.svg"))
    png_files = sorted(folder.glob("*.png"))
    if not files:
        print(f"Theme: {theme}")
        print(f"  PNG files: {len(png_files)} (SVG audit skipped)")
        return 0

    viewboxes = Counter()
    sizes = Counter()
    colors = Counter()
    forbidden = Counter()
    empty = []
    missing_viewbox = []
    with_size = []
    with_opacity = []
    hashes = defaultdict(list)

    for path in files:
        try:
            root = parse_svg(path)
        except (ET.ParseError, ValueError) as error:
            print(f"  parse error: {path.name}: {error}")
            continue

        box = parse_viewbox(root)
        viewboxes[root.get("viewBox", "missing")] += 1
        if box is None:
            missing_viewbox.append(path.name)
        sizes[nominal_size(root)] += 1
        if root.get("width") is not None or root.get("height") is not None:
            with_size.append(path.name)
        if not drawables(root):
            empty.append(path.name)

        for el in root.iter():
            tag = local_name(el.tag)
            if tag in FORBIDDEN:
                forbidden[tag] += 1
            for attr in ("fill", "stroke", "stop-color"):
                value = el.get(attr)
                if value and (value.startswith("#") or value.lower().startswith("rgb")):
                    colors[value.lower()] += 1
            for attr in OPACITY_ATTRS:
                value = el.get(attr)
                if value is not None:
                    try:
                        if abs(float(value) - 1.0) > 1e-6:
                            with_opacity.append(path.name)
                            break
                    except ValueError:
                        with_opacity.append(path.name)
                        break

        hashes[hashlib.md5(path.read_bytes()).hexdigest()].append(path.name)

    duplicates = [names for names in hashes.values() if len(names) > 1]
    names = {p.stem for p in files}
    referenced = source_references(theme)
    plugin = plugin_assets()
    missing = sorted(referenced - names - plugin)
    unused = sorted(names - referenced)

    print(f"Theme: {theme}")
    print(f"  SVG files: {len(files)}")
    print(f"  PNG files: {len(png_files)} (not audited)")
    print(f"  empty: {len(empty)}" + (f"  {empty[:12]}" if empty else ""))
    print(f"  no viewBox: {len(missing_viewbox)}" + (f"  {missing_viewbox[:12]}" if missing_viewbox else ""))
    print(f"  root width/height: {len(with_size)}")
    print(f"  viewBox values: {len(viewboxes)} distinct")
    for value, count in viewboxes.most_common(8):
        print(f"    {value!r}: {count}")
    print(f"  nominal sizes: {len(sizes)} distinct")
    for value, count in sizes.most_common(10):
        print(f"    {value}: {count}")
    print("  forbidden elements: " + (", ".join(f"{k}={v}" for k, v in forbidden.most_common()) or "none"))
    print(f"  files with opacity < 1: {len(set(with_opacity))}")
    print(f"  hard-coded fill/stroke colors: {len(colors)} distinct")
    for value, count in colors.most_common(8):
        print(f"    {value}: {count}")
    print(f"  exact duplicate groups: {len(duplicates)}")
    for group in duplicates:
        print(f"    {group}")

    print(f"  literal references not in this theme: {len(missing)}")
    if missing:
        print("    " + ", ".join(missing[:30]) + (" ..." if len(missing) > 30 else ""))
    print(f"  theme icons not referenced literally: {len(unused)}")
    if unused:
        print("    " + ", ".join(unused[:30]) + (" ..." if len(unused) > 30 else ""))
        print("    note: names built dynamically (note_*, lcd_*) may appear here.")

    for other in ("default", "classic", "legacy"):
        if other == theme:
            continue
        other_names = theme_names(other)
        if not other_names:
            continue
        print(f"  vs {other}: {other}-only {len(other_names - names)}, {theme}-only {len(names - other_names)}")

    return 0


def lint(paths, baseline):
    baseline_paths = set()
    if baseline:
        base = Path(baseline)
        if not base.is_file():
            print(f"error: baseline not found: {base}", file=sys.stderr)
            return 1
        for line in base.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                baseline_paths.add((REPO / line).resolve())

    total = 0
    errors = 0
    for raw in paths:
        path = Path(raw)
        if not path.is_absolute():
            path = (Path.cwd() / path)
        for file in svg_files(path):
            total += 1
            if file.resolve() in baseline_paths:
                continue
            messages = lint_file(file)
            errors += len(messages)
            for message in messages:
                print(message)

    print(f"lint: {total} files, {errors} errors")
    return 1 if errors else 0


def find_font():
    for candidate in (
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf",
    ):
        if Path(candidate).is_file():
            return candidate
    return None


def render_icon(source, destination, size, ink):
    text = source.read_text(encoding="utf-8", errors="ignore")
    text = text.replace("#FFFFFF", ink).replace("#ffffff", ink)
    text = text.replace("currentColor", ink)
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False, encoding="utf-8") as handle:
        handle.write(text)
        temporary = Path(handle.name)
    try:
        subprocess.run(
            [
                "inkscape", str(temporary), "--export-type=png",
                f"--export-filename={destination}",
                f"--export-width={size}", f"--export-height={size}",
                "--export-background-opacity=0",
            ],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
    finally:
        temporary.unlink(missing_ok=True)


def sheet(args):
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print("error: the sheet command needs Pillow (pip install pillow)", file=sys.stderr)
        return 2
    if shutil.which("inkscape") is None:
        print("error: the sheet command needs Inkscape on PATH", file=sys.stderr)
        return 2

    folder = THEMES / args.theme
    if not folder.is_dir():
        print(f"error: theme not found: {folder}", file=sys.stderr)
        return 1

    sizes = [int(value) for value in args.sizes.split(",") if value.strip()]
    only = {value.strip() for value in args.only.split(",") if value.strip()}
    files = sorted(folder.glob("*.svg"))
    if only:
        files = [path for path in files if path.stem in only]
    if not files:
        print("error: no icons selected", file=sys.stderr)
        return 1

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    font_path = find_font()
    font = ImageFont.truetype(font_path, 11) if font_path else ImageFont.load_default()
    title_font = ImageFont.truetype(font_path, 16) if font_path else ImageFont.load_default()

    backgrounds = (
        ("dark", (16, 18, 19), "#e6e9ef", (230, 233, 239)),
        ("light", (245, 246, 248), "#1a1b1e", (26, 27, 30)),
    )

    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        for label, background, ink, foreground in backgrounds:
            for size in sizes:
                scale = 4 if size <= 24 else 2
                cell = size * scale
                columns = 8
                rows = (len(files) + columns - 1) // columns
                width = columns * (cell + 16) + 16
                height = 48 + rows * (cell + 34)
                image = Image.new("RGB", (width, height), background)
                draw = ImageDraw.Draw(image)
                draw.text((16, 14), f"{args.theme} · {size} px · {label}", font=title_font, fill=foreground)

                for index, source in enumerate(files):
                    rendered = temporary / f"{source.stem}-{size}-{label}.png"
                    render_icon(source, rendered, size, ink)
                    icon = Image.open(rendered).convert("RGBA")
                    icon = icon.resize((cell, cell), Image.NEAREST)
                    column = index % columns
                    row = index // columns
                    x = 16 + column * (cell + 16)
                    y = 48 + row * (cell + 34)
                    image.paste(icon, (x, y), icon)
                    draw.text((x, y + cell + 4), source.stem, font=font, fill=foreground)

                destination = out / f"{args.theme}-{size}-{label}.png"
                image.save(destination)
                print(destination)

    return 0


def migration_items(path):
    items = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if not line.startswith("| "):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != 5:
            continue
        group, name, _old, _new, _note = cells
        match = re.fullmatch(r"`([a-z0-9_]+)`", name)
        if match:
            items.append((group, match.group(1)))
    return items


def compare(args):
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print("error: the compare command needs Pillow (pip install pillow)", file=sys.stderr)
        return 2
    if shutil.which("inkscape") is None:
        print("error: the compare command needs Inkscape on PATH", file=sys.stderr)
        return 2

    items = migration_items(args.doc)
    if not items:
        print(f"error: no icon rows in {args.doc}", file=sys.stderr)
        return 1

    old_dir = Path(args.old)
    new_dir = Path(args.new)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    font_path = find_font()
    font = ImageFont.truetype(font_path, 12) if font_path else ImageFont.load_default()
    group_font = ImageFont.truetype(font_path, 14) if font_path else ImageFont.load_default()
    title_font = ImageFont.truetype(font_path, 18) if font_path else ImageFont.load_default()
    group_labels = {
        "传输与常用": "Transport",
        "文件与浏览": "Files",
        "轨道与编辑器": "Tracks & editors",
        "侧栏与状态": "Sidebar & state",
        "设置页": "Setup",
        "乐器页签": "Instrument tabs",
        "混音/控制器": "Mixer & controllers",
        "状态": "Status",
    }

    size = args.size
    icon_px = size * 2
    columns = 3
    block_w = 230
    row_h = 96
    header_h = 30
    margin = 22
    top = 54
    ink = "#e6e9ef"
    bg = (16, 18, 19)
    fg = (230, 233, 239)

    layout = []
    y = top
    column = 0
    last_group = None
    for group, name in items:
        if group != last_group:
            if column:
                y += row_h
                column = 0
            layout.append(("group", group, margin, y))
            y += header_h
            last_group = group
        layout.append(("item", name, margin + column * block_w, y))
        column += 1
        if column == columns:
            column = 0
            y += row_h
    if column:
        y += row_h
    height = y + margin
    width = margin * 2 + columns * block_w

    image = Image.new("RGB", (width, height), bg)
    draw = ImageDraw.Draw(image)
    draw.text((margin, 16), "Icon migration · legacy (left) -> default (right)", font=title_font, fill=fg)

    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        for kind, value, x, y in layout:
            if kind == "group":
                draw.text((x, y + 2), group_labels.get(value, value), font=group_font, fill=(150, 158, 170))
                continue
            old_path = old_dir / f"{value}.svg"
            new_path = new_dir / f"{value}.svg"
            old_png = temporary / f"{value}-old.png"
            new_png = temporary / f"{value}-new.png"
            if old_path.is_file():
                render_icon(old_path, old_png, icon_px, ink)
            if new_path.is_file():
                render_icon(new_path, new_png, icon_px, ink)
            if old_path.is_file():
                icon = Image.open(old_png).convert("RGBA").resize((size, size), Image.LANCZOS)
                image.paste(icon, (x + 20, y + 8), icon)
            else:
                draw.text((x + 30, y + 20), "—", font=font, fill=fg)
            arrow_y = y + 8 + size // 2
            draw.line([(x + 72, arrow_y), (x + 100, arrow_y)], fill=(120, 128, 140), width=2)
            draw.polygon([(x + 100, arrow_y - 4), (x + 108, arrow_y), (x + 100, arrow_y + 4)],
                         fill=(120, 128, 140))
            if new_path.is_file():
                icon = Image.open(new_png).convert("RGBA").resize((size, size), Image.LANCZOS)
                image.paste(icon, (x + 112, y + 8), icon)
            else:
                draw.text((x + 122, y + 20), "—", font=font, fill=fg)
            draw.text((x + 20, y + size + 14), value, font=font, fill=fg)

    image.save(out)
    print(out)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)

    audit_parser = sub.add_parser("audit", help="summarise a theme and its usage")
    audit_parser.add_argument("--theme", default="default")

    lint_parser = sub.add_parser("lint", help="check SVG files against the icon spec")
    lint_parser.add_argument("paths", nargs="+", help="SVG files or directories")
    lint_parser.add_argument("--baseline", help="file listing legacy paths to skip")

    sheet_parser = sub.add_parser("sheet", help="render contact sheets (Inkscape + Pillow)")
    sheet_parser.add_argument("--theme", default="default")
    sheet_parser.add_argument("--sizes", default="16,24,48")
    sheet_parser.add_argument("--only", default="", help="comma-separated icon names")
    sheet_parser.add_argument("--out", default="build/icon-sheets")

    compare_parser = sub.add_parser("compare", help="render old/new icon comparison (Inkscape + Pillow)")
    compare_parser.add_argument("--doc", default="doc/icon-migration.md")
    compare_parser.add_argument("--old", default="data/themes/legacy")
    compare_parser.add_argument("--new", default="data/themes/default")
    compare_parser.add_argument("--out", default="doc/ui-ux-shots/icon-migration.png")
    compare_parser.add_argument("--size", type=int, default=48)

    args = parser.parse_args()
    if args.command == "audit":
        return audit(args.theme)
    if args.command == "lint":
        return lint(args.paths, args.baseline)
    if args.command == "sheet":
        return sheet(args)
    if args.command == "compare":
        return compare(args)
    return 1


if __name__ == "__main__":
    sys.exit(main())
