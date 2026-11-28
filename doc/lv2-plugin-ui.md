# LV2 plugin UIs

LMMS can host `ui:X11UI` interfaces for LV2 instruments and effects.

## Opening a UI

Open the instrument/effect window and click **Show GUI** in the plugin tab.
The button is only shown when a supported X11 UI is available.

## Window modes

The setting is under **Edit -> Settings -> Performance -> Plugins ->
LV2 plugins UI**:

| Mode | Behaviour |
| --- | --- |
| Open in a separate window | Standalone X11 window (also through XWayland) |
| Disabled | Generic LMMS controls only |

Embedding the UI in an MDI subwindow was removed. The installed plugin UIs take
`instance-access`, so a UI has to share a process with its DSP instance; P6.4
RemoteLv2 will place both in a host child and keep the separate window. A saved
`embedded` or `auto` value is treated as "open in a separate window".

Environment overrides:

- `LMMS_DISABLE_LV2_UI=1` disables all LV2 UIs.
- `LMMS_ENABLE_UNSTABLE_LV2_UI=1` re-enables UIs for plugins on the known
  unstable list (currently empty).
- `LMMS_LV2_UI_SCALE=1.5` overrides the scale factor passed to plugin UIs.
  The default is 1.0 so the plugin's pixel size matches the size that VST2
  editors report. The window is also clamped to the screen; resizable UIs
  receive the clamped size and re-layout their contents.

## Implemented

- X11 UI selection via suil, including `ui:parent`, `instance-access` and
  `data-access`.
- Control-port changes from the UI are written back to LMMS models.
- Control values are periodically pushed to the UI.
- Atom events are forwarded in both directions (`atom:eventTransfer`).
- `ui:idleInterface` is called at ~30 Hz.
- `ui:showInterface` / `ui:hideInterface` are used when the UI provides them.
- Dynamic `ui:resize` requests are queued to the GUI thread and applied to the
  X11 window; resizable UIs also have their child window resized.
- Floating X11 windows handle window-manager close events and resizing; a
  closed window is hidden and can be reopened with **Show GUI**.
- Touch events are accepted (currently no-op).
- The UI is detached and freed before the plugin instance is destroyed.
- Plugin state (including state bundles with files) is saved and restored by
  the LV2 state support.

Surge XT was previously on the unstable list. After the feature lifetime and
resize fixes it passes ASan and repeated open/close tests, so its native UI is
enabled again.

## UI size audit

The native X11 UIs of the installed plugins were measured on a 1920x1080
screen with `ui:scaleFactor` 1.0. The sizes below are the plugin widget sizes;
window decorations are additional.

| Plugin | LV2 (separate window) | VST2 |
| --- | --- | --- |
| DrumGizmo | 750x740 (resizable) | - |
| OB-Xf | 1150x576 | - |
| Pianoteq 8 STAGE | 864x888 | 864x888 |
| sfizz | 775x515 | - |
| Surge XT | 1141x711 | - (VST3 only) |
| Surge XT Effects | 600x490 | - (VST3 only) |
| ZynAddSubFX | 1181x659 | 1181x659 |

- LV2 and VST2 sizes match for the plugins that provide both interfaces.
- The effect UIs are opened from the mixer channel's **Effects chain** panel:
  enable the effect, click **Controls**, then **Show GUI**.
- Native Linux VST2 editors always open as separate windows; the native host
  ignores the `qt`/`xembed` embedding request (see `vst-plugin-hosting.md`).
- DrumGizmo requests its natural size from inside `suil_instance_new()`. The
  host now keeps that request and applies it after the instance exists; before
  this, the UI stayed at the 1x1 size of its initial X11 window.
- The LV2 UI is always a separate X11 window. The former embedded mode was
  removed; see the window mode table above.
- With `LMMS_LV2_REMOTE=1` the same plugins were audited with the UI hosted by
  the `lmms-lv2-host` child process: DrumGizmo, ZynAddSubFX, OB-Xf, Pianoteq 8
  STAGE, sfizz and Surge XT all opened at the sizes in the table above and
  rendered non-black content.
- The MDI workspace scroll bars are enabled on demand, so a subwindow larger
  than the workspace can be panned instead of leaving part of it unreachable.

## Known limitations

- Plugins with more than two audio inputs or outputs are loaded, but LMMS
  only uses the first stereo pair. Extra ports are connected and ignored,
  matching the VST2 host.
- Qt5/GTK-only UIs are not supported by the current host.
- Atom events are queued with a mutex; the audio-thread path is not fully
  real-time safe yet.
- A UI crash runs in-process and can take LMMS down with it. The installed UIs
  take `instance-access` (a raw DSP pointer), so the UI cannot move to a child
  process on its own. P6.4 RemoteLv2 moves the DSP and the UI together into a
  host child and keeps the separate window; see `lv2-host-design.md`.
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

