# Sample-accurate automation design spike (P6.3)

Status: design document for the next P6.3 implementation step.

## Goal

Make plugin parameter automation land at the intended sample position instead
of the next audio-block boundary. A step that is written at frame 64 of a
256-frame block should change the plugin at frame 64, not at frame 0 or 256.

MIDI CC and pitch-bend events are already forwarded with frame offsets and are
handled by the plugin's own event timing. This document is about plugin
parameters (VST2 `effSetParameter`, LV2 control ports).

## Current behavior

- `VestigeInstrument` connects every `FloatModel::dataChanged` signal to
  `setParameter()` with a direct connection.
- `setParameter()` calls `VstPlugin::setParam()`, which sends one
  `IdVstSetParameter` message with the current scalar value.
- `RemoteVstPlugin` processes queued `IdVstSetParameter` messages before
  `IdStartProcessing`, so every change made during a period is applied at the
  start of the next `processReplacing()` call.
- `AutomatableModel::valueBuffer()` can provide per-frame values when a
  sample-exact controller is connected. For ordinary value changes it
  interpolates between the old and new scalar value over one period.
- LV2 control ports are block-rate by design. Some LV2 plugins expose
  sample-accurate parameter events through atom sequences, but LMMS currently
  writes control ports only.

Result: fast automation is quantized to the audio block size (for example
5.8 ms at 256 frames and 44.1 kHz), and short ramps are smeared over a whole
block.

## VST2 approach

VST2 has no per-event parameter API, so sample accuracy requires splitting the
block. The host and the remote plugin process share the same buffer size; the
host can call `processReplacing()` several times per block as long as the
sub-block sizes add up to the block size.

### Host side

Automation clips are evaluated at tick boundaries. When a clip changes a
parameter, `Song::processNextBuffer()` knows the frame offset of that tick
inside the current audio block. `VestigeInstrument` sends one event at that
offset. This is the maximum precision the automation system can provide
without changing the clip format. `ValueBuffer` interpolation is not used for
this case because it would start the change at frame 0 of the block.

1. In `VestigeInstrument::setParameter()`, ask the model for its
   `ValueBuffer` when the value comes from a controller rather than a clip.
2. If there is no per-frame data, or the model is not automated/controlled,
   keep the existing block-start `IdVstSetParameter` path.
3. If there is per-frame data, compress it into a bounded list of
   `(frameOffset, value)` points:
   - always include offset 0 when the value differs from the previous block,
   - include a point when the value changes by more than `1/4096` of the
     model range,
   - enforce a minimum spacing of 4 frames,
   - cap the list at 32 points per parameter and per block; if the cap is hit,
     keep the first and last points and evenly spaced points in between.
4. Send one `IdVstParameterEvents` message per automated parameter:
   `paramIndex`, `count`, then `count` pairs of `(frameOffset, value)`.
5. Skip duplicate sends within the same `AutomatableModel` period. This needs
   a public `periodCounter()` getter or a small counter in `VestigeInstrument`.

The compression is deliberately conservative: a linear ramp becomes a handful
of points, while a hard step becomes exactly one point at the step frame.

### Remote side

1. Add `IdVstParameterEvents` handling in `RemoteVstPlugin::processMessage()`.
   Store the points in a `std::vector<ParameterEvent>`; do not apply them yet.
2. In `RemoteVstPlugin::process()`:
   - sort the events by frame and clamp them to `[0, blockSize]`,
   - split the block at every distinct event frame,
   - apply all events at the current split point with `effSetParameter`,
   - call `processReplacing()` for `frames = nextFrame - currentFrame`,
   - offset `m_inputs`/`m_outputs` by `currentFrame` for each call,
   - clear the event list after the block.
3. MIDI events must be re-based per sub-block:
   - keep the existing `VstMidiEvent::deltaFrames` values as block-relative
     offsets,
   - before each sub-block, collect events whose frame is in
     `[currentFrame, nextFrame)`, subtract `currentFrame` from `deltaFrames`,
     and call `effProcessEvents`,
   - events at the block boundary are delivered once.
4. Keep the shared-memory lock held for the whole split sequence.

If the event list is empty, the existing single `processReplacing()` call is
kept unchanged. This is important for compatibility and CPU use.

### Effects

`VstEffect` has the same `VstPlugin` base and parameter model connections as
`VestigeInstrument`. Once the instrument path works, route effect automation
through the same `setParamEvents()` API. Effects with audio inputs need the
same input/output pointer offsetting.

## LV2 approach

- `Lv2Proc` currently writes scalar control ports before `run()`, so control
  changes are block-rate.
- Evaluate splitting the LV2 `run()` call at event boundaries only for plugins
  that expose sample-accurate parameter ports (atom sequence with parameter
  events). Keep the control-port path block-rate.
- Do not split for plugins without sample-accurate parameter input. Document
  the limitation in the plugin dialog or manual.

### Evaluation result (2026-11-25)

Decision: keep the LV2 control-port path block-rate and do not split `run()`
in this milestone.

Reasons:

- Core LV2 parameter changes are `patch:Set` messages on atom ports without a
  defined sample position. Even when a plugin accepts timestamped atom events,
  there is no standard parameter semantics tied to `time:Frame`; each plugin
  would need its own event type and mapping.
- `Lv2Proc::run()` also drives worker scheduling, options, state restore and
  UI notifications. Splitting it multiplies those callbacks per audio period
  and assumes the plugin is stateless between sub-block runs, which is not
  guaranteed.
- Most LV2 plugins smooth control changes internally, so the audible result of
  block-rate control is usually better than hard sub-block steps.
- The host still forwards MIDI with block-relative offsets; only the scalar
  control ports are quantized.

The limitation is documented in `doc/lv2-plugin-ui.md`. Revisit only when a
plugin with explicit sample-accurate parameter atoms becomes a target; in that
case gate splitting on the port's event type, not on "LV2 in general".

## Tests

Use a fake VST2 plugin with one parameter that maps to output gain or DC:

- Step test: automation jumps from 0 to 1 at frame 64. The rendered output
  must change at sample 64.
- Ramp test: a short linear ramp from 0 to 1 over frames 64-96. The first
  non-zero output and the midpoint must match the expected frames within the
  compression tolerance (a few samples).
- Block-boundary test: a change exactly at frame 0 and at frame 255 must not
  be applied twice or dropped.
- MIDI interaction: a note-on at frame 32 and a parameter step at frame 64
  must both keep their offsets.
- Export vs real-time: render the same project and compare the change frame.

The fake plugin should print or encode the gain per frame so the test can
locate transitions without relying on audio FFT analysis.

## Staged implementation

1. Protocol: `IdVstParameterEvents`, `VstPlugin::setParamEvents()`, remote event
   storage. No behavior change while the list is empty.
2. Host compression in `VestigeInstrument` for automated parameters, with a
   feature flag or a model check so manual knob changes keep the old path.
3. Remote sub-block processing for instruments, including MIDI re-basing.
4. Effects reuse.
5. LV2 evaluation and tests.

## Implementation status (2026-11-25)

The VST2 path is implemented:

- `IdVstParameterEvents` carries `(frame, value)` points.
- `VestigeInstrument` sends automation-clip changes at
  `Song::currentFrameOffsetInPeriod()` and controller curves through
  `compressParameterCurve()`.
- `VstEffectControls` uses the same helper: effect parameters are sent as
  offset events when the model is automated or driven by a sample-exact
  controller; manual knob changes keep the block-start scalar path.
- `RemoteVstPlugin::process()` splits the block at event frames, applies the
  points with `effSetParameter`, and re-bases MIDI events for each sub-block.
  With no events it keeps the original single `processReplacing()` call.
- `ParameterCurveTest` covers step, ramp, spacing and event-cap behavior.
- A fake VST2 parameter plugin with a step at tick 3 (110 BPM, 501.136 frames
  per tick) rendered the transition at sample 1504, matching the tick frame
  instead of the block boundary.

The LV2 control-port path is still block-rate; evaluating atom-sequence
parameters remains open.

## Risks

- Some plugins assume a constant block size or process the full shared buffer.
  Keep the old single-call path when no events are present, and cap the number
  of splits.
- Sub-block processing increases CPU use for fast automation. The 32-point cap
  bounds the worst case.
- `AutomatableModel::valueBuffer()` caches per period, so multiple automation
  updates inside one period may not be visible. Document this and revisit if
  automation clips become denser than one update per period.
- Parameter changes at sub-block boundaries can click. That is expected for
  hard steps; smooth ramps are the caller's responsibility.
- The IPC queue has a finite size. Cap points per parameter and total points
  per block, and drop extra points with a warning instead of blocking the
  audio thread.

## Non-goals for the first step

- Sample-accurate LV2 control ports (not possible without plugin support).
- VST3 parameter queues (separate branch).
- Automation for plugin GUI-only state that is not exposed as a parameter.
