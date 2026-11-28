# Real-plugin probes

These helpers only run locally. They never copy plugin binaries or plugin
state into the repository.

## Inventory

`inventory.py` knows the VST3/VST2/LV2 instruments and effects installed on
the development machine and discovers LADSPA libraries from:

- `~/.ladspa`
- `/usr/lib/ladspa`, `/usr/lib64/ladspa`
- `/usr/local/lib/ladspa`, `/usr/local/lib64/ladspa`

LADSPA descriptor labels are read with `ctypes` (no audio is processed). Carla
racks cannot be synthesized, so save one project per rack as
`fixtures/effects/carla/<name>.mmp`; it is picked up automatically.

Generate a machine-specific inventory:

```bash
python3 tests/stress/plugins/probe_plugins.py --build build --write-inventory
# writes plugins/inventory.generated.json (untracked)
```

`inventory.local.json` is the old render-result file and is still readable for
backwards compatibility.

## Probes

```bash
# every available instrument, one unmutated fixture each
python3 tests/stress/plugins/probe_plugins.py --build build --include-optional

# one instrument, including content-dependent plugins (sfizz, Pianoteq)
python3 tests/stress/plugins/probe_plugins.py --build build --plugin sfizz

# every effect inserted into a demo fxchain
python3 tests/stress/plugins/probe_effects.py --build build

# 1/4/8/16 instances of one plugin in one project
python3 tests/stress/plugins/multi_instance.py --build build --plugin dexed \
    --counts 1,4,8,16

# kill the plugin host while rendering
python3 tests/stress/plugins/host_kill.py --build build --plugin dexed --mode vst3
python3 tests/stress/plugins/host_kill.py --build build --plugin zyn-add-sub-fx --mode vst2
python3 tests/stress/plugins/host_kill.py --build build --plugin surge-xt --mode lv2

# mutate VST2 chunks / VST3 component+controller state
python3 tests/stress/plugins/state_fuzz.py --build build --plugin dexed --cases 50
```

Results are written next to the scripts (`inventory.results.json`,
`effect-results.json`, `multi-<id>.json`) and under
`/tmp/opencode/lmms-stress/runs/`.

## Fixtures

`fixtures.py` starts from `data/projects/shorties/Crunk(Demo).mmp`, replaces
the first instrument track that contains notes, and injects one effect into
the first `fxchain`. Generated fixtures are deterministic and are cached only
in memory; delete `fixtures/` to force regeneration. The `carla` subtree is
never deleted by `clear_local_fixtures()`.
