#!/usr/bin/env python3
"""Regenerate the native Windows and macOS app icons using Inkscape and icotool."""

import struct
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory


def main():
    repo = Path(__file__).resolve().parents[1]
    source = repo / "cmake/linux/icons/scalable/apps/lmms.svg"
    windows_sizes = (16, 24, 32, 48, 64, 96, 128, 256)
    macos_sizes = ((b"ic07", 128), (b"ic08", 256), (b"ic09", 512))

    with TemporaryDirectory() as temporary:
        folder = Path(temporary)
        frames = {}
        for size in sorted(set(windows_sizes) | {size for _, size in macos_sizes}):
            frames[size] = folder / f"{size}.png"
            subprocess.run(
                [
                    "inkscape", str(source), f"--export-width={size}", f"--export-height={size}",
                    f"--export-filename={frames[size]}",
                ],
                check=True,
            )
        windows_icon = folder / "icon.ico"
        subprocess.run(
            [
                "icotool", "--create", "--bit-depth=32", f"--output={windows_icon}",
                f"--raw={frames[256]}", *(str(frames[size]) for size in windows_sizes[:-1]),
            ],
            check=True,
        )

        # Each ICNS block contains a four-byte type, a big-endian length, and a PNG.
        macos_blocks = []
        for tag, size in macos_sizes:
            png = frames[size].read_bytes()
            macos_blocks.append(struct.pack(">4sI", tag, 8 + len(png)) + png)
        payload = b"".join(macos_blocks)
        macos_icon = struct.pack(">4sI", b"icns", 8 + len(payload)) + payload

        (repo / "cmake/nsis/icon.ico").write_bytes(windows_icon.read_bytes())
        (repo / "cmake/apple/icon.icns").write_bytes(macos_icon)


if __name__ == "__main__":
    main()
