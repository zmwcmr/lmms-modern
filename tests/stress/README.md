# LMMS local stability stress harness

This directory contains the local-only stress, fuzz and soak tooling used to
look for crashes, hangs, leaks and serialization bugs in LMMS. It is not part
of the normal ctest suite.

## Requirements

- A built LMMS tree (`build/` by default).
- Python 3.10+.
- Real plugins used by the generator (optional; cases skip missing plugins):
  - VST3: Dexed, Surge XT, Surge XT Effects, OB-Xf, sfizz
  - VST2: ZynAddSubFX, Zyn effects
  - LV2: Surge XT, Surge XT Effects, Zyn effects

Plugin paths can be overridden with `LMMS_TEST_DEXED`, `LMMS_TEST_ZYN`,
`LMMS_TEST_SURGE` (see `generator.py`).

## Quick start

```bash
# 50 tiny random projects, no third-party plugins
python3 tests/stress/run.py --build build --profile tiny --cases 50 --seed-start 1

# 200 random projects with real VST2/VST3/LV2 instruments and effects
python3 tests/stress/run.py --build build --profile medium --cases 200 \
    --seed-start 1000 --plugin-ratio 0.5 --timeout 90

# The same under AddressSanitizer
python3 tests/stress/run.py --build build-asan --profile tiny --cases 30 \
    --seed-start 7000 --plugin-ratio 0.5 --timeout 180

# Replay saved regressions
python3 tests/stress/run.py --build build --replay

# Soak: generate and run cases until the deadline (does not pre-generate)
python3 tests/stress/run.py --build build --profile tiny --soak-hours 2 \
    --seed-start 60000 --plugin-ratio 0.25 --timeout 60
```

Every case runs in an isolated `HOME`/`XDG_*` directory with a fresh plugin
scan cache. Successful WAVs are deleted; failures are copied to
`tests/stress/failures/<run-id>/`.

## Tools

| File | Purpose |
|---|---|
| `run.py` | Random project renderer / soak driver |
| `generator.py` | Structure-aware `.mmp` mutator, real plugin/effect injection |
| `runner.py` | Subprocess isolation, timeout, RSS/FD sampling, WAV checks |
| `wav_check.py` | RIFF/WAV parser and audio sanity checks |
| `fuzz_xml.py` | Byte-level project-file fuzzing through `lmms render` |
| `fuzz_numbers.py` | NaN/Inf/overflow values in numeric project attributes |
| `fuzz_samples.py` | Corrupted sample files referenced by a project |
| `fuzz_midi.py` | MIDI import fuzzing (GUI import; invalid files show a dialog) |
| `gui_smoke.py` | Start GUI, load a project, SIGINT, require clean exit |
| `plugins/probe_plugins.py` | Load every real plugin with an unmutated fixture |
| `plugins/multi_instance` tests (inline) | 1/4/8/16 instances of one plugin |
| `roundtrip.cpp` / `LmmsRoundTrip` | Load → save → load → save, optional loop count |

## Metrics and failure criteria

- exit code / fatal signal / timeout
- ASan/UBSan/LSan markers
- WAV: sample rate, channels, duration, NaN/Inf, peak, RMS
- peak RSS, FD count, thread count (process tree)
- real-plugin audio is not hash-gated; silence is a warning for mutated
  projects and a failure only for unmutated `probe_plugins.py` fixtures

## Real-plugin host isolation

The harness can kill plugin host processes while LMMS is rendering:

- VST3 remote DSP: `LMMS_VST3_REMOTE_DSP=1`, kill `lmms-vst3-host`
- VST2: kill `NativeLinuxRemoteVstPlugin64`
- LV2 remote: `LMMS_LV2_REMOTE=1`, kill `lmms-lv2-host`

LMMS must exit 0 (possibly with a partial output file) and must not crash or
hang. These cases were verified manually; see `REPORT.md`.

## ASan notes

Use `QT_QPA_PLATFORM=offscreen` and the provided suppressions:

```bash
export QT_QPA_PLATFORM=offscreen
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0
export LSAN_OPTIONS=suppressions=tests/stress/lsan.supp
```

Third-party plugin code is not instrumented. Plugin leaks are suppressed or
ignored; LMMS-side leaks are expected to be zero.

## Capacity observations

- 100 instrument tracks × 500 notes: renders in ~65 s, ~390 MB RSS.
- 10k simultaneous notes in one bar: ~9 s.
- 50k+ simultaneous notes in one bar: >5 min (CPU-bound polyphony); not a
  crash, but a practical limit for generated projects.
- Deeply nested XML (>1000 levels) is rejected before Qt's recursive parser
  can overflow the stack.

## Known harness limitations

- Invalid MIDI files trigger a modal “Couldn't import file” dialog in the GUI;
  `fuzz_midi.py` counts those as timeouts unless the dialog is dismissed.
- Full GUI interaction (menus, dialogs, plugin editors) needs an X server and
  an input driver such as `xdotool`; only offscreen start/load/quit is
  automated here.
- Real plugin audio/state is non-deterministic; do not hash WAV output.
