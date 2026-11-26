# VST3 plugin hosting

LMMS can host native VST3 instruments and effects through the optional
`Vst3Base`, `Vst3Instrument` and `Vst3Effect` plugins. The existing VST2 /
VeSTige path is unchanged.

## Build

`WANT_VST3` is enabled by default:

```sh
cmake -S . -B build -DWANT_VST3=ON
cmake --build build
cmake --install build
```

Disable it with `-DWANT_VST3=OFF`. The build installs:

- `libvst3base.so` / `vst3base` – shared host code and the plugin browser
  feature,
- `lmms-vst3-scanner` – crash-isolated metadata scanner,
- `lmms-vst3-host` – out-of-process DSP host (Linux),
- `lmms-vst3-editor-host` – out-of-process editor host,
- `vst3instrument` and `vst3effect` – the LMMS plugins.

No Steinberg SDK is required. The host ABI is the ISC-licensed
[Travesty](https://github.com/DISTRHO/DPF) C interface, vendored under
`plugins/Vst3Base/travesty/`.

## Finding plugins

`Vst3List` scans these locations:

1. every directory in `VST3_PATH` (colon separated),
2. `~/.vst3`,
3. `/usr/local/lib/vst3` and `/usr/local/lib64/vst3`,
4. `/usr/lib/vst3` and `/usr/lib64/vst3`,
5. the platform VST3 directories on Windows and macOS.

The result is cached in `~/.cache/lmms/vst3-list.json`. A bundle is rescanned
only when its module binary changes. Broken or hanging bundles are cached as
errors and do not stop the rest of the scan. **Rescan plugins** in the
instrument browser and **Rescan** in the Add effect dialog clear the visible
list and scan again.

Scanning runs in `lmms-vst3-scanner`, one child process per bundle, with a
60 second timeout. A crashing bundle therefore cannot crash LMMS.

## Instruments and effects

The instrument browser shows a **VST3** tree with every scanned instrument
class. The effect dialog shows the same tree for effects. One bundle can
contain several classes; each class is listed separately.

The wrappers expose:

- stereo audio through the main bus,
- MIDI note on/off, CC, pitch bend, channel pressure, poly pressure and
  program change,
- one LMMS `FloatModel` per automatable parameter,
- program and unit selection from `IUnitInfo`,
- component/controller state in the project file.

Auxiliary buses are activated when the plugin needs them, but only the first
main output bus is routed back into LMMS. Plugins with no main output are
marked unsupported by the scanner.

VST3 instrument views are resizable and use a two-column parameter grid, so
labels and the **Show GUI** button stay readable even in a narrow instrument
window. Effect dialogs are resizable too and use a three-column grid. The
parameter area scrolls vertically for plugins with many parameters.

## Editors

The original plugin editor is available from **Show GUI** in the
instrument view and the effect dialog. The button is a checkable toggle near
the top of the view; closing the editor window resets it. It is enabled
whenever any editor backend is usable, not only when the Qt platform itself is
`xcb`.

- On `xcb`, the editor is embedded in a native X11 child window.
- On Wayland, LMMS creates a floating X11 window through XWayland. The window
  is mapped and synchronized before the editor host attaches to it.
- If neither xcb nor XWayland is available, the button stays disabled and
  explains that an X11 or XWayland display is required.
- Editors run in `lmms-vst3-editor-host`, a separate process started with
  `QT_QPA_PLATFORM=xcb` so its event loop matches the X11 window. An editor
  crash closes the editor window but does not take down LMMS.
- Keyboard and scroll input are handled by the native plugin window.
- `IRunLoop` file descriptors and timers are supported (`QSocketNotifier` and
  `QTimer`).

## Out-of-process DSP

On xcb and Wayland the default path runs DSP in `lmms-vst3-host`:

- stereo audio, MIDI and parameter events travel through a shared-memory
  block,
- POSIX semaphores synchronize each audio block,
- a two second timeout returns silence if the host stops responding,
- a watchdog restarts a crashed host with the cached state,
- `PR_SET_PDEATHSIG` prevents orphan hosts when LMMS exits.

Environment switches:

```sh
LMMS_VST3_REMOTE_DSP=0     # force the in-process DSP path
LMMS_VST3_REMOTE_RESTART=0 # disable automatic host restart
LMMS_VST3_EDITOR_LOCAL=1   # use the in-process editor
LMMS_VST3_HOST=/path       # override lmms-vst3-host
LMMS_VST3_EDITOR_HOST=/path
LMMS_VST3_SCANNER=/path
```

The in-process path remains as a fallback when a helper executable is not
installed.

## Diagnostics

The low-level tools can be run without LMMS:

```sh
# list cached plugins
lmms-vst3-scanner --list --rescan

# scan one bundle
lmms-vst3-scanner --json /usr/lib/vst3/Foo.vst3

# render a short note through the in-process host
vst3-host-test /usr/lib/vst3/Foo.vst3 <class-id>

# render through the shared-memory host
vst3-remote-host-test /usr/lib/vst3/Foo.vst3 <class-id>
```

`vst3-remote-host-test --crash` kills the DSP host and checks that the
watchdog brings audio back.

## Limitations

- Native Wayland embedding is not implemented; editors are floating XWayland
  windows.
- Multi-output routing beyond the first stereo bus is not implemented.
- Note expression and advanced MIDI 2.0 are not implemented.
- 32-bit VST3 on Linux is not supported.
- Plugins that reject the host state stream may not restore all of their
  state; LMMS still stores the component/controller streams it receives.
- The out-of-process DSP host is Linux-only. The editor host is X11-specific;
  Windows and macOS use the in-process DSP path and have no native editor yet.

## File map

- `plugins/Vst3Base/Vst3Module.*` – bundle loading
- `plugins/Vst3Base/Vst3Scanner.*` – metadata extraction
- `plugins/Vst3Base/Vst3List.*` – scan cache
- `plugins/Vst3Base/Vst3Host.*` – in-process host and host interface
- `plugins/Vst3Base/Vst3ParameterModels.*` – LMMS automation bridge
- `plugins/Vst3Base/Vst3RemoteHostClient.*` – DSP IPC client
- `plugins/Vst3Base/Vst3RemoteEditorClient.*` – editor IPC client
- `plugins/Vst3Base/scanner/` – `lmms-vst3-scanner`
- `plugins/Vst3Base/dsp-host/` – `lmms-vst3-host`
- `plugins/Vst3Base/editor-host/` – `lmms-vst3-editor-host`
- `plugins/Vst3Instrument/`, `plugins/Vst3Effect/` – LMMS plugins
