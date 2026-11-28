# Single-process soak

`SingleProcessSoak` keeps one LMMS process alive and repeatedly
loads/renders/saves the same project. It samples `VmRSS`, file descriptors and
thread counts, so leaks and drift are visible without restarting LMMS between
cases.

```bash
cmake --build build --target SingleProcessSoak
python3 tests/stress/soak/soak.py --build build \
    --project 'data/projects/shorties/Crunk(Demo).mmp' \
    --hours 8 --chunk-minutes 60 --rss-limit-mb 2048
```

`soak.py` runs one child per chunk (default 30 minutes) so a crash still
leaves partial metrics. Use `--chunk-minutes 480` for one 8-hour process.
Metrics are appended to `/tmp/opencode/lmms-stress/soak/<run-id>/chunks.jsonl`;
the last line of each chunk contains `iterations`, `maxrss`, `maxfds`,
`maxthreads` and `failures`.

The C++ helper accepts `--mix load-save|render|load-render-save` and
`--buffers N` (audio buffers per iteration). It stops the audio device thread
and drives `Song::processNextBuffer()` manually, making renders deterministic
and keeping the process in control of teardown.
