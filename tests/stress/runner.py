"""Subprocess runner for LMMS stress cases."""

from __future__ import annotations

import json
import os
import signal
import shutil
import subprocess
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path

from generator import REPO_ROOT, Case
from wav_check import check_wav, validate

SANITIZER_MARKERS = (
    "AddressSanitizer",
    "LeakSanitizer",
    "ThreadSanitizer",
    "UndefinedBehaviorSanitizer",
    "MemorySanitizer",
    "runtime error:",
    "SUMMARY: ",
    "ERROR: AddressSanitizer",
)


def _read_int(path: str) -> int | None:
    try:
        with open(path, "r", encoding="ascii", errors="ignore") as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return None


def _proc_rss_kb(pid: int) -> int:
    # Sum the RSS of the process and its descendants.
    total = 0
    stack = [pid]
    seen = set()
    while stack:
        p = stack.pop()
        if p in seen:
            continue
        seen.add(p)
        status = f"/proc/{p}/status"
        try:
            with open(status, "r", encoding="ascii", errors="ignore") as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        total += int(line.split()[1])
                        break
        except OSError:
            pass
        try:
            for tid in os.listdir(f"/proc/{p}/task"):
                children = f"/proc/{p}/task/{tid}/children"
                try:
                    with open(children, "r", encoding="ascii", errors="ignore") as f:
                        for child in f.read().split():
                            stack.append(int(child))
                except OSError:
                    pass
        except OSError:
            pass
    return total


def _proc_counts(pid: int) -> tuple[int, int]:
    fds = 0
    threads = 0
    try:
        fds = len(os.listdir(f"/proc/{pid}/fd"))
    except OSError:
        pass
    try:
        threads = len(os.listdir(f"/proc/{pid}/task"))
    except OSError:
        pass
    return fds, threads


class Sampler(threading.Thread):
    def __init__(self, pid: int, interval: float = 0.2):
        super().__init__(daemon=True)
        self.pid = pid
        self.interval = interval
        self.stop_event = threading.Event()
        self.max_rss = 0
        self.max_fds = 0
        self.max_threads = 0

    def run(self) -> None:
        while not self.stop_event.wait(self.interval):
            rss = _proc_rss_kb(self.pid)
            fds, threads = _proc_counts(self.pid)
            self.max_rss = max(self.max_rss, rss)
            self.max_fds = max(self.max_fds, fds)
            self.max_threads = max(self.max_threads, threads)

    def stop(self) -> None:
        self.stop_event.set()
        self.join(timeout=2)


@dataclass
class RunResult:
    case_id: str
    seed: int
    profile: str
    status: str  # pass / fail / timeout / crash / sanitizer / render_error
    exit_code: int | None
    signal: int | None
    timed_out: bool
    wall_s: float
    max_rss_kb: int
    max_fds: int
    max_threads: int
    wav: dict
    problems: list[str]
    stderr_hits: list[str]
    case_path: str
    wav_path: str
    meta: dict

    def to_json(self) -> dict:
        return {
            "case_id": self.case_id,
            "seed": self.seed,
            "profile": self.profile,
            "status": self.status,
            "exit_code": self.exit_code,
            "signal": self.signal,
            "timed_out": self.timed_out,
            "wall_s": round(self.wall_s, 3),
            "max_rss_kb": self.max_rss_kb,
            "max_fds": self.max_fds,
            "max_threads": self.max_threads,
            "wav": self.wav,
            "problems": self.problems,
            "stderr_hits": self.stderr_hits,
            "case_path": self.case_path,
            "wav_path": self.wav_path,
            "meta": self.meta,
        }


def default_env(case_dir: Path) -> dict[str, str]:
    env = dict(os.environ)
    home = case_dir / "home"
    (home / "lmms").mkdir(parents=True, exist_ok=True)
    env.update(
        {
            "QT_QPA_PLATFORM": "offscreen",
            "HOME": str(home),
            "XDG_CONFIG_HOME": str(case_dir / "config"),
            "XDG_DATA_HOME": str(case_dir / "data"),
            "XDG_CACHE_HOME": str(case_dir / "cache"),
            "VST_PATH": str(Path.home() / ".vst"),
            "VST2_PATH": str(Path.home() / ".vst"),
            "VST3_PATH": str(Path.home() / ".vst3"),
            "LV2_PATH": ":".join(
                [
                    str(Path.home() / ".lv2"),
                    "/usr/lib/lv2",
                    "/usr/local/lib/lv2",
                ]
            ),
            "ASAN_OPTIONS": "detect_leaks=1:abort_on_error=1",
            "LSAN_OPTIONS": f"suppressions={Path(__file__).resolve().parent / 'lsan.supp'}",
        }
    )
    env.pop("LMMS_ENABLE_BLOCKED_PLUGINS", None)
    return env


def run_case(
    case: Case,
    *,
    binary: Path,
    work_dir: Path,
    timeout: float,
    keep_wav: bool,
    extra_env: dict[str, str] | None = None,
) -> RunResult:
    case_dir = Path(tempfile.mkdtemp(prefix=f"{case.case_id}-", dir=work_dir))
    case_path = case_dir / "case.mmp"
    wav_path = case_dir / "case.wav"
    case_path.write_bytes(case.xml_bytes)
    env = default_env(case_dir)
    if extra_env:
        env.update(extra_env)

    cmd = [
        str(binary),
        "render",
        str(case_path),
        "-o",
        str(wav_path),
        "-f",
        "wav",
        "-s",
        "44100",
    ]
    started = time.monotonic()
    timed_out = False
    exit_code: int | None = None
    stdout = b""
    stderr = b""
    sampler: Sampler | None = None
    proc: subprocess.Popen | None = None
    try:
        proc = subprocess.Popen(
            cmd,
            cwd=str(REPO_ROOT),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
        )
        sampler = Sampler(proc.pid)
        sampler.start()
        try:
            stdout, stderr = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = proc.communicate(timeout=10)
        exit_code = proc.returncode
    except Exception as exc:  # noqa: BLE001
        stderr = (stderr or b"") + f"\nharness error: {exc}\n".encode()
    finally:
        if sampler is not None:
            sampler.stop()
        if proc is not None and proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass

    wall = time.monotonic() - started
    stderr_text = stderr.decode("utf-8", errors="replace")
    stdout_text = stdout.decode("utf-8", errors="replace")
    combined = stdout_text + "\n" + stderr_text
    stderr_hits = [marker for marker in SANITIZER_MARKERS if marker in combined]

    report = check_wav(str(wav_path))
    wav_problems = validate(
        report,
        expect_audio=case.expect_audio and case.strict_audio,
        min_rms=1e-4,
        max_peak=1.0001,
    )
    if not case.strict_audio and report.silent and case.expect_audio:
        # Silence in a mutated project is a warning, not a hard failure: the
        # mutation may have moved/removed the audible notes.
        wav_problems = [p for p in wav_problems if "silence" not in p]

    signal_no = -exit_code if exit_code is not None and exit_code < 0 else None
    problems: list[str] = []
    if timed_out:
        problems.append(f"timeout after {timeout}s")
    if signal_no is not None:
        problems.append(f"killed by signal {signal_no}")
    if stderr_hits:
        problems.append("sanitizer/runtime error markers: " + ", ".join(stderr_hits))
    if not timed_out and signal_no is None and exit_code not in (0, None):
        problems.append(f"exit code {exit_code}")
    problems.extend(wav_problems)

    if signal_no is not None:
        status = "crash"
    elif timed_out:
        status = "timeout"
    elif stderr_hits:
        status = "sanitizer"
    elif case.malformed and exit_code not in (0, None):
        # Malformed input is expected to be rejected gracefully.
        status = "pass"
        problems = []
    elif exit_code not in (0, None):
        status = "render_error"
    elif wav_problems:
        status = "fail"
    else:
        status = "pass"

    meta = {
        "case_id": case.case_id,
        "seed": case.seed,
        "profile": case.profile,
        "description": case.description,
        "malformed": case.malformed,
        "notes": case.notes,
        "plugins": case.plugins,
        "effects": case.effects,
        "expect_audio": case.expect_audio,
        "strict_audio": case.strict_audio,
        "cmd": cmd,
        "exit_code": exit_code,
        "timed_out": timed_out,
        "stderr_tail": stderr_text[-8000:],
        "stdout_tail": stdout_text[-4000:],
        "wav_path": str(wav_path),
        "case_path": str(case_path),
    }
    (case_dir / "meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    (case_dir / "stdout.log").write_text(stdout_text, encoding="utf-8", errors="replace")
    (case_dir / "stderr.log").write_text(stderr_text, encoding="utf-8", errors="replace")

    result = RunResult(
        case_id=case.case_id,
        seed=case.seed,
        profile=case.profile,
        status=status,
        exit_code=exit_code,
        signal=signal_no,
        timed_out=timed_out,
        wall_s=wall,
        max_rss_kb=sampler.max_rss if sampler else 0,
        max_fds=sampler.max_fds if sampler else 0,
        max_threads=sampler.max_threads if sampler else 0,
        wav=report.to_dict(),
        problems=problems,
        stderr_hits=stderr_hits,
        case_path=str(case_path),
        wav_path=str(wav_path),
        meta=meta,
    )
    if status == "pass" and not keep_wav:
        try:
            wav_path.unlink()
        except OSError:
            pass
    return result


def collect_failure(result: RunResult, failures_root: Path) -> Path:
    """Copy a failed case into the persistent failures directory."""
    dest = failures_root / result.case_id
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    for name in ("case.mmp", "case.wav", "meta.json", "stdout.log", "stderr.log"):
        src = Path(result.case_path).parent / name
        if src.exists():
            shutil.copy2(src, dest / name)
    (dest / "result.json").write_text(
        json.dumps(result.to_json(), indent=2), encoding="utf-8"
    )
    return dest
