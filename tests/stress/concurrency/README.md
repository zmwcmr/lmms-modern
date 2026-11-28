# Concurrency / TSAN stress

`concurrency_stress.cpp` runs one thread in `Song::processNextBuffer()` while
another thread mutates the same song:

- `audio-model`: tempo, track volume and mute
- `audio-editor`: add/remove notes in a MIDI clip
- `automation`: create/remove automation tracks and nodes
- `mixer`: add/remove an effect while audio is processed

Build a TSAN tree and target:

```bash
cmake -S . -B build-tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DWANT_TSAN=ON
cmake --build build-tsan --target StressConcurrency
python3 tests/stress/concurrency/run_concurrency.py --build build-tsan --mode audio-editor --seconds 60
```

The runner sets `TSAN_OPTIONS=halt_on_error=0:second_deadlock_stack=1` and
stores stdout/stderr under `/tmp/opencode/lmms-stress/runs/`.
