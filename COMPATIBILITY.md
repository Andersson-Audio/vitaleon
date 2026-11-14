# Vital preset compatibility

Vitaleon uses the publicly available Vital **1.0.6** synthesis source. It is not a reimplementation of later Vital versions. A sound that imports can be represented by this engine; that is not a promise that it sounds identical to the same preset in a current Vital release.

## What can be loaded

| File | Use |
|---|---|
| `.vital` | Single sounds |
| `.vitalbank`, `.zip` | Banks. Only the index is read when a bank is opened; a sound is unpacked when it is loaded. |
| `.vitaltable`, `.wav` | Wavetables. WAV files are read as frames of 2048 samples, up to 256 frames. |
| `.vitallfo` | LFO shapes |
| `.wav` | Samples, PCM or float, 8 kHz to 384 kHz, up to 4,194,304 frames per channel |

Embedded wavetables, samples, the eight LFO curves and all 64 modulation mappings of a sound are stored with the MPC project. A saved project does not need the original files any more.

## How newer presets are handled

The importer loads everything this engine can represent and tells you what it left out. A sound with omissions shows **Loaded with changes** and the first reason in the status line.

- **Newer spectral morph modes** (numbered 12 and above) do not exist in this engine. That oscillator loads without spectral morphing; it is never mapped to a different mode.
- **Spectral phase** settings other than the default are ignored.
- **Modulation ramps** from Vital 1.6 are ignored. Sounds that leave them at their minimum are unaffected.
- **Unison stack types** from Vital 1.5 are translated to this engine's numbering, including Three Octaves and Subharmonics.
- **LFO Point Cycle** is supported.
- **Unknown settings**, modulation sources or targets are skipped.

## What is rejected

A file is refused, and the current sound stays untouched, when:

- it was saved by a Vital version before 1.0,
- a value lies outside the range of a control this engine has,
- the file or bank is damaged, or
- a sample is longer than the limit above.

## Settings applied on import

Every imported sound starts at **1x oversampling** and with at most **eight voices**, because most presets are saved with 2x oversampling and the MPC's processor is small. Both can be raised under HOME > VOICES and are saved with the project. The preset file itself is never changed.

## Not available

- Text-to-wavetable, online services and account downloads
- Custom tuning files
- FLAC and MP3
- Saving to `.vital` or `.vitalbank`
- The desktop interface, the 3D wavetable view and the wavetable component editor
