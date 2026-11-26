# Stability campaign report

Date: 2026-11-27 (local Fedora 45 machine)
Branch: `stability/autotest-20261127`

This report records the bugs found and fixed during the local stress/fuzz
campaign, plus the test evidence. No functional features were added.

## Bugs found and fixed

### 1. GUI deadlock while loading a VST2 instrument

**Symptom:** rendering/GUI loading a project with ZynAddSubFX intermittently
hung forever (observed 4/10 runs; later 6/6 under load).

**Root cause:** `VestigeInstrument::loadFile()` held `m_pluginMutex` while
constructing `VstInstrumentPlugin`. VST2 startup processes the Qt event loop.
The periodic `InstrumentTrack` latency timer then called
`VestigeInstrument::latency()`, which blocked on the same non-recursive mutex.

**Fix:**
- construct the plugin before publishing it under `m_pluginMutex`;
- make `latency()` non-blocking (`tryLock`, return 0 while busy);
- apply the same non-blocking latency + construct-outside-lock pattern to
  `VstEffect`.

**Evidence:** the saved reproducer (`corpus/regressions/vestige-load-deadlock.mmp`)
now completes 10/10 times in ~1 s instead of hanging.

### 2. VST2 plugin host handshake could block forever

**Symptom:** a plugin host that connects but never sends `IdHostInfoGotten` or
`IdInitDone` made LMMS wait indefinitely.

**Fix:** `RemotePluginBase::waitForMessage()` now has a deadline (15 s for
host info/init, 30 s default, 5 s for audio processing). On timeout it
invalidates the transport, so the plugin is treated as failed. Also hardened
the protocol against oversized string lengths/string counts and removed an
out-of-bounds `std::string` write.

**Evidence:** a fake `NativeLinuxRemoteVstPlugin64` that connects and sleeps
no longer hangs; LMMS reports the plugin as failed and exits in ~16 s.

### 3. Configuration upgrades silently destroyed audio/MIDI settings

**Symptom:** loading any config with `configversion < 3` cleared
`audioengine/audiodev`, `mididev`, `samplerate`, `framesperaudiobuffer` and
`hqaudio`, and dropped old `MidiAlsaRaw/Device` / `app/displaydbv` values.

**Root cause:** `ConfigManager::value()` defaults to `""` (an empty but
**non-null** `QString`), while the upgrade routines used `isNull()` to detect
missing legacy keys. Every missing key was treated as present and copied as
empty over valid settings.

**Fix:** use explicit `QString()` defaults in the upgrade routines.

**Evidence:** `ConfigUpgradeTest` covers audio-engine preservation, legacy
mixer migration, `displaydbv` rename and `MidiAlsaRaw` rename.

### 4. NotePlayHandle cache leaked its backing blocks

**Symptom:** ASan/LSan reported 79,872 bytes leaked at exit; every cache
extension leaked the previous block as well.

**Fix:** track all `malloc`ed note blocks and free them in
`NotePlayHandleManager::free()`.

### 5. Base64 sample/state helpers

- `base64::decode<T>()` allocated `size / sizeof(T)` elements but copied
  `size` bytes. A non-multiple length caused a heap buffer overflow.
  Fixed to copy only whole elements and return the truncated byte count.
- `FreeBoy`, `Watsyn`, `MultitapEcho` leaked decoded buffers (Watsyn and
  MultitapEcho leaked 3 and 1 blocks per load).
- Those plugins (and DynamicsProcessor, WaveShaper, Xpressive, Vibed) copied
  `graphModel::length()` floats without checking the decoded payload size.
  Added size guards; short/corrupt state is ignored instead of reading past
  the allocation.

**Evidence:** `Base64Test` plus ASan runs of `FreeBoy`-using demo projects.

### 6. CLI render leaked the renderer and crashed on exit

**Symptom:** ASan reported the dummy audio device and related objects leaked;
a first cleanup attempt crashed because `RenderManager` was destroyed after
`QApplication`, while `restoreAudioDevice()` emitted `sampleRateChanged` to
VST plugin slots.

**Fix:** keep a `RenderManager*` in CLI mode and delete it after `app->exec()`
but **before** deleting `QApplication` and destroying the engine. This
restores the original audio device and releases the file encoder.

**Evidence:** ASan renders of built-in and real-plugin projects are leak-free.

### 7. Non-finite / overflowing numeric attributes caused SIGFPE and hangs

**Symptom:** a project with
`timesig_numerator="nan"`, `timesig_denominator="inf"` crashed with SIGFPE
inside `MidiClip::beatClipLength()`; a project with `bpm="nan"` did not finish
rendering within 60 s.

**Root cause:** `std::clamp(NaN, min, max)` keeps NaN. `IntModel`/`FloatModel`
therefore accepted non-finite values, which later reached
`TimePos::ticksPerBar()` / `Song::framesPerTick()` and produced zero or
non-finite divisors.

**Fix:**
- `AutomatableModel::fittedValue()` replaces NaN/Inf with the model's initial
  value before clamping.
- `TimePos::ticksPerBar(TimeSig)` clamps numerator/denominator to 1..32 and
  `setTicksPerBar()` never stores zero.
- `ticksPerBeat()` uses `max(1, numerator)`.

**Evidence:** both reproducers are in `corpus/regressions/`
(`nonfinite-meter-sigfpe.mmp`, `nonfinite-tempo-hang.mmp`) and now render
normally. `ValidationTest` covers non-finite model values and zero meters;
`fuzz_numbers.py` ran 400 release + 30 ASan cases with no crashes/hangs.

### 8. A FIFO/device node as sample source hung the loader

**Symptom:** a project whose `audiofileprocessor src` pointed at a named pipe
(`mkfifo`) made LMMS block forever while opening the sample; `/dev/zero` was
accepted as a sample path.

**Fix:** `SampleDecoder::decode()` now requires a regular file
(`QFileInfo::isFile()`) before opening anything, so FIFOs, character devices,
directories and sockets are rejected immediately. The same guard is applied
to the project file itself, the configuration file on load/save, and the
audio export path on write.

**Evidence:** the FIFO reproducer now completes in ~2 s with a warning; the
`nonregular-sample-devzero.mmp` regression is replayed by
`run.py --replay`.

### 9. Zero-length pattern hung the GUI while creating track views

**Symptom:** one of 50 ASan GUI runs (a project with a 0-length pattern and
negative meter values) never exited. The main thread was inside
`TrackContentWidget::updateBackground()`, drawing grid lines forever.

**Root cause:** the PatternEditor sets `pixelsPerBar` to 0 when the maximum
clip length is 0. `TrackContentWidget::updateBackground()` then uses
`x += ppb * resolution` with `ppb == 0`, an infinite loop.

**Fix:** `TrackContainerView::setPixelsPerBar()` clamps to at least 1, and
`updateBackground()` returns early for a non-positive `ppb` and sanitizes a
non-positive/NaN snap size.

**Evidence:** `corpus/regressions/gui-zero-length-pattern.mmp` now starts and
quits in ~4 s; the ASan GUI matrix is clean afterwards.

### 10. Huge pattern step counts / clip lengths froze the GUI

**Symptom:** two numeric edge-case projects did not finish starting the GUI
within 8 s. GDB showed the main thread in `MidiClipView::paintEvent()`:
one project had 2,147,483,647 pattern steps (billions of step buttons), the
other had an enormous clip length (billions of bar lines).

**Fix:**
- `MidiClip` clamps the loaded step count to 1..1024 (and keeps the
  note-driven length sync clamped).
- `MidiClipView::paintEvent()` uses a safe non-zero clip length and skips the
  bar-line loop when `pixelsPerBar < 1` (lines would be denser than a pixel).

**Evidence:** `gui-huge-clip-length.mmp` and
`gui-unbounded-pattern-steps.mmp` now start and quit in ~3 s.

### 11. Intermittent SIGSEGV in AudioFileProcessor playback

**Symptom:** one numeric-fuzz case (non-finite automation values) produced a
SIGSEGV in `AudioFileProcessor::playNote()` while passing a pointer
`_working_buffer + offset` to `Sample::play()`.

**Root cause:** a malformed/non-finite time or automation value can produce a
negative note offset or a negative per-period frame count. The playback code
only handled offsets that were too large, not negative ones, and
`framesLeftForCurrentPeriod()` could return a negative count.

**Fix:** clamp negative offsets to zero in `NotePlayHandle::play()`, and clamp
the per-period frame count to at least zero. This protects all instruments,
not only AudioFileProcessor.

**Evidence:** the reproducer is in
`corpus/regressions/afp-note-offset-crash.mmp`; it ran 20 release and 10 ASan
times cleanly afterwards. The original crash was intermittent, so it is kept
as a fuzz regression case.

### 12. Deeply nested XML crashed the project loader

**Symptom:**
- 100,000 nested elements → SIGSEGV inside
  `QXmlSimpleReaderPrivate::parseElement()` (stack overflow);
- 10,000 nested elements → no completion within 20 s.

**Fix:** `DataFile::loadData()` now scans the raw/uncompressed XML for element
depth before handing it to Qt's recursive reader and rejects depth > 1000.
It also rejects qCompress headers claiming an uncompressed size > 512 MB
(decompression-bomb guard).

**Evidence:** 1k/10k/100k-deep files and a compressed 100k-deep file are now
rejected gracefully (“project is empty”) in well under a second.
`ValidationTest::deeplyNestedProjectIsRejected` covers the guard.

## Test evidence

### Unit/integration tests

- Release ctest: **29/29 passed** (including new `Base64Test`,
  `ConfigUpgradeTest` and the deep-XML test).
- ASan ctest with `QT_QPA_PLATFORM=offscreen`: **29/29 passed**
  (external Qt/Wayland/fontconfig leaks suppressed).

### Random render campaigns (release)

| Run | Cases | Result |
|---|---|---|
| tiny, plugin ratio 0.3 | 150 | 148 pass, 3 timeouts (fixed deadlock), 1 silence (master volume mutation) |
| medium, plugin ratio 0.4 | 100 | 98 pass, 2 silence (master channel volume -1) |
| tiny + effects, plugin ratio 0.6 | 100 | 100 pass |
| tiny, plugin ratio 0.5 | 300 | 300 pass |
| medium + effects, plugin ratio 0.5 | 100 | 100 pass |
| medium + effects, plugin ratio 0.5 | 200 | 200 pass |
| medium + effects, plugin ratio 0.5 | 100 | 100 pass |

### ASan campaigns

- 30 tiny cases with real instruments/effects: 30 pass (one silence caused by
  a mutated missing clip length; harness now treats silence as a warning for
  mutated projects).
- 320 malformed XML cases under ASan, 500 under release: 0 crashes,
  0 sanitizer reports.
- 400 numeric edge-case projects (NaN/Inf/overflow attributes): 0 crashes,
  0 hangs (after the meter/tempo fixes).
- 300 corrupted sample files (and 100 under ASan): 0 crashes.
- Real-plugin probe (Dexed, Zyn, Surge LV2/VST3, OB-Xf, sfizz, Pianoteq):
  all load and render, ASan-clean.
- 50 load/save cycles with Dexed state: ASan-clean.

### GUI

- 50 offscreen GUI start/load/quit cycles: all pass (exit code 3 on SIGINT).
- 20 GUI cycles with real plugins/effects: all pass.
- 50 ASan GUI cycles with plugins: 49 pass, 1 timeout caused by the
  zero-length-pattern grid loop (fixed); the reproducer now passes.
- 200 release GUI start/load/quit cycles: 200 pass.
- 30-minute idle ASan GUI soak with a mixed real-plugin project: RSS stable
  at 993–996 MB, clean SIGINT exit (3), no LMMS leaks.
- GUI start/load/quit is now part of `gui_smoke.py`.

### Soak

- 2-hour tiny soak on a copied build: 1,781 cases, RSS stable at
  ~221–224 MB (average 222.9 MB), no crashes/hangs. Five failures were
  harness false positives (missing root `type`, mutated clip length/master
  volume), all addressed in the generator; the final reruns are clean.
- 45-minute final soak on the latest build: 826 cases, 0 failures,
  average RSS 223.6 MB (max 399 MB).

### Plugin isolation

Killing plugin host processes during render was tested:

- VST3 remote DSP (`lmms-vst3-host`): LMMS exits 0, partial WAV.
- VST2 (`NativeLinuxRemoteVstPlugin64`): LMMS exits 0.
- LV2 remote (`LMMS_LV2_REMOTE=1`, `lmms-lv2-host`): LMMS exits 0.

No LMMS crash or hang was observed.

## Capacity observations

- 100 instrument tracks × 500 notes: ~65 s, ~390 MB RSS.
- 10,000 simultaneous notes in one bar: ~9 s.
- 50,000+ simultaneous notes in one bar: CPU-bound, >5 min; not a crash.
- 100 load/save cycles: ~3.4 s, max RSS 47 MB (no growth over 1000 cycles).
- 64 Dexed VST3 instances: ~16 s, 2.1 GB RSS, 419 FDs.
- 128 Dexed VST3 instances: ~37 s, 4.2 GB RSS, 803 FDs; no crash.
- Deep XML: rejected before parser recursion.

## Remaining / future work

- TSAN was not available on this machine (`libtsan` runtime is not installed,
  no root); concurrency races could not be checked here.
- Full GUI interaction needs an X server plus `xdotool`/`wmctrl`; only
  offscreen start/load/quit is automated.
- MIDI import of invalid files opens a modal dialog by design; automated
  fuzzing needs a dialog auto-dismisser.
- LADSPA/Carla third-party plugins were not part of the default matrix.

## Reproduce

```bash
# deadlock reproducer
QT_QPA_PLATFORM=offscreen ./build/lmms render \
  tests/stress/corpus/regressions/vestige-load-deadlock.mmp -o /tmp/out.wav

# deep XML regression
./build/tests/ValidationTest

# config upgrade regression
./build/tests/ConfigUpgradeTest

# base64 regression
./build/tests/Base64Test

# random + real plugins
python3 tests/stress/run.py --build build --profile tiny --cases 50 \
  --seed-start 1 --plugin-ratio 0.5

# plugin probe
python3 tests/stress/plugins/probe_plugins.py --build build --include-optional
```
