# Unified plugin host framework

Status: design proposal. No code is changed by this document.

This document describes how VST2, VST3 and LV2 hosting can share one hosting
infrastructure without changing the plugin formats themselves. It is the design
spike that should happen before P6.5 (sidechain and multi-bus) so that bus
routing is implemented once instead of three times.

## Goal

One plugin lifecycle, one process supervisor, one scanner protocol, one editor
backend and one set of tests for every supported plugin format. Format-specific
semantics (parameters, state, buses, events) stay behind a small adapter
interface.

The LMMS model layer (`Instrument`, `Effect`, project XML) must not care which
format a plugin uses.

## Non-goals

- Unifying plugin semantics. VST2 has flat parameter indices and an opaque
  chunk, VST3 has component/controller state and normalised parameter IDs, LV2
  has ports and a state bundle. Those differences are real and stay in the
  adapters.
- Removing `Vestige` before the compatibility layer has shipped. Old projects
  store `<vestige>` / `<vstplugin>` nodes; those must keep loading.
- Adopting the Steinberg SDK. VST3 keeps using the vendored Travesty C ABI.
- OS sandboxing (seccomp, namespaces). That is a separate hardening step.

## Current state

| Concern | VST2 today | VST3 today | LV2 today |
| --- | --- | --- | --- |
| Model wrapper | `VestigeInstrument`, `VstEffect` | `Vst3Instrument`, `Vst3Effect` | `Lv2Instrument`, `Lv2Effect` |
| Host object | `VstPlugin` (`RemotePlugin`) | `Vst3HostBase` (`Vst3Host` / `Vst3RemoteHostClient`) | `Lv2Proc` + `Lv2HostClient` |
| Child process | `NativeLinuxRemoteVstPlugin64`, `RemoteVstPlugin64`, `32/RemoteVstPlugin32` | `lmms-vst3-host` (DSP), `lmms-vst3-editor-host` (editor) | `lmms-lv2-host` (DSP + UI) |
| Transport | shared audio + socket / shmFifo messages, block handshake | `Vst3SharedAudio` + POSIX semaphores, then JSON socket for the editor | shared audio + `RemotePluginBase` socket / shmFifo |
| Scanning | `VstList` probes each file in a child, JSON cache | `lmms-vst3-scanner` per bundle, JSON cache, per class | lilv metadata in process, no separate cache |
| Parameters | flat index, `parameterDump()` strings, chunk state | `IEditController` IDs, normalised values, component + controller state | ports with min/max/def, `state:interface` bundle |
| Editor | in the DSP child, X11 window ID | separate editor host, `IRunLoop`, XWayland floating | in the DSP child (required by `instance-access`) |
| Crash policy | child dies, host invalidates | DSP watchdog restart, editor crash isolated | host invalidates, UI window dies with the child, **Reload** |

VST3 has the most complete skeleton (scanner, DSP host, editor host,
watchdog). VST2 and LV2 each have their own half of the same machinery.

## Target architecture

```text
LMMS model layer
  HostedInstrument / HostedEffect        (Instrument / Effect subclasses)
  - one AutomatableModel per parameter
  - program / unit selection
  - project XML (legacy node names preserved)
              │
              ▼
PluginHost interface (per instance)
  load / unload / process / parameters / events / state / latency / editor
              │
     ┌────────┼─────────┐
     ▼        ▼         ▼
 Vst2Host  Vst3Host   Lv2Host             (format adapters)
     │        │         │
     └────────┴─────────┘
              │
Shared kernel
  ProcessSupervisor    QProcess, timeout, watchdog, PDEATHSIG, restart
  SharedAudio          planar stereo block + block handshake
  EventChannel         MIDI / atom / parameter queues
  PluginScanner        per-format scanner executable + common JSON schema
  EditorHost           in-process widget / X11 embed / remote editor process
```

### PluginHost interface

```cpp
enum class Format { Vst2, Vst3, Lv2 };
enum class ParameterKind { FlatIndex, NormalisedId, PortRange };
enum class StateKind { Chunk, ComponentController, Lv2Bundle };

struct HostCapabilities
{
    bool hasEditor;
    bool editorMustShareProcess;   // LV2 instance-access
    bool supportsPrograms;
    bool supportsUnits;
    bool sampleAccurateParameters;
    bool supportsMidiOut;
    bool multipleBuses;
    ParameterKind parameterKind;
    StateKind stateKind;
};

struct ParameterInfo
{
    uint32_t id = 0;
    QString name;
    QString unit;
    double min = 0.0;
    double max = 1.0;
    double defaultValue = 0.0;
    int stepCount = 0;
    bool automatable = true;
    bool readOnly = false;
    bool hidden = false;
};

class PluginHost
{
public:
    virtual ~PluginHost() = default;

    virtual bool load(const LoadRequest& request, QString* error) = 0;
    virtual void unload() = 0;
    virtual bool isLoaded() const = 0;
    virtual Format format() const = 0;
    virtual HostCapabilities capabilities() const = 0;

    virtual bool process(const ProcessBlock& block) = 0;
    virtual bool setParameter(uint32_t id, double value, int frameOffset) = 0;
    virtual double parameterValue(uint32_t id) const = 0;
    virtual void sendMidi(const MidiEvent& event) = 0;

    virtual int latency() const = 0;
    virtual bool setProgram(int index) = 0;
    virtual int currentProgram() const = 0;

    virtual bool saveState(StateBlob& state, QString* error) = 0;
    virtual bool loadState(const StateBlob& state) = 0;

    virtual EditorHandle createEditor(quintptr parentWindow, QString* error) = 0;
};
```

`EditorHandle` covers both worlds: an in-process `QWidget*` for UIs that must
share the process (LV2) or a remote window handle/description for the editor
host. The host object reports which one it produced.

### Adapter mapping

| Interface | VST2 adapter | VST3 adapter | LV2 adapter |
| --- | --- | --- | --- |
| `ParameterInfo` | flat index + dump min/max | `IEditController` normalized | port range + `lv2:designation` |
| `parameterValue` | `parameterDump()` | `getParamNormalized` | cached control values |
| `StateBlob` | chunk + parameter list | component + controller streams | `lv2state` file bundle |
| `process` | `VstPlugin::process` | `Vst3HostBase::process` | `Lv2Proc::run` / `Lv2HostClient::processBlock` |
| `sendMidi` | `IdMidiEvent` | `Vst3HostBase::queueMidi` | `handleMidiInputEvent` |
| `latency` | `initialDelay` | `getLatencySamples` | `lv2:latency` port |
| `createEditor` | child X11 window | editor host / in-process view | child X11 window (required) |

### Process supervisor

Extract the proven pieces:

- `QProcess` start and watcher thread
- `PR_SET_PDEATHSIG` and parent polling in the child
- instantiate timeout
- audio-block watchdog
- crash notification, restart policy, cached state for restart
- parent-death cleanup

VST2's `RemotePlugin`, VST3's `Vst3RemoteHostClient` and LV2's `Lv2HostClient`
should all use the same supervisor instead of three independent
implementations.

### Transport

Two transports exist today and both are proven:

- VST2 and LV2: shared audio + `RemotePluginBase` message channel.
- VST3: shared audio + POSIX semaphores, JSON socket for the editor.

The unified kernel should keep one audio block protocol (planar stereo,
versioned handshake, timeout) and one event channel. The adapter chooses the
serialization of parameters/events, not the transport.

### Scanner protocol

One JSON schema for every format:

```json
{
  "format": "vst2|vst3|lv2",
  "path": "/path/to/plugin",
  "uniqueId": "…",
  "name": "…",
  "vendor": "…",
  "version": "…",
  "isInstrument": true,
  "classes": [ { "id": "…", "name": "…", "isInstrument": true } ],
  "parameters": [ { "id": 0, "name": "…", "min": 0, "max": 1 } ],
  "error": null
}
```

- VST2: probe in a child (`VstPlugin` with `skipInit`).
- VST3: `lmms-vst3-scanner` per bundle.
- LV2: lilv metadata, optionally in a scanner child for untrusted bundles.
- Cache: one file per format under `~/.cache/lmms/`, same incremental rules
  (mtime/size), same "rescan" entry points.

### Editor host

Three modes behind one interface:

1. **In-process Qt widget** — required when the format needs a shared address
   space (LV2 `instance-access`, some VST2 Wine cases).
2. **X11 embedding** — the child owns the window, the host passes an XID.
3. **Remote editor process** — VST3 today; could serve future formats.

The capability `editorMustShareProcess` decides which mode is legal.

## Migration plan

The migration must never leave projects unloadable or lose the VST2 32-bit/Wine
path.

### Stage 0 — this document

- Define the interface and the capability matrix.
- Decide the transport and scanner schema.
- Acceptance: review agrees on the boundaries; no code changed.

### Stage 1 — shared kernel

- Extract `ProcessSupervisor` and `SharedAudio` from VST3 and `RemotePlugin`.
- Port `Lv2HostClient` and `VstPlugin` to the supervisor.
- Keep every wire format and environment switch unchanged.
- Acceptance: existing ctest, LV2 crash tests, VST2 smoke test.

### Stage 2 — `PluginHost` and adapters

- `Vst2Host` wraps `VstPlugin`.
- `Vst3Host` implements the interface directly.
- `Lv2Host` wraps `Lv2Proc` (in-process) and `Lv2HostClient` (remote).
- Wrappers use the interface for play, parameters, state and latency.
- Acceptance: all three formats play, automate, save and reload projects;
  parity with the current behavior.

### Stage 3 — generic wrappers and Vestige compatibility

- Introduce `HostedInstrument` / `HostedEffect`.
- `VestigeInstrument` becomes a thin VST2 alias; XML nodes `<vestige>` and
  `<vstplugin>` keep their exact meaning.
- Acceptance: old projects load and save byte-compatible state; VST2 UI
  behavior unchanged.

### Stage 4 — scanner unification

- Move VST2 probing and LV2 metadata behind the scanner protocol.
- Keep the existing cache files readable during the transition.
- Acceptance: same plugin lists, rescan behavior and startup time.

### Stage 5 — editor host unification

- One editor backend with the three modes above.
- Migrate Vestige's MDI subwindow, LV2's floating window and VST3's editor host.
- Acceptance: the UI audit matrix (sizes, crash isolation, Wayland fallback).

### Stage 6 — remove Vestige code

- After one release with the compatibility alias in place.
- Keep the project loader mapping old nodes to `HostedInstrument`.
- Acceptance: an old project with VST2, VST3 and LV2 tracks loads unchanged.

## Compatibility constraints

- Project XML node names and attributes must not change:
  `<vestige>`, `<vstplugin>`, `<vst3instrument>`, `<lv2instrument>`,
  `<lv2effect>`, `<effect name="vst3effect">`.
- Existing environment switches stay valid: `LMMS_LV2_REMOTE`,
  `LMMS_LV2_INPROCESS`, `LMMS_LV2_UI_SCALE`, `LMMS_VST3_REMOTE_DSP`,
  `LMMS_VST3_EDITOR_LOCAL`, `LMMS_VST3_HOST`, `LMMS_VST3_EDITOR_HOST`,
  `LMMS_VST3_SCANNER`.
- VST2 32-bit under Wine keeps its own host executable and test matrix.
- LV2 editors that require `instance-access` must stay in the DSP process; the
  interface must never force them into a remote editor host.
- State formats remain readable: VST2 chunks, VST3 component/controller
  streams, LV2 state files.

## Testing strategy

- **Host contract tests**: one test binary per format's fake plugin
  (gain/MIDI/crash for LV2, fake VST2 MIDI, fake VST3 module) exercising the
  same `PluginHost` expectations.
- **Project round trips**: save/load per format with state, parameters and
  programs; compare audio and state.
- **Crash containment**: DSP crash, UI crash, editor crash, kill -9.
- **Scanner**: broken bundle, hanging bundle, rescan, cache migration.
- **Compatibility**: old project files, 32-bit VST, Wayland/XWayland.
- **Performance**: block round-trip overhead and startup time compared with
  the current paths.

## Risks

| Risk | Mitigation |
| --- | --- |
| VST3 code is new (recent play-handle crash) | extract only proven pieces; keep VST3 tests green per stage |
| LV2 instance-access UI cannot use a remote editor host | capability flag, keep editor in DSP child |
| Vestige XML/UI regressions | compatibility alias, old project tests before removal |
| 32-bit Wine path breaks | keep the existing VST2 child and test matrix; no changes in stage 1 |
| Transport rewrite destabilizes timing | keep the two proven transports, unify the supervisor first |
| Scope creep into P6.5 | bus model is designed in stage 0 but implemented in P6.5 |

## Open questions

1. Keep `RemotePlugin` as the common transport, or introduce a new
   `PluginProcess` class with the VST3 semaphore protocol?
2. Should the generic parameter model live in `core` (so tests can use it
   without GUI) or stay in `gui`?
3. Do we keep the `Vestige` name for the VST2 wrapper in the UI, or rename it
   to "VST2" while keeping the XML node?
4. Does P6.5 use the unified interface from day one, or do we ship sidechain
   first for VST3 only and migrate LV2/VST2 later?

## Effort

| Stage | Estimate |
| --- | --- |
| 0 design | 1-2 weeks |
| 1 shared kernel | 3-4 weeks |
| 2 host interface and adapters | 3-5 weeks |
| 3 generic wrappers / Vestige alias | 2-3 weeks |
| 4 scanner unification | 2-3 weeks |
| 5 editor unification | 2-3 weeks |
| 6 Vestige removal | 1 week |
| Total | 3-4 months including tests |

## Relationship to the P6 plan

Recommended order:

1. P6.4 completion (done),
2. this design spike,
3. Stage 1 (shared kernel),
4. P6.5 sidechain/multi-bus implemented on the unified interface,
5. Stage 2-6 and P6.6 hardening in parallel.
