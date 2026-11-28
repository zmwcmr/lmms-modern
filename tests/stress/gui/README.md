# GUI stress tests

Two lanes are provided:

1. **In-process scenarios** (`GuiStress`):
   `gui_stress.cpp` constructs the real `GuiApplication` once per process and
   runs one scenario, then exits. This avoids singleton state leaking between
   scenarios. It is intended for `QT_QPA_PLATFORM=offscreen`.

2. **External X11 scenarios** (`xvfb_session.sh`, `xdotool_driver.py`,
   `window_checks.py`, `dialog_watchdog.py`):
   these cover real windows, plugin editors and X11 embedding under Xvfb.

Build the in-process helper (not part of the default build):

```bash
cmake --build build --target GuiStress
```

Run offscreen:

```bash
python3 tests/stress/gui/gui_scenarios.py --build build \
  --scenarios startup_quit,track_crud,editors,undo_redo,mixer_effects
```

Run under Xvfb:

```bash
python3 tests/stress/gui/gui_scenarios.py --build build --xvfb \
  --scenarios startup_quit,editors,load_play_save \
  --project data/projects/shorties/Crunk(Demo).mmp
```

Plugin editor scenarios:

```bash
python3 tests/stress/gui/plugin_editors.py --build build --xvfb \
  --plugin dexed --format vst3

# all entries from plugin_editor_scenarios.json
python3 tests/stress/gui/run_plugin_editors.py --build build --xvfb
```

Requirements for the X11 lane: `Xvfb`, `openbox`, `xdotool`, `wmctrl`,
`xorg-x11-utils`, ImageMagick (`import`). Missing tools are reported, not
silently ignored.
