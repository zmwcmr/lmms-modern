# Plugin hosting hardening plan (P6)

Status: proposed roadmap, based on the current `feature/vst-improvements` state.

## Current state

### Done

- VST2 instrument/effect hosting through VeSTige/VstEffect and `VstBase`.
- VST2 remote host process, `PR_SET_PDEATHSIG`, crash containment.
- VST2 scan paths, JSON cache, incremental scan, rescan entry.
- VST2 state/chunk and fxp/fxb preset handling.
- VST2 effect/instrument editor embedding and show/hide.
- LV2 mono/stereo, state bundles, X11 UI, dynamic resize.
- LV2 UI instance lifetime, feature lifetime, valid `ui:resize` callback.
- LV2 `ui:showInterface`/`hideInterface`, HiDPI scale factor, WM close/destroy.
- LV2 multichannel plugins accepted; first stereo pair used.
- LV2 factory presets (`pset:Preset`) exposed through a controls-UI selector
  and restored with `lilv_state_restore`.
- Surge XT native UI enabled again.
- VST2 MIDI events from `audioMasterProcessEvents` forwarded to the track MIDI
  output.
- LV2 MIDI output parsed from the `m_midiOut` atom sequence and routed to the
  instrument track's MIDI output.
- Plugin-generated MIDI can target another song-level instrument track through
  the MIDI output page; one internal hop is supported.
- Stop/pause and playback jumps send All Notes Off (CC 123) to external MIDI
  outputs.
- VST2 `initialDelay` and LV2 `lv2:latency` output ports are queried and
  reported through `Plugin::latency()`; `EffectChain::latency()` sums effects.
- Track buses feeding the same mixer channel are delay-compensated against the
  maximum input latency, and mixer sends are compensated against the full
  channel latency graph.
- VST2 `audioMasterAutomate` and `audioMasterBeginEdit`/`EndEdit` forwarded to
  the host parameter models; gestures coalesce undo checkpoints.
- Build, unit tests, scripted checks, AppImage/tar.gz packaging.

### Known gaps

1. LV2 UI and DSP run in the LMMS process; P6.4 RemoteLv2 moves both into a
   host child.
2. PDC aligns track buses and mixer sends, and exports trim the master chain
   latency from the start of the stream. The latency values are not yet shown
   in the UI.
3. VST2 and LV2 MIDI output are forwarded to the track MIDI port or one
   internal target track; longer routing chains are still missing.
4. VST2 sample-accurate parameter automation is implemented for instruments and
   effects; LV2 control ports remain block-rate by design.
5. Only bundle presets are exposed; user presets and preset saving are missing.
6. VST2 host callback coverage is still incomplete: the file selector is not
   implemented, and several 2.4 callbacks remain stubs.
7. No sidechain/aux bus or multi-output routing.
8. 32-bit VST remains untested.
9. VST2 UI scale/DPI handling is not uniform.
10. No automated fake-plugin tests for host callbacks.

## Guiding principles

- Keep the existing stereo model correct before adding channels.
- Prefer process isolation for untrusted plugin code.
- Do not make the engine timing worse while chasing features.
- Every milestone must have a measurable acceptance test.
- Keep VST2/LV2 behavior changes backward compatible with saved projects.
- Do not mix VST3 work into this plan; VST3 has its own branch and plan.

## Milestones

### P6.0: Compatibility quick wins

Goal: remove the most visible plugin compatibility failures without touching
the audio graph.

Tasks:

- [x] VST2 `audioMasterProcessEvents`:
  - serialize `VstEvents`/`VstMidiEvent` into a new IPC message,
  - forward to the host-side `VstPlugin`,
  - expose as a MIDI output queue.
  - current status: instrument events are routed to the track MIDI output;
    effect routing is still pending.
- [x] VST2 `audioMasterBeginEdit` / `audioMasterEndEdit`:
  - map to parameter gesture begin/end,
  - update automation models without jumping.
  - current status: `audioMasterAutomate` is forwarded as well, and a
    begin/end pair creates one undo checkpoint per gesture. Automated or
    controlled models are not overwritten by plugin-originated changes.
- [x] VST2 `audioMasterGetDirectory` and file selector:
  - return a useful directory string,
  - implement a minimal host-side file selector or a safe failure.
  - current status: the plugin directory is returned; the file selector
    returns 0 so plugins fall back to their own dialog.
- [x] LV2 preset discovery:
  - enumerate `pset:Preset` nodes related to the plugin,
  - add a preset selector to the LV2 controls UI,
  - restore via `lilv_state_restore`, refresh port values.
  - current status: bundle labels and lazy `rdfs:seeAlso` loading implemented;
    user preset files are not listed yet.
- [ ] 32-bit VST smoke test:
  - build `RemoteVstPlugin32`,
  - test with one 32-bit VST2 plugin under Wine,
  - document the supported Wine/32-bit combinations.
- [x] VST2 UI scale investigation:
  - measure `effEditGetRect` and device pixel ratio,
  - decide between host-side scaling and plugin-native scaling.
  - current status: measured Pianoteq 8 STAGE at 864x888 and ZynAddSubFX at
    1181x659 for both VST2 and LV2 at `ui:scaleFactor` 1.0. Native Linux VST2
    editors always open separately; the `qt`/`xembed` setting only applies to
    Wine-hosted editors. The size audit is recorded in `doc/lv2-plugin-ui.md`.

Files:

```text
plugins/VstBase/RemoteVstPlugin.cpp
plugins/VstBase/VstPlugin.cpp
plugins/VstBase/VstPlugin.h
plugins/VstBase/communication.h
plugins/Vestige/Vestige.cpp
plugins/VstEffect/VstEffectControls.cpp
src/core/lv2/Lv2Manager.cpp
src/core/lv2/Lv2Proc.cpp
src/gui/Lv2ProcView.cpp
```

Acceptance:

- A MIDI-out VST2 plugin emits events visible to the host.
- A VST2 parameter gesture updates the host knob and creates one undo
  checkpoint when the gesture begins.
- An LV2 factory preset can be selected and restored.
- The 32-bit host loads one real 32-bit plugin without crashing LMMS.
- Existing VST2/LV2 projects still load.

Estimate: 2-3 weeks.

### P6.1: MIDI output and routing

Goal: let instrument/effect plugins generate MIDI for other LMMS instruments.

Tasks:

- [x] Add a host-side MIDI event queue for VST2 (`VstPlugin`).
- [x] Parse LV2 `m_midiOut` atom sequences into `MidiEvent`s.
  - current status: note on/off, pressure, CC, program change and pitch bend
    are forwarded; SysEx is skipped.
- [x] Add a MIDI routing target to instrument/effect tracks or the track's MIDI
      output port.
  - current status: plugin-generated MIDI can target another song-level
    instrument track; one hop is supported to prevent cycles. Effect-track
    routing is still pending.
- [x] Forward note on/off, CC, pitch bend and program changes.
- [x] Handle all-notes-off on stop/seek.
  - current status: external MIDI outputs receive CC 123 on stop, pause and
    playback jumps; internal notes are cleared by the audio engine.
- [x] Add a test plugin that emits a fixed note sequence.
  - current status: `tests/plugins/fake-midi-vst.cpp` is built as the
    `FakeMidiVst` test target. The automated routing test still needs to be
    wired into ctest (see P6.6).

Files:

```text
plugins/VstBase/VstPlugin.*
plugins/VstBase/RemoteVstPlugin.cpp
src/core/lv2/Lv2Proc.*
plugins/Lv2Instrument/Lv2Instrument.*
plugins/Lv2Effect/Lv2Effect.*
src/tracks/InstrumentTrack.*
src/core/MidiClient.* (if needed)
```

Acceptance:

- A MIDI-out arpeggiator drives a second instrument track.
- LV2 note generator output reaches the target track.
- No stuck notes after stop/seek.
- Existing MIDI input behavior is unchanged.

Estimate: 2-4 weeks.

### P6.2: Plugin delay compensation (PDC)

Goal: keep parallel tracks and effect chains phase-aligned.

Tasks:

- [x] Design spike for the latency graph and delay buffers:
  - current status: `doc/pdc-design.md` defines the path model, delay-line
    placement, staged implementation and acceptance tests.
- [x] Add a latency query to the plugin abstraction:
  - VST2: `initialDelay` and `audioMasterGetOutputLatency`,
  - LV2: `lv2:latency` control port / reportLatency.
  - current status: VST2 initialDelay and LV2 designated latency ports are
    queried; VST2 re-reports initialDelay when it changes during processing.
- [x] Report latency through `Plugin`/`Effect`/`Instrument`.
  - current status: `Plugin::latency()` is implemented for VST2 and LV2, and
    `EffectChain::latency()` sums only the effects that are processing audio.
- [x] Add a latency graph to the engine:
  - per-track latency,
  - effect-chain latency,
  - mixer bus latency,
  - maximum latency alignment across buses and sends.
  - current status: the graph propagates latency through the send DAG and
    aligns both buses and send routes; `Mixer::totalLatency()` includes the
    master insert latency.
- [x] Delay compensation during playback and export (first stage).
  - current status: track-to-channel and send alignment works in both paths;
    the renderer trims `Mixer::totalLatency()` from the start of the exported
    stream. Latency reporting in the UI is still missing.
- [x] Update routing when plugin latency changes dynamically.
  - current status: the graph is rebuilt before and after every mixer round.
    VST2 reports `initialDelay` changes, instrument tracks refresh their bus
    latency from a GUI timer, and VST2/LV2 instruments publish it from
    `play()` so headless renders see runtime changes too.
- [x] Warn when the required delay exceeds the delay-line capacity.
- [ ] Add a committed impulse/phase test project.
  - `DelayLineTest` covers delayed impulses and block boundaries; the full
    mixer impulse project is still verified with temporary VST2 plugins.

Files:

```text
include/Plugin.h
include/Effect.h
src/core/Plugin.cpp
src/core/Effect.cpp
src/core/EffectChain.*
src/core/AudioEngine.*
src/tracks/InstrumentTrack.*
src/tracks/SampleTrack.*
src/core/lv2/Lv2Proc.*
plugins/VstBase/VstPlugin.*
```

Acceptance:

- A zero-latency and a high-latency plugin in parallel stay aligned.
- Changing plugin latency during playback does not produce permanent drift.
- Export matches real-time playback alignment.
- The compensation is visible in the project/status UI.

Estimate: 4-6 weeks.

### P6.3: Sample-accurate automation

Goal: make fast parameter automation land at the intended sample position.

Tasks:

- [x] Design spike for sample-accurate automation:
  - current status: `doc/automation-timing-design.md` defines host-side curve
    compression, the `IdVstParameterEvents` protocol, VST2 sub-block
    processing, MIDI re-basing, LV2 limitations and tests.
- [x] Add a parameter event queue with sample offsets to VST2.
  - `IdVstParameterEvents` carries `(frame, value)` points and is handled on
    the remote processing thread.
- [x] Split process blocks at automation event boundaries for VST2.
  - `RemoteVstPlugin::process()` splits at event frames and re-bases MIDI
    events per sub-block. Without events the old single call is unchanged.
- [x] Apply automation-clip changes at their tick position.
  - `Song::currentFrameOffsetInPeriod()` gives the tick's frame offset inside
    the audio period; `VestigeInstrument` sends a VST2 event at that offset.
- [x] Apply CC/pitch-bend events at their offsets.
  - already forwarded as `VstMidiEvent::deltaFrames`; now re-based when a
    block is split.
- [x] Preserve parameter changes across block boundaries.
  - events are queued per block and applied exactly once; no events means the
    original block-at-once path.
- [x] Add tests comparing rendered automation timing.
  - `ParameterCurveTest` covers curve compression. A fake VST2 parameter
    plugin with a tick-3 step rendered the transition at sample 1504 instead
    of the block boundary.
- [x] Route VST effect parameter automation through the same event path.
  - current status: `VstEffectControls` sends automated and controller-driven
    parameters through `compressParameterCurve()`/`setParamEvents()`;
    manual knob changes keep the block-start scalar path.
- [x] For LV2, evaluate block splitting vs. control-port limitations:
  - current status: `doc/automation-timing-design.md` records the decision to
    keep LV2 control ports block-rate. Core LV2 has no sample-accurate
    parameter semantics, and splitting `run()` would multiply worker, option
    and UI callbacks. The limitation is documented in the LV2 UI guide.

Files:

```text
plugins/VstBase/VstPlugin.*
plugins/VstBase/RemoteVstPlugin.cpp
plugins/VstEffect/VstEffectControls.*
src/core/lv2/Lv2Proc.*
src/core/AutomatableModel.*
src/core/AutomationClip.*
```

Acceptance:

- A short parameter ramp has the expected onset in a rendered file.
- No clicks due to parameter jumps at block boundaries.
- Automation in exported audio matches real-time playback.

Estimate: 3-5 weeks.

### P6.4: RemoteLv2 process isolation

Goal: a crashing LV2 plugin, UI included, must not kill LMMS.

Why the UI cannot be moved out on its own:

- `instance-access` hands the UI a raw pointer to the DSP instance. A pointer
  has no meaning in another process.
- `data-access` is process-local as well: the extdata pointer and everything it
  returns live in the host address space.
- Only serialized channels survive the boundary: atom ports, `state:interface`,
  shared memory, or a plugin-specific protocol such as Zyn Fusion's OSC/UDP.

The installed plugins confirm the scope: DrumGizmo, sfizz, OB-Xf, Pianoteq 8
STAGE, Surge XT and Surge XT Effects declare `instance-access` as a required
UI feature. A UI-only remote host would therefore cover almost nothing; Zyn
Fusion (DPF UI over OSC) is the exception.

The unit of isolation is consequently the plugin instance, not the UI: a
`lmms-lv2-host` child runs the DSP and the UI in one process, the way
`RemoteVstPlugin` already does for VST2. `instance-access` keeps working
because both sides live in the child. `doc/lv2-host-design.md` holds the
protocol.

The UI always opens in its own X11 window. Embedding it in an MDI subwindow was
removed together with the in-process UI path, so this milestone has no
window-ID or XEmbed work.

Tasks:

- [x] Implement `lmms-lv2-host`: instantiate the plugin, provide map/unmap,
      options, worker and state features, run ports and atom events.
- [x] Implement `Lv2HostClient` on the VST2 remote transport: control channel,
      shared audio/MIDI/control segment, block handshake, `QProcess` watcher.
- [x] Reuse the VST2 lifecycle rules: `PR_SET_PDEATHSIG`, instantiate timeout,
      watchdog, crash notification, reload.
- [x] Create the suil UI in the child and show it as a top-level X11 window;
      forward geometry, visibility, close, `ui:scaleFactor` and idle. Wayland
      uses an XWayland window, as today.
- [x] Exchange state and factory presets over the filesystem.
- [x] Keep the in-process path behind `LMMS_LV2_INPROCESS=1`; it also uses a
      separate window because embedding is gone. The isolated host is the
      default on Linux since the reload/crash UX landed.
- [x] Demote the `pluginUiIsUnstable()` list once the child path is the default.
      The list is empty.
- [x] Add fake test plugins that crash in UI and in DSP. `fake-lv2-crash-dsp`
      aborts in `run()`, `fake-lv2-crash-ui` aborts in the UI instantiate
      callback; `Lv2HostClientTest` proves both are contained.
- Current status: complete and enabled by default on Linux. `lmms-lv2-host`
  runs the DSP and the UI in one child, and the host talks to it over the
  `RemotePluginBase` transport with a stereo shared segment, MIDI events,
  control values, latency, LV2 state and factory presets. The UI opens in the
  child's own X11 window; a crash closes the window and enables **Reload
  Plugin**, which recreates the instance. `LMMS_LV2_INPROCESS=1` forces the
  old in-process path and `LMMS_LV2_REMOTE=0` disables the isolated host if it
  cannot start. The audit matrix (DrumGizmo, ZynAddSubFX, OB-Xf, Pianoteq 8
  STAGE, sfizz, Surge XT) matches the in-process sizes and renders;
  `Lv2HostClientTest` passes under AddressSanitizer. `cmake --install` places
  `lmms-lv2-host` next to the plugin modules. Remaining: only the first two
  audio ports are routed, UI atom events are not forwarded, and the fake
  crashing plugins still belong to P6.6.
- Acceptance: audio parity with the in-process path, PDC unchanged, project
  save/load round trip, `kill -9` of the child leaves LMMS alive, every audited
  plugin opens its UI in the child, and a UI crash only takes the child down.
- Optional follow-up: split UI and DSP into separate children for plugins with
  a serialized UI channel (Zyn Fusion), if UI crashes turn out to be frequent.

Files:

```text
src/core/lv2/Lv2Proc.*
src/gui/Lv2ProcView.*
plugins/Lv2Instrument/Lv2Instrument.*
plugins/Lv2Effect/Lv2Effect.*
new: src/core/lv2/Lv2HostClient.*
new: src/core/lv2/Lv2HostProtocol.h
new: plugins/Lv2Host/Lv2Host.cpp
tests/plugins/ (fake crashing LV2 bundles)
```

Estimate: 4-6 weeks.

### P6.5: Sidechain and multi-bus

Goal: support at least one aux/sidechain input and multiple outputs without
breaking the stereo path.

Design note: the bus model should be implemented once, on the unified host
interface described in `doc/plugin-host-unification.md`; otherwise VST2, VST3
and LV2 each grow their own copy of the routing code.

Tasks:

- [x] Add bus descriptors to `PluginHost` (`PluginBusInfo`,
      `PluginBusRole`) and a bus-aware `PluginProcessBlock`; the legacy flat
      fields still describe one main bus.
- [x] Report real LV2 buses: main and sidechain audio ports are grouped by
      `lv2:isSideChain`. This also fixed the property name (it was checked as
      `isSidechain`, so LV2 sidechain ports were never detected). The new
      `fake-lv2-sidechain` test plugin covers the layout.
- [x] Report VST3 bus layouts from the component's bus info (name, channels,
      main/aux type, active flag); the remote DSP client reports the main
      stereo buses because the bus layout is not yet in its protocol.
- [x] Report VST2 main input/output bus counts (channels are clamped to
      stereo); VST2 aux inputs are still not exposed.
- [x] Add an aux/sidechain bus to the LMMS effect routing model:
      `Effect::setSidechainBuffer()`, `EffectChain` propagation and
      `HostedEffect` passing a second input bus when the host has one.
- [x] Map one sidechain input to LV2 plugins: sidechain audio ports are now
      connected to their buffers and filled from the second input bus.
- [x] Mixer routing: a channel can select another channel as its effect
      sidechain, the worker graph tracks the extra dependency, cycles are
      rejected, and the route is saved as the `sidechain` XML attribute.
- [x] Route sidechain audio through VST3: `Vst3HostBase::processBuses()`
      carries every input/output bus, `Vst3Host` fills aux input buses and
      copies the output buses, and `Vst3PluginHost` maps the generic bus
      arrays. The fake VST3 effect now has a main and a sidechain input; the
      adapter test verifies the sidechain gain change.
- [ ] Route sidechain audio through VST2 (the current VST2 host exposes only
      the main stereo input).
- [ ] Add sidechain source selection to the mixer UI and an unsupported-bus
      warning.
- [x] Add multi-output plumbing and routing for two stereo pairs:
      `Effect`/`EffectChain` carry a secondary output buffer,
      `HostedEffect` writes output bus 1 into it, `MixerChannel` owns that
      buffer and can route it into another mixer channel with a worker
      dependency, cycle checks and the `multioutput` XML attribute. The
      channel context menu exposes the target selection.
- [ ] Add the second output pair to VST2 and to the remote VST3 protocol.
- [ ] Extend the mixer UI and project XML.
- [ ] Define a policy for unsupported bus counts.
- [ ] Add tests with a sidechain compressor and a multi-out sampler.

Files:

```text
include/Effect.h
include/EffectChain.h
src/core/EffectChain.*
src/gui/EffectView.*
src/gui/Mixer*
src/core/lv2/Lv2Proc.*
plugins/VstBase/VstPlugin.*
```

Acceptance:

- A sidechain compressor receives a real sidechain signal.
- Two stereo outputs can be routed independently.
- Existing stereo projects remain unchanged.
- Unsupported plugins are clearly marked, not silently broken.

Estimate: 6-8 weeks.

### P6.6: Hardening and release

Tasks:

- [~] Add fake VST2/LV2 plugins for automated tests: the LV2 gain, MIDI,
      DSP-crash and UI-crash bundles are built by `tests/CMakeLists.txt`; the
      VST2 fake MIDI plugin exists, a VST2 crash plugin is still missing.
- [ ] Add host callback/parameter/state tests to `ctest`.
- [ ] Add VST2 32-bit and LV2 crash tests to the manual matrix.
- [ ] Run AddressSanitizer on the host code.
- [ ] Update `doc/vst-plugin-hosting.md` and `doc/lv2-plugin-ui.md`.
- [ ] Add a CI build with plugin hosting enabled/disabled.
- [ ] Rebuild AppImage/tar.gz and verify plugin loading.

Estimate: 2-3 weeks.

## Recommended order

1. P6.0 compatibility quick wins
2. P6.1 MIDI output
3. P6.2 PDC
4. P6.3 sample-accurate automation
5. P6.4 RemoteLv2 isolation
6. Plugin host framework design + stage 1 shared kernel
   (`doc/plugin-host-unification.md`)
7. P6.5 multi-bus, implemented on the unified interface
8. P6.6 hardening

P6.0 and P6.1 can run in parallel. P6.2 should start with a design spike before
touching the audio graph. P6.4 is done; the framework design spike and the
shared kernel should land before P6.5 so the bus model is written once.

## What not to do in this plan

- Do not add VST3 here; use the separate `feature/vst3` branch and
  `doc/vst3-implementation-plan.md`.
- Do not add native Wayland embedding; XWayland is the supported path.
- Do not promise full surround/multi-bus in the first P6.5 step.
- Do not silently drop plugin latency; either compensate or document the
  limitation.

## Definition of done

- All existing tests pass.
- `check-namespace`, `check-strings` and `tests/scripted/verify` pass.
- AppImage and tar.gz build.
- A manual matrix covers:
  - VST2 instrument/effect,
  - LV2 instrument/effect,
  - LV2 floating UI on xcb and XWayland,
  - MIDI out,
  - PDC,
  - sample-accurate automation,
  - LV2 host crash,
  - 32-bit VST (if available).
- Existing VST2/LV2 projects load without data loss.
