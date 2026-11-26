# VST3 support implementation plan

Status: P2 in-process host implemented. P3 parameter models are in place; state
save/load is best-effort and needs hardening.

## 1. Goal

Add native VST3 instrument and effect hosting to LMMS without replacing the
existing VST2/VeSTige path.

The first release should:

- scan and list VST3 bundles and classes,
- load instruments and effects,
- run stereo audio through LMMS tracks and effect chains,
- expose parameters to LMMS automation,
- save/restore component and controller state in projects,
- open the original editor on X11 and XWayland,
- isolate plugin scanning from the LMMS process,
- keep the implementation optional at build time.

Non-goals for the first release:

- VST3 note expression and advanced MIDI 2.0 features,
- multi-output routing beyond the first stereo pair,
- surround/multi-bus mixing inside LMMS,
- 32-bit VST3 on Linux,
- VST3 preset browser parity with commercial hosts,
- replacing Carla or the existing VST2 implementation.

## Progress

### Implemented: P1 scanner and metadata cache

The first phase is in `plugins/Vst3Base/`:

- `Vst3Module` resolves the platform bundle layout and loads the module
  through `ModuleEntry` / `GetPluginFactory` / `ModuleExit` (or the Windows
  and macOS equivalents).
- `Vst3Scanner` enumerates every audio class in a factory, instantiates the
  component to read bus and MIDI counts, and checks 32-bit sample support.
- `lmms-vst3-scanner` runs the scan in a separate process and writes JSON to a
  file. This is important because several real plugins print banners to
  stdout during loading.
- `Vst3List` walks `VST3_PATH`, `~/.vst3` and the standard system directories,
  runs the scanner with a timeout, caches results in
  `~/.cache/lmms/vst3-list.json`, deduplicates identical bundles by SHA-256,
  and keeps failures in the cache so a crashing bundle is not retried on every
  launch.
- `Vst3SubPluginFeatures` exposes the scanned classes to the instrument
  browser and the effect dialog, and provides the rescan hook.
- `WANT_VST3` (default `ON`) builds the scanner and the shared `vst3base`
  library. There is no external dependency; the Travesty C headers are
  vendored under `plugins/Vst3Base/travesty/`.

Quick check without starting the GUI:

```sh
cmake -S . -B build -DWANT_VST3=ON
cmake --build build --target lmms-vst3-scanner
build/plugins/lmms-vst3-scanner --list --rescan
```

The scanner was verified on Dexed, OB-Xf, sfizz (two classes), Surge XT and
Surge XT Effects. A deliberately broken bundle is cached as an error and does
not abort the scan.

### Implemented: P2 in-process stereo host

- `Vst3Host` in `plugins/Vst3Base/` owns the component, controller and
  processor, implements the host application and component handler, and
  processes a stereo main bus.
- Audio buses are configured with stereo arrangements; additional buses are
  activated so plugins such as Surge XT accept the configuration, but only the
  first main output bus is routed back to LMMS.
- MIDI note on/off, CC, pitch bend, channel pressure, poly pressure and program
  change are converted to VST3 events.
- `Vst3Instrument` is a MIDI-based LMMS instrument with an `InstrumentPlayHandle`
  and project state blobs.
- `Vst3Effect` is a normal LMMS effect with wet/dry mixing and project state
  blobs.
- `vst3-host-test` is a small command-line smoke test for the host itself.

Verified with the host test:

```text
Dexed, OB-Xf, sfizz, sfizz-multi, Surge XT   instruments, audible output
Surge XT Effects                             effect, passes/processes audio
```

Verified end-to-end with `lmms render` on a project containing a
`vst3instrument` track. The same project with a `vst3effect` in its effect
chain also renders audio.

### Implemented: P3 parameter models and automation (basic)

- `Vst3ParameterModels` creates one LMMS `FloatModel` per automatable,
  non-hidden VST3 parameter.
- Effects and instruments expose the models to the automation system.
- Automated models are sampled through `ValueBuffer` while processing and
  converted into sample-offset parameter queues in `ProcessData`.
- Manual knob changes are picked up at the next process block.
- The effect dialog and instrument view show knobs for the first 64/48
  parameters; the remaining parameters are still available to automation.
- Automated/controlled model values are saved in the project.

State handling is currently best-effort: sfizz returns a component state
stream, while Surge XT and Dexed reject the host stream with
`V3_INVALID_ARG`/`V3_NOT_IMPLEMENTED` and therefore need a follow-up
investigation (likely activation order or a stricter stream implementation).

Still open:

- state round-trip for every tested plugin,
- program/unit selection,
- automation editor round-trip tests,
- native editor embedding (P4).

Next: finish P3 state/program handling, then P4 (native editor).

## 2. What Ardour and Carla teach us

| Area | Ardour | Carla | Decision for LMMS |
| --- | --- | --- | --- |
| Interface layer | A vendored subset of the VST3 C++ SDK | Travesty, a pure C VST3-compatible interface | Use Travesty for the host ABI; avoid exposing C++ VST3 interfaces to LMMS |
| Engine integration | `VST3Plugin` is a native Ardour plugin | Generic backend plugin, also usable through a bridge | Add `Vst3Instrument`/`Vst3Effect` as LMMS plugins on top of a shared `Vst3Base` |
| Host callbacks | `VST3PI` implements component handler, unit handler and plug frame | Generic host callbacks in `CarlaPluginVST3` | Port the callback split from Ardour into a dedicated host object |
| Scanning | Separate `vst3-scanner` process and XML cache | Generic discovery tool and cache | Separate `lmms-vst3-scanner` process with JSON cache and rescan |
| Audio buses | Native multi-bus/multi-channel engine | Native multi-bus engine | Use the main stereo bus only; explicitly document ignored aux buses |
| Parameters | Full Ardour automation mapping | Generic parameter model | Map VST3 parameters to LMMS `FloatModel`s |
| State | Component + controller state in session XML | Component + controller state in plugin chunk | Store both states in the LMMS project |
| GUI | GTK/X11 socket + `IPlugView` attach | Generic `CarlaPluginUI` + `IPlugView` attach | Reuse LMMS's existing Qt/X11 host patterns; XWayland on Wayland |
| Crash isolation | Scanner isolated; DSP in Ardour process | Scanner and generic bridge; DSP can run in a child | Isolate the scanner and the DSP/UI host process |
| Build dependency | Optional `VST3_SUPPORT` | Bundled VST3/Travesty code | Optional `WANT_VST3`, no hard dependency for normal builds |

## 3. Recommended architecture

```text
LMMS process
  |
  +-- Vst3Instrument / Vst3Effect (LMMS Plugin subclasses)
  |     |
  |     +-- Vst3Plugin (model adapter)
  |           |
  |           +-- Vst3Host (host callbacks, parameters, state)
  |                 |
  |                 +-- Vst3RemoteClient (IPC)
  |
  +-- Vst3List / Vst3SubPluginFeatures
  |     |
  |     +-- Vst3Scanner cache (JSON)
  |
  +-- UI host widget / QMdiSubWindow / floating X11 window

lmms-vst3-scanner
  |
  +-- module load -> factory -> class metadata -> cache

lmms-vst3-host
  |
  +-- module load -> component/processor/controller
  +-- IComponentHandler / IPlugFrame
  +-- ProcessData / parameter queues / event lists
  +-- IPlugView -> X11 window
```

### Why Travesty

Travesty gives a stable C ABI over VST3:

- avoids C++ ABI mismatch between LMMS and plugins,
- makes a separate host process straightforward,
- is small and permissively licensed (ISC),
- is already proven by Carla.

The VST3 SDK itself remains a build-time metadata/license dependency only; the
host should not link plugin C++ interfaces directly.

## 4. Components

### 4.1 Scanner: `lmms-vst3-scanner`

- Executable built from the VST3 backend, not linked into `lmms`.
- Accepts a bundle path and writes a per-bundle cache file.
- Uses `ModuleEntry` / `GetPluginFactory` / `ModuleExit`.
- Enumerates every audio class in the factory, not just one plugin per file.
- Metadata per class:
  - bundle path,
  - class UID/TUID,
  - name, vendor, version, category, subcategories,
  - audio input/output bus and channel counts,
  - MIDI input/output counts,
  - SDK version,
  - last modified time and bundle hash.
- Timeout and crash handling are done by the parent, like the existing
  `VstList` scan and VST2 remote host.
- Cache location: `~/.cache/lmms/vst3-list.json`.
- Scan paths:
  - configured VST3 path,
  - `VST3_PATH`,
  - `~/.vst3`,
  - `/usr/local/lib/vst3`,
  - `/usr/lib/vst3`,
  - platform-specific Windows/macOS paths.
- A rescan action is exposed in the instrument browser and the Add effect
  dialog, mirroring `VstList`/`VstSubPluginFeatures`.

### 4.2 Module loader: `Vst3Module`

- Resolve Linux bundle layout:
  `Foo.vst3/Contents/x86_64-linux/Foo.so`.
- Load with `dlopen`/`LoadLibrary` and resolve:
  - `ModuleEntry`,
  - `ModuleExit`,
  - `GetPluginFactory`.
- Hold the factory and module handle for the lifetime of the host.
- Support bundles with multiple classes and multiple factory versions.
- Never load the module in the LMMS GUI process; the scanner and DSP host do it.

### 4.3 Host object: `Vst3Host`

Implements the VST3 host side:

- `IComponentHandler`:
  - `beginEdit`, `performEdit`, `endEdit`, `restartComponent`.
- `IComponentHandler2`:
  - dirty state, open editor, group edits.
- `IUnitHandler`:
  - program/unit changes.
- `IPlugFrame`:
  - `resizeView` for the editor.
- `IHostApplication`:
  - host name, vendor, attributes.
- Optional content-scale interfaces:
  - `IPlugViewContentScaleSupport` if available,
  - PSL/PreSonus extensions only if they are safe to support.

The host owns:

- `IComponent`,
- `IAudioProcessor`,
- `IEditController`,
- parameter descriptors,
- state streams,
- process context,
- event and parameter queues.

### 4.4 LMMS model adapter: `Vst3Plugin`

- Base class for `Vst3Instrument` and `Vst3Effect`.
- Stores:
  - bundle path,
  - class UID,
  - plugin name/vendor,
  - editor visibility,
  - chunk/state.
- Exposes:
  - audio input/output ports (stereo only),
  - MIDI input,
  - parameter models,
  - preset/state operations.
- Project key example:

```xml
<instrument name="vst3instrument">
  <vst3controls plugin="/usr/lib/vst3/Foo.vst3"
                class="0123456789abcdef..."/>
  <key>
    <attribute name="file" value="/usr/lib/vst3/Foo.vst3"/>
    <attribute name="class" value="0123456789abcdef..."/>
  </key>
</instrument>
```

The class UID is mandatory because one VST3 bundle can contain many plugins.

### 4.5 Audio policy

LMMS is a stereo host. VST3's bus model is richer, so the policy must be
explicit and visible:

- Select the main audio input and output buses.
- Use the first two channels of each main bus.
- Ignore aux/sidechain buses in the first release.
- If a plugin has no main output, treat it as unsupported.
- If a plugin has more than two main output channels, use the first two and log
  a warning.
- Set a stereo bus arrangement where the plugin supports it.
- Map mono plugin layouts to LMMS mono/stereo as the VST2 path does.
- Never silently drop a plugin that cannot produce a stereo output; mark it in
  the browser as unsupported.

### 4.6 Parameters and automation

- Build one LMMS `FloatModel` per automatable VST3 parameter.
- Convert between normalized `[0, 1]` and plain values through
  `IEditController`.
- UI edits:
  - `beginEdit` -> start gesture,
  - `performEdit` -> update model,
  - `endEdit` -> end gesture.
- Host automation:
  - queue parameter changes with sample offsets,
  - feed `IParameterChanges` in `ProcessData`,
  - read output parameter changes back.
- Program changes and unit selection:
  - support the standard program list,
  - defer note expression and complex unit hierarchies.

### 4.7 State and presets

- Save component state and controller state separately in one LMMS state blob.
- Include a marker/version so future formats can migrate.
- On load:
  1. restore component state,
  2. restore controller state,
  3. refresh parameter values,
  4. resync the editor.
- Support `.vstpreset` import/export as a later feature; do not block the first
  release on it.
- Preserve old VST2/VeSTige projects unchanged.

### 4.8 GUI

- Linux X11:
  - create a native Qt/X11 parent window,
  - `IPlugView::attached(parent, "X11EmbedWindowID")`,
  - use `getSize` / `canResize` / `onSize`,
  - handle focus and scroll forwarding.
- Wayland:
  - VST3 editors are still X11-based; open them in an XWayland window.
  - Do not attempt native Wayland embedding.
- xcb:
  - embed in a `QMdiSubWindow` when possible,
  - fall back to a floating window.
- Window ownership:
  - the host process owns the editor window,
  - LMMS embeds or floats the X11 window by ID,
  - WM close hides the editor and keeps the instance alive,
  - unexpected destroy tears down the editor and allows a fresh reopen.
- Resize:
  - host window is the master for user resizes,
  - resize the plugin view and ignore echoed size notifications,
  - respect `getSizeConstraint` when available.
- HiDPI:
  - pass the device pixel ratio through `IPlugViewContentScaleSupport` when
    supported,
  - otherwise scale the host window and document the limitation.

### 4.9 Process isolation

Two-stage approach:

**MVP:** run DSP and editor in the LMMS process, but scanning in a child.

**Production:** run DSP and editor in `lmms-vst3-host`:

- host process loads the module and owns component/controller/view,
- LMMS communicates audio, parameters, MIDI and state over IPC,
- LMMS embeds the host's X11 window,
- host crash is reported as a plugin crash, not an LMMS crash,
- host is terminated with the LMMS process (`PR_SET_PDEATHSIG` on Linux),
- no orphan windows after an LMMS crash.

Reuse the existing `RemotePlugin`/shared-memory transport where possible, but
use a dedicated VST3 message protocol rather than overloading VST2 messages.

## 5. Phased plan

### P0: Design and licensing review

Deliverables:

- final architecture decision,
- VST3/Travesty licensing review,
- list of supported platforms,
- build option design.

Estimate: 1-2 weeks.

### P1: Scanner and metadata cache — implemented

Deliverables:

- [x] `lmms-vst3-scanner`,
- [x] `Vst3Module` loader,
- [x] `Vst3List` JSON cache,
- [x] bundle/class enumeration,
- [x] rescan entry in both plugin selection UIs,
- [x] timeout/crash handling.

The scanner runs per bundle with a 60 second timeout. Scan failures are
cached together with the module's mtime and size, and the cache is
invalidated when the scanner protocol version changes.

Estimate: 2-3 weeks.

### P2: Minimal in-process stereo host — implemented

Deliverables:

- [x] `Vst3Host` with component/controller setup,
- [x] stereo audio processing through `ProcessData`,
- [x] instrument and effect wrappers,
- [x] MIDI note on/off and basic CC,
- [x] one known VST3 instrument and one effect working.

The wrappers are `Vst3Instrument` and `Vst3Effect`. Parameter models and a
usable parameter UI are deliberately deferred to P3; the current control
dialogs only show plugin information.

Estimate: 2-3 weeks.

### P3: Parameters, automation and state — partially implemented

Deliverables:

- [x] parameter models and automation (per-block and sample-offset queue),
- [~] component/controller state save/load (works with sfizz; Surge XT and
  Dexed still reject the host stream),
- [ ] program/unit basics,
- [ ] project round-trip tests.

Estimate: 2-3 weeks.

### P4: Native editor

Deliverables:

- `IPlugView` attach on xcb and XWayland,
- resize/show/hide/close handling,
- editor lifecycle and crash recovery,
- DPI/content-scale handling where supported.

Estimate: 2-3 weeks.

### P5: Out-of-process DSP/UI host

Deliverables:

- `lmms-vst3-host`,
- IPC transport for audio/parameters/MIDI/state,
- window-ID handoff and embedding,
- crash recovery and orphan cleanup,
- watchdog/timeout behavior.

Estimate: 3-5 weeks.

### P6: Packaging, documentation and CI

Deliverables:

- `WANT_VST3` build option and CMake install rules,
- AppImage/Windows/macOS packaging,
- user documentation and limitation list,
- CI build with VST3 on/off,
- fake VST3 test module for automated tests,
- release notes.

Estimate: 2-3 weeks.

### Overall

- MVP (P1-P4, in-process, stereo): roughly 8-12 weeks for one experienced
  developer.
- Production-quality with isolation and packaging (P1-P6): roughly 3-5 months.
- Estimates assume no major upstream churn in Travesty or the VST3 ABI.

## 6. Testing strategy

### Unit / integration

- Fake VST3 module with:
  - one instrument class,
  - one effect class,
  - multiple buses,
  - parameters and state,
  - a deliberately crashing class for scanner tests.
- Test:
  - bundle resolution,
  - factory/class enumeration,
  - cache invalidation,
  - stereo bus selection,
  - parameter round-trip,
  - state save/load,
  - editor attach/detach.

### Real plugins

On this machine:

- Surge XT VST3 (instrument and effects),
- additional open-source VST3 plugins in a test VM,
- at least one plugin with a separate controller,
- at least one plugin with multiple audio buses,
- at least one plugin that refuses embedding.

### Manual matrix

- xcb embedded,
- xcb floating,
- Wayland + XWayland floating,
- show/hide/reopen editor,
- WM close and destroy,
- automation playback,
- project save/load,
- plugin crash during scan,
- plugin crash during processing,
- host process crash.

## 7. Risks

| Risk | Mitigation |
| --- | --- |
| VST3 SDK / Travesty licensing | legal review; keep the host optional and isolated |
| C++ ABI mismatch | use Travesty C ABI and a separate host process |
| Plugin crashes | separate scanner and DSP host, watchdog, blacklist |
| Multi-bus plugins | explicit stereo policy; warn/ignore aux buses |
| Wayland UI | XWayland floating window; no native embedding promise |
| Editor resize feedback loops | host-master resize, last-size suppression |
| State format changes | versioned state blob; preserve old projects |
| Scan time / hangs | per-bundle timeout, cache, rescan UI |
| Packaging size | optional build, separate scanner/host binaries |

## 8. Recommended next step

P1 is done: scanner, module loader and cache. The next step is **P2**, the
in-process stereo host. It adds `Vst3Host`, the `Vst3Instrument`/`Vst3Effect`
wrappers and MIDI input, and finally exposes the scanner list and its rescan
button through the normal plugin browser. Only after P2/P3 are stable should
the out-of-process host (P5) be attempted.
