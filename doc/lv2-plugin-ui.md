# LV2 plugin UIs

LMMS can host `ui:X11UI` interfaces for LV2 instruments and effects.

## Opening a UI

Open the instrument/effect window and click **Show GUI** in the plugin tab.
The button is only shown when a supported X11 UI is available.

## Window modes

The mode is selected under **Edit -> Settings -> Performance -> Plugins ->
LV2 plugins UI**:

| Mode | Behaviour |
| --- | --- |
| Automatically choose | Embedded on X11 (`xcb`), floating on Wayland |
| Embed in LMMS window | MDI subwindow (X11 only, falls back to floating) |
| Open in a separate window | Standalone X11 window (also through XWayland) |
| Disabled | Generic LMMS controls only |

On Wayland, the UI is a separate XWayland window because Qt Wayland cannot
embed foreign X11 windows.

Environment overrides:

- `LMMS_DISABLE_LV2_UI=1` disables all LV2 UIs.
- `LMMS_ENABLE_UNSTABLE_LV2_UI=1` re-enables UIs for plugins on the known
  unstable list (currently empty).
- `LMMS_LV2_UI_SCALE=1.5` overrides the scale factor passed to plugin UIs.
  By default the widget's device pixel ratio is used for embedded UIs, while
  floating X11 UIs use 1.0 so they match the pixel size that VST2 editors
  report. The floating window is also clamped to the screen; resizable UIs
  receive the clamped size and re-layout their contents.

## Implemented

- X11 UI selection via suil, including `ui:parent`, `instance-access` and
  `data-access`.
- Control-port changes from the UI are written back to LMMS models.
- Control values are periodically pushed to the UI.
- Atom events are forwarded in both directions (`atom:eventTransfer`).
- `ui:idleInterface` is called at ~30 Hz.
- `ui:showInterface` / `ui:hideInterface` are used when the UI provides them.
- Dynamic `ui:resize` requests are queued to the GUI thread. Host-side
  resizing also resizes the embedded plugin widget.
- Floating X11 windows handle window-manager close events and resizing; a
  closed window is hidden and can be reopened with **Show GUI**.
- Touch events are accepted (currently no-op).
- The UI is detached and freed before the plugin instance is destroyed.
- Plugin state (including state bundles with files) is saved and restored by
  the LV2 state support.

Surge XT was previously on the unstable list. After the feature lifetime and
resize fixes it passes ASan and repeated open/close tests, so its native UI is
enabled again.

## Known limitations

- Plugins with more than two audio inputs or outputs are loaded, but LMMS
  only uses the first stereo pair. Extra ports are connected and ignored,
  matching the VST2 host.
- Qt5/GTK-only UIs are not supported by the current host.
- Atom events are queued with a mutex; the audio-thread path is not fully
  real-time safe yet.
- A UI crash runs in-process and can take LMMS down with it. Out-of-process
  UI hosting is future work.
- LV2 control ports are block-rate: a parameter change takes effect at the
  start of the next `run()` cycle, not at a sample offset inside the block.
  Sample-accurate plugin parameters are VST2-only for now; core LV2 has no
  standard parameter event with a sample position. MIDI atom events still keep
  their block-relative frame offsets.
- Plugins taken from Flatpak runtimes and symlinked into `~/.lv2` can load, but
  their data files are not visible outside the sandbox. Surge, for example,
  looks for `~/.local/share/surge` or `/usr/share/surge`; without that link
  its UI reports a skin error and stays black. Link the runtime's `files/share`
  directory into the path the plugin expects, or install a native build.

## Flatpak plugin bundles

Flatpak audio runtimes install their LV2 bundles under
`~/.local/share/flatpak/runtime/org.freedesktop.LinuxAudio.Plugins.*/.../files/lv2`.
Symlinking those bundles into `~/.lv2` is enough for scanning and audio, but
the plugin's resource lookup still uses paths outside the runtime:

```bash
# Surge XT
ln -s "$(readlink -f ~/.lv2/Surge.lv2)/../../share/surge" \
      ~/.local/share/surge
```

The same pattern applies to other bundles that ship banks, skins or kits.
Check the plugin's log output first; a missing resource usually produces an
explicit path in the error message.

