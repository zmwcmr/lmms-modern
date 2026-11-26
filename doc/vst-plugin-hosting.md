# VST2 plugin hosting

LMMS hosts VST2 instruments and effects through the Vestige plugin family.
VST3 is not supported.

## Where plugins are found

The plugin list is built lazily and cached in
`~/.cache/lmms/vst-list.json`. On Linux the following locations are scanned:

1. the directory configured in **Edit -> Settings -> Paths -> VST plugins
   directory** (defaults to `<lmms-config-dir>/vst`),
2. every directory listed in `VST_PATH` or `VST2_PATH` (colon separated),
3. `~/.vst`,
4. `~/.local/lib/vst`,
5. `/usr/local/lib/vst` and `/usr/local/lib64/vst`,
6. `/usr/lib/vst` and `/usr/lib64/vst`.

On Windows the configured directory and
`C:/Program Files/Steinberg/VstPlugins` are scanned. Wine prefixes are added
when 32-bit VST support is enabled.

The scan is incremental: a plugin is re-probed only when its modification time
or size changes. Files which are not loadable VST2 plugins are kept in the
cache as `invalid` and are not shown in the plugin browser. Deleting the cache
file forces a full re-scan on the next start; **Rescan VST plugins** in the
instrument browser and **Rescan** in the Add effect dialog do the same without
restarting LMMS.

## How plugins are loaded

Native Linux 64-bit ELF plugins are loaded by `NativeLinuxRemoteVstPlugin64`,
a separate host process. 32-bit plugins use the matching Wine bridge when it is
built. Windows uses `RemoteVstPlugin32` / `RemoteVstPlugin64`.

Running the plugin out of process means a crashing plugin does not take LMMS
down. The remote host is started with `PR_SET_PDEATHSIG(SIGTERM)` on Linux, so
it cannot outlive LMMS and leave an orphan window behind.

## Instrument and effect GUIs

VST instruments and effects expose their original editor. What **Show/hide**
does depends on **Edit -> Settings -> Performance -> Plugins -> VST plugins
UI**:

| Method | Behaviour |
| --- | --- |
| Separate window (default) | The editor is a standalone window. On Wayland it still opens through XWayland. |
| Embed in LMMS window (`qt`) | The editor window is embedded into the LMMS dialog. Available on `xcb` only; on Wayland LMMS falls back to the separate window. |
| XEmbed (`xembed`) | X11 embedding, available with Qt 5 on `xcb` only. |
| Disabled/headless | No editor; the generic parameter controls remain usable. |

Hiding an embedded editor does not destroy the plugin editor; reopening it is
cheap and avoids reinitialising plugin GUIs which are not restart-safe.

Native Linux 64-bit VST2 plugins do not support host embedding through the
VST2 ABI. Their editors always open as separate X11 windows, regardless of the
`qt`/`xembed` setting. The setting only affects Windows VST2 plugins hosted
through the Wine bridge.

VST2 editors report a fixed pixel size through `effEditGetRect`; the host has
no standard way to scale their content. LMMS re-reads the rectangle after
`effEditOpen` because some plugins only report their real size then, and the
remote X11 window remains user-resizable. On fractional Wayland desktops the
plugin's fixed pixel size can look smaller than native HiDPI applications;
that is a plugin limitation, not a host size calculation error.

## State and presets

When a project is saved, LMMS asks the plugin for its opaque chunk
(`effGetChunk`). If the plugin supports chunks, the complete state is stored
base64-encoded in the project. Otherwise LMMS stores the current program and
the individual parameter values.

The VST effect dialog also offers preset load/save and previous/next program
navigation. Presets are `.fxp`/`.fxb` files handled by the plugin.

## Known limitations

- VST3 is not supported.
- 32-bit VSTs require the 32-bit remote host (and Wine on Linux).
- Plugin editors are X11 windows. Native Wayland embedding is not possible, so
  the default separate-window mode is used on Wayland. A saved `qt` embedding
  setting is ignored there instead of producing a black embedded widget.
  XEmbed and `qt` embedding require the `xcb` platform.
- Native Linux VST2 editors ignore the embedding method and always open as
  separate windows; the host prints `Native linux VST works only without
  embedding` when an embedded mode is requested.
- A remote host crash loses the plugin's current unsaved state. The parameters
  stored in the project are restored the next time it is loaded.
- Scanning starts every candidate plugin once. A large collection can make
  the first scan slow; subsequent starts use the cache.
