# MIDI import stress

`midi_corpus.py` builds a valid SMF and mutates it. `import_runner.py` starts
`lmms --import <file> -e` in a separate process and, when running under Xvfb,
uses the shared `gui/dialog_watchdog.py` to dismiss the expected
"Couldn't import file" dialog.

```bash
# offscreen: malformed files are counted as timeouts
python3 tests/stress/imports/import_runner.py --build build --cases 50 --timeout 15

# with an X server and window manager, the watchdog closes the import dialog
tests/stress/gui/xvfb_session.sh -- \
    python3 tests/stress/imports/import_runner.py --build build \
        --cases 50 --timeout 15 --display :99
```

(`fuzz_midi.py` is the older offscreen-only runner and is kept unchanged.)

Classification:

- `pass`: process exited normally
- `dialog_timeout`: dialog appeared but could not be dismissed
- `timeout`: no dialog and process did not exit
- `crash`: fatal signal
