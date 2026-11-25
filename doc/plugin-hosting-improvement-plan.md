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
  maximum input latency.
- VST2 `audioMasterAutomate` and `audioMasterBeginEdit`/`EndEdit` forwarded to
  the host parameter models; gestures coalesce undo checkpoints.
- Build, unit tests, scripted checks, AppImage/tar.gz packaging.

### Known gaps

1. LV2 UI and DSP run in the LMMS process.
2. Track-to-channel PDC is implemented, but send and master compensation is
   still missing.
3. VST2 and LV2 MIDI output are forwarded to the track MIDI port or one
   internal target track; longer routing chains are still missing.
4. No sample-accurate automation.
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
- [ ] VST2 UI scale investigation:
  - measure `effEditGetRect` and device pixel ratio,
  - decide between host-side scaling and plugin-native scaling.

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
    queried; runtime latency changes still need to be re-queried.
- [x] Report latency through `Plugin`/`Effect`/`Instrument`.
  - current status: `Plugin::latency()` is implemented for VST2 and LV2, and
    `EffectChain::latency()` sums the effects in a chain.
- [x] Add a latency graph to the engine (first stage):
  - per-track latency,
  - effect-chain latency,
  - mixer bus latency,
  - maximum latency alignment across buses feeding the same channel.
  - current status: track buses are aligned at mixer-channel inputs; send,
    master and cross-channel alignment are next.
- [x] Delay compensation during playback and export (first stage).
  - current status: track-to-channel alignment works in both paths; freeze
    and send/master compensation are still missing.
- [ ] Update routing when plugin latency changes.
- [ ] Add a committed impulse/phase test project.
  - verified locally with temporary VST2 impulse plugins (128-sample delay).

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

- [ ] Add a parameter event queue with sample offsets to VST2.
- [ ] Split process blocks at automation event boundaries for VST2.
- [ ] For LV2, evaluate block splitting vs. control-port limitations:
  - document that LV2 control ports are block-rate,
  - split only where a plugin exposes sample-accurate data.
- [ ] Apply CC/pitch-bend events at their offsets.
- [ ] Preserve parameter changes across block boundaries.
- [ ] Add tests comparing rendered automation timing.

Files:

```text
plugins/VstBase/VstPlugin.*
plugins/VstBase/RemoteVstPlugin.cpp
src/core/lv2/Lv2Proc.*
src/core/AutomatableModel.*
src/core/AutomationClip.*
```

Acceptance:

- A short parameter ramp has the expected onset in a rendered file.
- No clicks due to parameter jumps at block boundaries.
- Automation in exported audio matches real-time playback.

Estimate: 3-5 weeks.

### P6.4: LV2 process isolation

Goal: a crashing LV2 UI or DSP must not kill LMMS.

Tasks:

- [ ] Design a generic plugin host process interface (audio, parameters, MIDI,
      state, UI window ID).
- [ ] Implement `lmms-lv2-host`:
  - instantiate the plugin,
  - provide LV2 features (map/unmap, options, worker, state),
  - create the X11 editor and expose its window ID,
  - forward audio/control/atom events.
- [ ] Implement the LMMS-side client with shared memory and a control socket.
- [ ] Reuse the VST2 remote-host lifecycle rules:
  - parent-death signal,
  - timeout/watchdog,
  - crash notification and UI close,
  - reload/reopen path.
- [ ] Keep the current in-process path as a fallback or debug option.
- [ ] Add a test plugin that deliberately crashes in UI and in DSP.

Files:

```text
src/core/lv2/Lv2Proc.*
src/gui/Lv2ProcView.*
plugins/Lv2Instrument/Lv2Instrument.*
plugins/Lv2Effect/Lv2Effect.*
new: src/core/lv2/Lv2HostClient.*
new: plugins/Lv2Host/Lv2Host.cpp
```

Acceptance:

- Killing the LV2 host does not kill LMMS.
- The plugin can be reopened after a UI crash.
- Audio continues or the track is safely muted after a DSP crash.
- The project still saves/loads.

Estimate: 6-8 weeks.

### P6.5: Sidechain and multi-bus

Goal: support at least one aux/sidechain input and multiple outputs without
breaking the stereo path.

Tasks:

- [ ] Add an aux/sidechain bus to the LMMS effect/instrument routing model.
- [ ] Map one sidechain input to VST2/LV2 plugins where declared.
- [ ] Add multi-output track routing (first step: two stereo pairs).
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

- [ ] Add fake VST2/LV2 plugins for automated tests.
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
5. P6.4 LV2 isolation
6. P6.5 multi-bus
7. P6.6 hardening

P6.0 and P6.1 can run in parallel. P6.2 should start with a design spike before
touching the audio graph. P6.4 should not start until the generic host protocol
is agreed, because it will later be reused for VST3.

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
  - xcb embedded and Wayland floating UI,
  - MIDI out,
  - PDC,
  - sample-accurate automation,
  - LV2 host crash,
  - 32-bit VST (if available).
- Existing VST2/LV2 projects load without data loss.
