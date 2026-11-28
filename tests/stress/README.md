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

The GUI/X11 lane additionally needs `Xvfb`, `openbox`, `xdotool`, `wmctrl`,
`xorg-x11-utils` and ImageMagick (`import`). The concurrency lane needs a
TSAN build (`-DWANT_TSAN=ON`).

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
| `imports/import_runner.py` | MIDI import fuzzing with automatic dialog dismissal |
| `gui_smoke.py` | Start GUI, load a project, SIGINT, require clean exit |
| `gui/gui_scenarios.py` | One-process-per-scenario in-process GUI stress driver |
| `gui/gui_stress.cpp` | `GuiStress` helper: tracks, editors, undo, mixer, resize, actions |
| `gui/plugin_editors.py` | Open real VST2/VST3/LV2 editors under Xvfb |
| `gui/xdotool_driver.py` | Bounded random input into a GUI window |
| `plugins/inventory.py` | Discover VST2/VST3/LV2/LADSPA/Carla fixtures with hashes |
| `plugins/fixtures.py` | Build unmutated instrument/effect fixtures per plugin |
| `plugins/probe_plugins.py` | Load every real instrument with an unmutated fixture |
| `plugins/probe_effects.py` | Insert every real effect into a demo fxchain |
| `plugins/multi_instance.py` | 1/4/8/16/32 instances of one plugin in one render |
| `plugins/host_kill.py` | SIGKILL a VST2/VST3/LV2 host process mid-render |
| `plugins/state_fuzz.py` | Mutate VST2 chunks/VST3 component+controller state |
| `concurrency/run_concurrency.py` | TSAN audio-thread vs model/editor mutations |
| `soak/soak.py` | Single-process load/render/save soak with RSS/FD drift checks |
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

Run them with:

```bash
python3 tests/stress/plugins/host_kill.py --build build --plugin dexed --mode vst3
python3 tests/stress/plugins/host_kill.py --build build --plugin zyn-add-sub-fx --mode vst2
python3 tests/stress/plugins/host_kill.py --build build --plugin surge-xt --mode lv2
```

## Real plugin fixtures and state

`plugins/inventory.py` discovers local plugins and stores a machine-specific
inventory in `plugins/inventory.generated.json`. Fixtures are generated from
the demo song on first use, so no plugin binary or state is committed.

```bash
python3 tests/stress/plugins/probe_plugins.py --build build --write-inventory
python3 tests/stress/plugins/probe_effects.py --build build
python3 tests/stress/plugins/multi_instance.py --build build --plugin dexed
python3 tests/stress/plugins/state_fuzz.py --build build --plugin dexed --cases 50
```

LADSPA libraries are discovered from `LADSPA_PATH`, `~/.ladspa` and the
standard system directories. Carla rack state cannot be synthesized; save one
project per rack as `plugins/fixtures/effects/carla/<name>.mmp` through the
LMMS GUI and it joins the effect inventory automatically.

## GUI and plugin editors

`GuiStress` is a self-contained scenario runner (one process per scenario):

```bash
cmake --build build --target GuiStress
python3 tests/stress/gui/gui_scenarios.py --build build
python3 tests/stress/gui/gui_scenarios.py --build build --xvfb \
    --project 'data/projects/shorties/Crunk(Demo).mmp'
python3 tests/stress/gui/plugin_editors.py --build build --xvfb \
    --plugin dexed --format vst3
```

The X11 lane needs `Xvfb`, `openbox`, `xdotool`, `wmctrl`,
`xorg-x11-utils` and ImageMagick. Unexpected modal dialogs are closed by
`gui/dialog_watchdog.py`.

## Concurrency and soak helpers

These targets are `EXCLUDE_FROM_ALL` and are never registered with ctest:

```bash
cmake --build build-tsan --target StressConcurrency
python3 tests/stress/concurrency/run_concurrency.py --build build-tsan \
    --mode audio-editor --seconds 60

cmake --build build --target SingleProcessSoak
python3 tests/stress/soak/soak.py --build build \
    --project 'data/projects/shorties/Crunk(Demo).mmp' --hours 8
```

`StressConcurrency` covers audio vs model, editor, automation and mixer
mutations. `SingleProcessSoak` keeps one LMMS process alive and reports peak
RSS, file descriptors, thread count and sanitizer markers per chunk.

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

- Invalid MIDI files trigger a modal “Couldn't import file” dialog in the GUI.
  `fuzz_midi.py` therefore counts them as timeouts unless a window manager
  closes the dialog; `imports/import_runner.py --display :99` does that
  automatically and classifies the result as `dialog_timeout` when dismissal
  fails.
- Plugin editor windows need a real X server. The in-process `GuiStress`
  scenarios are offscreen-only; use the Xvfb lane for editors and X11
  embedding.
- Real plugin audio/state is non-deterministic; do not hash WAV output.
- `GuiStress` deliberately skips static destruction (`std::_Exit`) so a
  crashed/hung scenario cannot be masked by shutdown code. Leak checking is
  covered by `gui_smoke.py`, which exits normally.
