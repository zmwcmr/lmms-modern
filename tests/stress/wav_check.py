"""WAV parsing and audio health checks for the LMMS stress harness.

Only depends on the Python standard library. Supports PCM 8/16/24/32-bit
and IEEE float 32/64-bit RIFF files.
"""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass, asdict


@dataclass
class WavReport:
    exists: bool = False
    error: str | None = None
    sample_rate: int = 0
    channels: int = 0
    bits: int = 0
    format_tag: int = 0
    frames: int = 0
    duration_s: float = 0.0
    peak: float = 0.0
    rms: float = 0.0
    nan_count: int = 0
    inf_count: int = 0
    clipped_count: int = 0
    silent: bool = True

    def to_dict(self) -> dict:
        return asdict(self)


def _read_chunks(data: bytes):
    if len(data) < 12 or data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file")
    pos = 12
    while pos + 8 <= len(data):
        chunk_id = data[pos : pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        body = pos + 8
        end = body + size
        if end > len(data):
            raise ValueError("truncated chunk")
        yield chunk_id, data[body:end]
        pos = end + (size & 1)


def check_wav(path: str) -> WavReport:
    report = WavReport()
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as exc:
        report.error = str(exc)
        return report

    report.exists = True
    try:
        fmt = None
        raw = None
        for chunk_id, body in _read_chunks(data):
            if chunk_id == b"fmt ":
                fmt = body
            elif chunk_id == b"data":
                raw = body

        if fmt is None or len(fmt) < 16:
            raise ValueError("missing fmt chunk")
        if raw is None:
            raise ValueError("missing data chunk")

        format_tag, channels, sample_rate, _byte_rate, block_align, bits = struct.unpack_from(
            "<HHIIHH", fmt, 0
        )
        report.format_tag = format_tag
        report.channels = channels
        report.sample_rate = sample_rate
        report.bits = bits

        bytes_per_sample = (bits + 7) // 8
        if channels <= 0 or bytes_per_sample <= 0:
            raise ValueError("invalid channel/sample size")
        frame_size = channels * bytes_per_sample
        if frame_size <= 0 or block_align != frame_size:
            raise ValueError("invalid block alignment")
        report.frames = len(raw) // frame_size
        report.duration_s = report.frames / sample_rate if sample_rate else 0.0

        peak = 0.0
        sum_sq = 0.0
        count = 0
        nan_count = 0
        inf_count = 0
        clipped = 0

        # Fast paths for the formats LMMS writes by default.
        if format_tag == 1 and bits == 16:
            samples = struct.unpack("<%dh" % (len(raw) // 2), raw)
            for s in samples:
                v = s / 32768.0
                av = abs(v)
                if av > peak:
                    peak = av
                if av >= 0.999969:
                    clipped += 1
                sum_sq += v * v
            count = len(samples)
        elif format_tag == 1 and bits == 24:
            n = len(raw) // 3
            for i in range(n):
                b = raw[3 * i : 3 * i + 3]
                v = int.from_bytes(b + (b"\xff" if b[2] & 0x80 else b"\x00"), "little", signed=True)
                v /= 8388608.0
                av = abs(v)
                if av > peak:
                    peak = av
                if av >= 0.999969:
                    clipped += 1
                sum_sq += v * v
            count = n
        elif format_tag == 1 and bits == 8:
            samples = struct.unpack("<%dB" % len(raw), raw)
            for s in samples:
                v = (s - 128) / 128.0
                av = abs(v)
                if av > peak:
                    peak = av
                sum_sq += v * v
            count = len(samples)
        elif format_tag == 1 and bits == 32:
            samples = struct.unpack("<%di" % (len(raw) // 4), raw)
            for s in samples:
                v = s / 2147483648.0
                av = abs(v)
                if av > peak:
                    peak = av
                if av >= 0.999969:
                    clipped += 1
                sum_sq += v * v
            count = len(samples)
        elif format_tag == 3 and bits == 32:
            samples = struct.unpack("<%df" % (len(raw) // 4), raw)
            for v in samples:
                if math.isnan(v):
                    nan_count += 1
                    continue
                if math.isinf(v):
                    inf_count += 1
                    continue
                av = abs(v)
                if av > peak:
                    peak = av
                if av >= 0.999969:
                    clipped += 1
                sum_sq += v * v
            count = len(samples)
        elif format_tag == 3 and bits == 64:
            samples = struct.unpack("<%dd" % (len(raw) // 8), raw)
            for v in samples:
                if math.isnan(v):
                    nan_count += 1
                    continue
                if math.isinf(v):
                    inf_count += 1
                    continue
                av = abs(v)
                if av > peak:
                    peak = av
                if av >= 0.999969:
                    clipped += 1
                sum_sq += v * v
            count = len(samples)
        else:
            raise ValueError(f"unsupported format tag {format_tag} / {bits} bits")

        report.peak = peak
        report.rms = math.sqrt(sum_sq / count) if count else 0.0
        report.nan_count = nan_count
        report.inf_count = inf_count
        report.clipped_count = clipped
        report.silent = report.rms < 1e-7
    except Exception as exc:  # noqa: BLE001 - report parser failures as data
        report.error = f"{type(exc).__name__}: {exc}"

    return report


def validate(
    report: WavReport,
    *,
    expect_audio: bool = True,
    min_rms: float = 1e-4,
    max_peak: float = 1.0001,
    expected_duration: float | None = None,
    duration_tolerance: float = 0.5,
) -> list[str]:
    problems: list[str] = []
    if not report.exists:
        problems.append("wav missing")
        return problems
    if report.error:
        problems.append(f"wav invalid: {report.error}")
        return problems
    if report.nan_count:
        problems.append(f"non-finite samples: {report.nan_count} NaN")
    if report.inf_count:
        problems.append(f"non-finite samples: {report.inf_count} Inf")
    if report.peak > max_peak:
        problems.append(f"peak {report.peak:.4f} > {max_peak}")
    if report.frames == 0 or report.duration_s <= 0:
        problems.append("empty audio")
    if expect_audio and report.rms < min_rms:
        problems.append(f"unexpected silence (rms={report.rms:.6g})")
    if expected_duration is not None:
        if abs(report.duration_s - expected_duration) > duration_tolerance:
            problems.append(
                f"duration {report.duration_s:.3f}s != expected {expected_duration:.3f}s"
            )
    return problems
