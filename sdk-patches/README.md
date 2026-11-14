# Local changes to the MPC Plugin Builder SDK

Vitaleon does not build against an unmodified SDK. `sdk-local-changes.patch` holds the two local changes.

- SDK repository: https://github.com/inmusic-software/mpc-plugin-builder-sdk
- Based on commit `febd2ca5bd9eb16c4a412837196e145566bc981b` (2026-11-07, "Add plugin format to plugin.json")
- SDK licence: MIT, Copyright (c) 2026 inMusic Brands, Inc. The patch contains SDK code; keep the MIT notice with it.

## Apply

From the SDK root, with this folder at `plugins/Vitaleon`:

```
git checkout febd2ca5bd9eb16c4a412837196e145566bc981b
git apply plugins/Vitaleon/sdk-patches/sdk-local-changes.patch
```

Checked: the patch applies cleanly to a fresh checkout of that commit and reverse-applies to the working tree it was taken from.

## What it changes

### `framework/mpc_plugin.h` (required, written before 0.3.2)

The stock framework restores state by calling `set()` per parameter, which runs `parameterChanged` each time, and its program list is fixed at construction. Vitaleon needs hooks around both:

- `publishNormalised(i, norm)`: writes a parameter value without calling `parameterChanged`. Used for every readout and proxy (graph columns, library rows, status texts).
- `numPrograms()`, `programName()`, `setProgram()` are virtual, so the PRESET list can follow the opened bank or folder.
- `restoreParameter()`, `stateRestored()`, `programSelected()`, `beginStateTransaction()` / `endStateTransaction()`: a chunk or program restore is one transaction; the plugin stages the expensive work (tables, samples, routes) once instead of per parameter.
- `internalLoadState` validates before it changes anything: chunk version 1 or 2 only, at most 100000 entries, finite values, extra-state length inside the buffer. A rejected chunk leaves the current sound untouched.
- `currentProgram_` is atomic (read by the audio thread, written by the service thread).

Vitaleon overrides all eight hooks (`src/FullSynth.h`); without this patch it does not compile.

### `tools/build.py` (optional)

Two lines: extra compiler flags from the environment variable `MPC_EXTRA_CXXFLAGS`, used once for a measured `-O3 -ffast-math` comparison on the device. Release builds do not set it.
