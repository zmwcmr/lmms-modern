"""Result checks shared by the external GUI drivers."""

from __future__ import annotations

from dataclasses import dataclass, asdict


@dataclass
class GuiResult:
    scenario: str
    exit_code: int | None = None
    signal: int | None = None
    timed_out: bool = False
    wall_s: float = 0.0
    modal_seen: bool = False
    windows_before: int = 0
    windows_after: int = 0
    clients_before: int = 0
    clients_after: int = 0
    screenshot: str | None = None
    problems: list[str] | None = None
    stdout: str = ""
    stderr: str = ""

    def __post_init__(self) -> None:
        if self.problems is None:
            self.problems = []

    def to_dict(self) -> dict:
        return asdict(self)


def validate(
    result: GuiResult,
    *,
    allow_nonzero: bool = False,
    max_window_delta: int = 1,
) -> list[str]:
    problems: list[str] = []
    if result.timed_out:
        problems.append("timeout")
    if result.signal is not None:
        problems.append(f"signal {result.signal}")
    if result.exit_code not in (0, None) and not allow_nonzero:
        problems.append(f"exit code {result.exit_code}")
    if abs((result.windows_after - result.windows_before)) > max_window_delta:
        problems.append(
            f"window leak {result.windows_before} -> {result.windows_after}"
        )
    if abs((result.clients_after - result.clients_before)) > max_window_delta:
        problems.append(
            f"X client leak {result.clients_before} -> {result.clients_after}"
        )
    if result.modal_seen:
        problems.append("modal dialog was not dismissed")
    markers = (
        "AddressSanitizer",
        "LeakSanitizer",
        "ThreadSanitizer",
        "Segmentation fault",
        "ASSERT",
        "SIGSEGV",
    )
    for marker in markers:
        if marker in result.stderr or marker in result.stdout:
            problems.append(f"marker: {marker}")
    result.problems.extend(problems)
    return problems
