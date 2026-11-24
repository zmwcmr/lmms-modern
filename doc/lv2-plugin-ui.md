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
  unstable list (currently Surge XT, whose UI corrupts the heap in this host).

## Implemented

- X11 UI selection via suil, including `ui:parent`, `instance-access` and
  `data-access`.
- Control-port changes from the UI are written back to LMMS models.
- Control values are periodically pushed to the UI.
- Atom events are forwarded in both directions (`atom:eventTransfer`).
- `ui:idleInterface` is called at ~30 Hz.
- Dynamic `ui:resize` requests are queued to the GUI thread.
- Touch events are accepted (currently no-op).
- The UI is detached and freed before the plugin instance is destroyed.
- Plugin state (including state bundles with files) is saved and restored by
  the LV2 state support.

## Known limitations

- Surge XT is blocklisted for native UI because its JUCE X11 UI corrupts the
  heap when hosted here; its generic controls remain usable.
- Qt5/GTK-only UIs are not supported by the current host.
- Atom events are queued with a mutex; the audio-thread path is not fully
  real-time safe yet.
- A UI crash runs in-process and can take LMMS down with it. Out-of-process
  UI hosting is future work.
