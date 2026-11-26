# RemoteLv2 process isolation design (P6.4)

Status: design document, split into two stages: a remote plugin instance with a
floating UI, then cross-process embedding. No host process code exists yet.

## Goal

A crashing LV2 plugin, or a crashing LV2 UI, must not take LMMS down. Audio
should continue with the affected track safely silenced, and the plugin must be
reopenable without restarting the application.

The same host process interface should later be reusable for VST3, so the
protocol is described in plugin-neutral terms where possible.

## Current state

- `Lv2Proc` instantiates the plugin, owns its ports and runs `lilv_instance_run`
  in the LMMS process.
- `Lv2ProcView` creates the LV2 UI through suil in the same process and talks
  to it through direct pointers (`instance-access`, `data-access`).
- The UI and the DSP share the plugin instance, so a UI crash and a DSP crash
  are equally fatal.
- VST2 already uses the opposite model: `RemoteVstPlugin` runs in a child
  process and LMMS talks to it through `RemotePluginBase`.

## Why the plugin instance is the unit of isolation

Moving only the UI to a child process does not work for these plugins:

- `instance-access` passes a raw pointer to the DSP instance. A pointer is
  meaningless in another process.
- `data-access` is equally process-local: the extdata pointer and every
  structure it returns live in the host address space.
- Only serialized channels cross a process boundary reliably: atom ports,
  `state:interface`, shared memory, or a plugin-specific protocol.

Measured against the installed bundles, the UIs that require `instance-access`
are DrumGizmo, sfizz, OB-Xf, Pianoteq 8 STAGE, Surge XT and Surge XT Effects.
Zyn Fusion is the exception: its DPF UI talks to the engine over OSC/UDP
(`osc.udp://127.0.0.1:%d`) and declares no `instance-access`.

Therefore the child hosts the DSP **and** the UI. `instance-access` stays valid
because both sides are in the same process, no plugin needs a change, and a UI
crash is contained like a DSP crash. A UI-only remote host would only ever work
for Zyn Fusion and is not worth a separate code path.

The VST2 host provides the precedent to reuse:

| Concern | VST2 today |
| --- | --- |
| Control channel | `shmFifo` inside `RemotePluginBase` |
| Audio buffer | `SharedMemory<float[]>` created by the host |
| Block handshake | `IdStartProcessing` / `IdProcessingDone` |
| Process start | `QProcess` on a watcher thread |
| Orphan prevention | `PR_SET_PDEATHSIG` plus `getppid()` check |
| UI | X11 window owned by the child |

## Non-goals for the first step

- OS sandboxing (seccomp, namespaces, dropping privileges).
- Multi-bus and sidechain routing; that is P6.5.
- Lock-free, non-blocking audio transport. The first version keeps the VST2
  block handshake and can be optimized later.
- Windows and macOS. Linux is the target; the protocol should stay portable.
- Removing the in-process path. It remains a debug and fallback option.

## Process model

One `lmms-lv2-host` child per `Lv2Proc` instance, i.e. per plugin instance.

Reasons:

- A crash only loses one plugin, matching the VST2 containment model.
- UI and DSP crash handling share one lifecycle.
- Plugin scanning stays in-process: `lilv_world_load_all` reads metadata and
  does not run plugin code. Only `lilv_plugin_instantiate` moves to the child.

Alternatives considered:

- **Thread isolation.** A thread cannot contain a segmentation fault, so this
  does not meet the goal.
- **One host per mixer channel or per song.** Fewer processes, but a crash
  kills several plugins and all their edits. Rejected for the first step.
- **Shared plugin host for all LV2 instances.** Same crash-radius problem and
  a much more complex protocol. Rejected.

The executable is installed next to `RemoteVstPlugin64` and found through the
same `plugins:` search path, with `LMMS_PLUGIN_DIR` honored for development
builds.

An environment switch `LMMS_LV2_INPROCESS=1` keeps the current in-process path
for debugging and for platforms where the child cannot be built. The setting is
reported in the plugin dialog so a crash report is unambiguous.

## Architecture

```text
LMMS process                                  lmms-lv2-host child
+---------------------------+                 +---------------------------+
| Lv2Proc (front-end)       |                 | Lv2Host                   |
|  - port models            |   control FIFO  |  - lilv instance          |
|  - automation routing     |<--------------->|  - ports / worker         |
|  - state model            |                 |  - suil UI                |
| Lv2ProcClient             |   shared memory |  - LV2 features           |
|  - QProcess watcher       |<--------------->|  - atom / MIDI I/O        |
|  - audio copy             |                 |  - state save / restore   |
+---------------------------+                 +---------------------------+
```

`Lv2Proc` keeps the model-facing API so `Lv2Effect`, `Lv2Instrument` and the
views do not change conceptually. Internally it uses a new `Lv2ProcClient`
that implements `RemotePlugin`-style process management and the protocol.

## Transport

### Control channel

Reuse `RemotePluginBase` and its `shmFifo` pair, exactly like VST2: two shared
memory FIFOs, one in each direction, passed to the child as command-line
arguments. Message IDs start at `RemoteMessageIDs::IdUserBase` plus a new LV2
range, so the generic IDs stay compatible.

### Shared audio and event memory

The host allocates one shared segment per plugin and sends its key with
`IdChangeSharedMemoryKey`. All offsets are byte offsets from the segment start;
pointers are never stored in the segment because the two processes may map it
at different addresses.

```text
struct Lv2HostSegment
{
    Header header;                 // magic, version, frames, sample rate
    float  audioIn [portsIn ][frames];
    float  audioOut[portsOut][frames];
    ControlSlot controls[controlPorts];
    MidiRing midiIn;
    MidiRing midiOut;
    Status status;
};
```

- `Header` carries counts and is rewritten when the sample rate or period size
  changes.
- Audio port buffers are planar, matching LV2. The first step maps LMMS stereo
  to the first two audio input/output ports; extra ports are still connected so
  plugins that require them run, but they are not routed (same policy as
  today).
- `ControlSlot` contains `float value` and `std::atomic<uint32_t> version` for
  each control port, in both directions:
  - host-to-child: LMMS models and automation,
  - child-to-host: UI edits and plugin-driven output controls.
  The writer increments the version, the reader compares it, so no FIFO
  traffic is needed for knob movement.
- `MidiRing` is a single-producer/single-consumer byte ring. Each record is
  `{ int32 frame; uint16 size; uint8 data[]; }`.
- `Status` contains atomics for the child state (running, failed, UI ready) and
  the reported latency.

The segment is replaced when `framesPerPeriod` or the port count changes. The
host sends the new key before `IdStartProcessing`, and the child maps it between
blocks, never during a run.

### Block handshake

The first version keeps the VST2 sequence:

1. Host fills `audioIn`, MIDI ring and control slots.
2. Host sends `IdStartProcessing`.
3. Child copies MIDI into atom sequences, applies control versions, runs the
   plugin, collects outputs, writes MIDI output records and control versions.
4. Child sends `IdProcessingDone`.
5. Host copies `audioOut`, drains MIDI output and applies control changes.

This costs one inter-process round trip per block per plugin, which VST2
already pays. If profiling shows it matters, the next step is a double-buffered
segment plus a semaphore so the host never waits for the whole block.

## Protocol

New IDs, all below the LV2 host base:

| ID | Direction | Payload |
| --- | --- | --- |
| `IdLv2Instantiate` | host to child | URI, sample rate, block size, UI mode, parent XID |
| `IdLv2PortInfo` | child to host | port index, type, direction, flags, default/min/max |
| `IdLv2InstantiateFailed` | child to host | issue list (missing feature, failed instantiate) |
| `IdLv2Ready` | child to host | instance is running |
| `IdLv2Latency` | child to host | latency in frames (only on change) |
| `IdLv2ShowUI` | host to child | parent XID (0 in stage 1), scale factor |
| `IdLv2HideUI` | host to child | none |
| `IdLv2DestroyUI` | host to child | none |
| `IdLv2UiGeometry` | child to host | x, y, width, height |
| `IdLv2UiClosed` | child to host | reason (WM close, UI destroyed) |
| `IdLv2SaveState` | host to child | state directory path |
| `IdLv2StateSaved` | child to host | manifest of written files |
| `IdLv2RestoreState` | host to child | state directory path |
| `IdLv2StateChanged` | child to host | plugin marked state dirty |
| `IdLv2Reload` | host to child | new sample rate / block size |
| `IdLv2Quit` | host to child | none |

Port metadata is sent once, right after instantiation, and is enough for the
host to create the same `AutomatableModel` objects and labels the in-process
path creates today. Values and flags are sent as plain strings/floats over the
control FIFO; this is a one-time cost.

## Audio path

`Lv2Effect::processImpl` and `Lv2Instrument::play` keep their current shape:

1. Copy `SampleFrame` input into the shared input planes.
2. Publish MIDI input and control versions.
3. Run the block.
4. Copy output planes back.

The child owns the LV2-specific details:

- mono to stereo upmix and stereo to mono downmix,
- float port connection for optional ports,
- latency port update,
- worker `emitResponses` before `run()` and `notifyPluginThatRunFinished`
  after it,
- atom output forwarding.

If the child is gone, the client returns silence (effect) or zeros (instrument)
and reports the failure to the track. `EffectChain` already handles a silent
effect; the instrument path needs an explicit "failed, do not retry every
block" flag.

## MIDI path

- Host converts `MidiEvent` to raw bytes with its block-relative frame offset
  and writes them to `midiIn`.
- Child drains `midiIn` into the existing `Lv2Evbuf` MIDI atom port with the
  same offsets. Running status and SysEx are handled exactly as in `Lv2Proc`.
- Child collects output atoms into `midiOut`; host drains them after
  `IdProcessingDone` and reuses `Lv2Proc::collectMidiOutEvent`'s filtering.
- Ring overflow drops events and increments a counter reported on the next
  status message, instead of blocking the audio thread.

## UI hosting

The UI lives in the child because the installed UIs take `instance-access`;
see "Why the plugin instance is the unit of isolation". A UI crash is the most
common LV2 failure, so keeping the UI out of the LMMS process is the whole point
of this milestone.

Stage 1 shows the UI as a top-level window:

- Floating mode: the child owns a top-level X11 window. It reports geometry and
  the host moves/hides it through messages; `WM_DELETE_WINDOW` is handled in the
  child and reported as `IdLv2UiClosed`.
- Embedded mode in stage 1 falls back to that separate window, with the
  container replaced by a **Show UI** placeholder. Native Linux VST2 behaves the
  same way today, so this is not a new user experience.
- Wayland: the child opens an XWayland window; `DISPLAY` is forwarded.
- Scale factor: the host sends the device pixel ratio; the child sets
  `ui:scaleFactor` and `ui:windowTitle` before instantiating the UI.
- Control flow: the UI `write_function` writes into the child's port values and
  bumps the child-to-host `ControlSlot` version. The child applies the value to
  the plugin at the next block boundary, so UI edits are block-rate like any
  other control change.
- Plugin-to-UI control changes are pushed to the child's UI inside `run()`'s
  post-processing, as `Lv2Proc` does today.

Stage 2 adds embedding:

- the host passes the X11 window ID of the LMMS container to `IdLv2ShowUI`;
  the child creates the suil UI with that XID as `ui:parent`. Reparenting
  across processes is normal X11 behaviour, and `RemoteVstPlugin`'s `xembed`
  path proves the mechanism.
- Sizing parity: `ui:resize` is forwarded to the subwindow together with the
  title-bar allowance, workspace clamping and scroll bars that exist today.
- Wayland keeps the floating XWayland window because native Wayland embedding
  is not supported.

In both stages a UI crash terminates the child and therefore the DSP instance.
The host closes the container, marks the plugin failed, and offers **Reload**.
Splitting UI and DSP into two children is only worth doing for plugins with a
serialized UI channel (Zyn Fusion); it is an optional follow-up, not part of
the milestone.

## State and presets

State is exchanged through the filesystem, not the FIFO:

- Save: the host creates a temporary directory, sends its path with
  `IdLv2SaveState`, the child calls `lilv_state_new_from_instance` with
  `LILV_STATE_SKIP_RESET`, writes the state and referenced files there, and
  returns the file list. The host copies the files into the project's state
  bundle.
- Restore: the host extracts the bundle to a temporary directory, sends the
  path, and the child calls `lilv_state_restore`.
- Factory presets are enumerated by the child at instantiation because it
  already has the `LilvWorld`; the host receives `(label, URI)` pairs and asks
  the child to load one by URI. This avoids duplicating preset discovery in the
  host.
- The existing `pset:Preset` UI selector keeps working unchanged.

State calls must not race the audio thread. The child uses the same
`m_stateMutex` ordering as `Lv2Proc` today: block the control thread, not the
audio thread, and apply state between runs.

## Lifecycle and failure handling

- Start: `Lv2ProcClient::start()` launches the child with the two FIFO keys and
  waits for `IdLv2Ready` with a timeout (default 10 s for instantiate).
- Orphan prevention: the child calls
  `prctl(PR_SET_PDEATHSIG, SIGTERM)` and checks `getppid() == 1` right after
  start, same as `RemoteVstPlugin`.
- Crash detection: the existing `ProcessWatcher` invalidates the client. The
  host marks the plugin failed, closes any UI container, silences the output
  and posts one "plugin crashed" notification per failure (not per block).
- Watchdog: a GUI-thread timer checks the block handshake. If a plugin blocks
  longer than a configurable deadline (default 5 s), the child is terminated and
  reported as hung. The first version only warns in debug builds to avoid
  killing slow offline renders; export disables the watchdog.
- Reload: recreate the child, re-instantiate with the same URI and the current
  sample rate/block size, restore the last saved state if available, and rebuild
  the shared segment.
- Quit: send `IdLv2Quit`, close the UI, wait, then terminate/kill as
  `RemotePlugin` does today.

## Threading and real-time rules

- The host audio thread only touches the shared segment and the block
  semaphore. It must not allocate, call Qt, or take locks also used by the GUI.
- Control-slot versions are lock-free atomics; audio reads and writes are plain
  float accesses guarded by the version counter.
- The child's audio thread is the only thread that runs the plugin, exactly as
  today.
- UI calls stay on the child's main thread, never on its audio thread.

## Testing

Test plugins (built as small LV2 bundles under `tests/plugins`):

1. `FakeLv2Gain`: one control port, stereo audio, deterministic output.
2. `FakeLv2Midi`: emits a fixed note sequence and echoes input offsets.
3. `FakeLv2CrashDsp`: aborts on the Nth run when `FAKE_LV2_CRASH_DSP` is set.
4. `FakeLv2CrashUi`: aborts when its UI is opened and `FAKE_LV2_CRASH_UI` is
   set.

Automated checks:

- **Audio parity:** render the same project with the in-process and the child
  path; outputs match within float tolerance.
- **DSP crash:** `kill -9` the child and the crash plugin; LMMS stays alive,
  the effect outputs silence, the instrument produces no notes, and **Reload**
  works.
- **UI crash:** open the crashing UI; LMMS stays alive, the container closes,
  and the DSP failure is handled.
- **State round trip:** save a project with a state bundle, reopen it in the
  child path, and compare control values and files.
- **MIDI offsets:** a note at frame 32 and a parameter change at frame 64 keep
  their positions.
- **Latency:** reported latency reaches `Plugin::latency()` and PDC aligns the
  track.
- **Sample rate / block size change:** the segment is reallocated and audio
  continues without a crash.
- **ASan:** host and client run under AddressSanitizer for the crash tests.

Manual matrix additions:

- xcb embedded UI, floating UI and XWayland floating UI,
- Surge XT native UI (the most complex UI tested so far),
- a plugin with worker threads,
- force-kill during playback and during export.

## Staged implementation

### Stage 1: remote instance with a floating UI

1. **Scaffold.** `lmms-lv2-host` executable, `Lv2ProcClient` with FIFO setup,
   process start/watcher, shared segment allocation and a no-op block loop.
   Acceptance: a fake "plugin" echoes audio and survives a kill.
2. **DSP.** Move instantiation, ports, control slots, audio planes, latency and
   worker handling into the child. Keep UI disabled. Acceptance: audio parity
   and PDC.
3. **MIDI.** Move MIDI atom input/output and forwarding. Acceptance: fake MIDI
   plugin drives another track.
4. **UI.** suil in the child, top-level window, scale factor, close/destroy.
   Embedded mode shows a **Show UI** placeholder instead of the container.
   Acceptance: every audited plugin opens, Surge XT resizes and closes, and a UI
   crash does not kill LMMS.
5. **State, presets, failure UX.** State bundles over the filesystem, preset
   loading, reload path, crash notification, in-process fallback. Acceptance:
   full test matrix and ASan run.

### Stage 2: embedded UI and parity

6. **Embedding.** Pass the container XID with `IdLv2ShowUI`, create the suil UI
   with `ui:parent`, and reparent it into the MDI subwindow. Acceptance: the
   audit matrix opens embedded and floating with the current sizes.
7. **Parity.** Resize forwarding, title-bar allowance, workspace clamping,
   scroll bars, `LMMS_LV2_UI_SCALE`, reopen after a DSP crash and the
   `pluginUiIsUnstable()` demotion. Acceptance: UI crash closes the container
   but LMMS and the project survive.

Each stage keeps the in-process path selectable so a regression can be bisected
quickly.

## Risks

- **Per-block round trip.** Same cost as VST2 today, but LV2 chains can have
  more instances than VST2 chains. Measure before optimizing; the double-buffer
  design is the fallback.
- **Plugin initialization hangs.** `dlopen` and `instantiate` can block on
  global locks or display servers; the instantiate timeout and watchdog are
  required, not optional.
- **Cross-process X11 (stage 2).** Reparenting and focus handling differ between
  window managers. Keep floating mode as the fallback and test at least one
  tiling WM.
- **State files.** Paths inside state bundles must stay portable; the manifest
  needs relative names, not temporary absolute paths.
- **Packaging.** The new binary must be installed and discoverable in AppImage
  and tar.gz builds; add it to the runtime dependency check.
- **UI and DSP lifetime.** A UI crash kills the DSP instance because both live
  in the child. That is contained to the child and is the accepted trade-off for
  keeping `instance-access` valid. Splitting them is only possible for plugins
  with a serialized UI channel.
- **Stage 1 UX regression.** Embedded mode opens a separate window until stage 2
  lands. Document it in the release notes so it is not reported as a bug.
- **Feature parity.** `Lv2Proc` currently handles options, worker scheduling,
  latency reporting and UI notifications. The child must reach parity before
  the in-process path can be demoted to a fallback.

## Files

```text
new: plugins/Lv2Host/Lv2Host.cpp          (child main)
new: plugins/Lv2Host/CMakeLists.txt
new: src/core/lv2/Lv2HostClient.h/.cpp    (process + shared segment)
new: src/core/lv2/Lv2HostProtocol.h       (message IDs, segment layout)
include/Lv2Proc.h
src/core/lv2/Lv2Proc.cpp
src/core/lv2/Lv2Ports.cpp
src/gui/Lv2ProcView.cpp
plugins/Lv2Instrument/Lv2Instrument.cpp
plugins/Lv2Effect/Lv2Effect.cpp
tests/plugins/                            (fake LV2 bundles)
```
