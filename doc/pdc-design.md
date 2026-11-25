# PDC design spike (P6.2)

Status: design document for the next P6.2 implementation step.

## Goal

Keep parallel audio paths sample-aligned when plugins report processing latency.
The first version should compensate paths inside the mixer graph; it should not
try to compensate latency inside every plugin.

Example: a drum track with a look-ahead compressor and a dry reference track
must stay phase-aligned, both during playback and export.

## Current graph

- `InstrumentTrack`/`SampleTrack` render into a `MixerChannel` through
  `Mixer::mixToChannel()`.
- A `MixerChannel` first sums its incoming sends (`m_receives`), then runs its
  effect chain (`m_fxChain`), then sends its output to the channels in
  `m_sends`.
- Channel 0 is the master. `Mixer::masterMix()` waits for channel dependencies
  and copies the master buffer into the output device buffer.
- Sends are parallel paths: a channel can feed several receivers, and a
  receiver can sum several senders.
- `EffectChain::latency()` already reports the sum of its effects.
- `Plugin::latency()` reports VST2 `initialDelay` and LV2 `lv2:latency`.

## Latency model

For each source-to-destination path:

```text
pathLatency = sourceLatency + sum(latency of every FX chain on the path)
```

At every summing node (a mixer channel input), each incoming path must be
delayed by:

```text
delay = maxIncomingLatency - pathLatency
```

A serial path with higher latency does not need an internal delay to remain
correct by itself; compensation is only required when two or more paths meet.
The master FX chain adds a constant offset to the final output. It does not
break internal alignment, but it must be included in the reported total latency
used for export offset and UI information.

## Buffer design

- Add a stereo delay line per compensated source/send path.
- Preallocate for a maximum supported latency (for example 2 seconds at the
  current sample rate). Report the limitation instead of allocating in the
  audio thread.
- Delay lines live next to `MixerChannel`/`MixerRoute` and are reset when the
  graph or sample rate changes.
- A latency change is a model change: update path latencies between mixer
  rounds, then rebuild delay lines. Avoid per-block allocations.
- Do not delay the dry path inside a single serial effect chain; use the chain
  latency only when computing the path latency.

## Staged implementation

1. Graph query
   - Add `MixerChannel::latency()` returning the FX-chain latency.
   - Add an instrument/sample-track latency query and expose the total path
     latency for debugging.
   - Acceptance: a unit test can query a chain with a known fake latency.
2. Track-to-channel compensation
   - Delay each track's contribution to its mixer channel so all instrument
     and sample tracks feeding the same channel align.
   - Acceptance: two parallel tracks, one with 256 samples of latency, render
     an impulse at the same sample position.
3. Send compensation
   - At each channel input, compare the latency of direct sends and track
     sources and delay shorter paths.
   - Handle fan-out and feedback loops; keep the existing infinite-loop
     protection.
   - Acceptance: a dry direct path and a high-latency send cancel when one is
     inverted.
4. Master and export
   - Include the master FX latency in the reported total.
   - Verify that real-time playback, offline render and `rendertracks` use the
     same compensation.
   - Acceptance: exported WAV has the same relative alignment as a real-time
     capture; export additionally removes the reported total latency from the
     start of the stream.
5. UI/status
   - Show per-track/channel latency and the total latency in the mixer or
     plugin dialog.
   - Warn when the requested latency exceeds the allocated delay line.

## Implementation status (2026-11-25)

Stages 1-4 are implemented and verified with temporary VST2 impulse plugins:

- `Mixer::updateLatencyCompensation()` rebuilds the graph before and after
  each mixer round and aligns track buses and send routes.
- `Mixer::totalLatency()` reports the path to the master output including the
  master FX chain.
- `ProjectRenderer` discards the engine warm-up period and trims that many
  frames from the beginning of the actual song stream. Export therefore starts
  at timeline frame 0 without an extra warm-up block.
- VST2 re-reports `initialDelay` when it changes while processing; VST2 and
  LV2 instruments publish their current latency through `play()`, and
  instrument tracks also poll it on the GUI thread for UI updates.
- A saturation warning is emitted once when a required delay exceeds the
  8192-sample delay line.

Stage 5 (UI display) and a committed impulse/phase test project are still
open.

## Tests

- Fake VST2/LV2 plugins with a configurable `latency` value.
- Impulse test: dry track + delayed track with one inverted copy.
- Parallel compressor with look-ahead.
- Export vs real-time comparison.
- Sample-rate change and latency change during playback.

## Non-goals for the first PDC step

- Compensating latency inside a plugin that already implements internal PDC.
- Sidechain, multi-bus and multi-output routing (P6.5).
- VST3 (separate branch).
- Latency compensation for MIDI input monitoring.

## Risks

- A wrong graph traversal can create feedback or permanently silent channels.
- Latency changes can click; use a short crossfade or reset between mixer
  rounds.
- Large look-ahead values increase memory use; cap and report them.
- Sends and solo/mute handling already have ordering rules; PDC must not
  change the dependency graph.
