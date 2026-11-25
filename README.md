# Vitaleon

A wavetable synthesizer plugin for Akai MPC standalone, by Andersson Audio.

Vitaleon runs the sound engine of Matt Tytel's Vital 1.0.6 (GPLv3) inside the MPC's plugin format, with a touch interface built for the MPC screen. It is an independent project and is **not affiliated with or endorsed by Matt Tytel or Vital Audio**.

**Download:** the ready-to-install package, with the factory wavetables and sounds, is on [dreyandersson.com/vitaleon](https://dreyandersson.com/vitaleon/). This repository holds the source code.

- Three wavetable oscillators, a sampler, two voice filters, six envelopes, eight LFOs, four random sources, four macros
- A 64-route modulation matrix shown as a table: source, amount, target
- Nine effects: distortion, chorus, flanger, phaser, delay, reverb, compressor, EQ, filter
- Imports Vital presets (`.vital`), banks (`.vitalbank`, ZIP), wavetables (`.vitaltable`, WAV), LFO shapes (`.vitallfo`) and WAV samples
- Saves your own sounds as `.vital` files on the MPC
- The release package adds 32 factory wavetables, a bank of 32 factory sounds, the bank Fizmatic (54 sounds) with its 54 wavetables and two banks of sounds by qub1t (95 and 100 sounds)
- Live oscillator and LFO graphs, Q-Link mapping per page, host tempo sync

## Status

Version 0.4.0. This is a young project.

| | |
|---|---|
| Tested on | MPC Live II, MPC OS 3.10.1, the 32-bit build |
| Not tested | Any other MPC model. The package also contains a 64-bit build that has not run on a device yet. |
| Requires | MPC OS 3.10.1 or later with **Enhancements (Beta)** and **External Plugin Support** switched on |

If you try it on another model, a report of what works is welcome.

## Install

Download the release ZIP from [dreyandersson.com/vitaleon](https://dreyandersson.com/vitaleon/), unzip it and copy the folder `Andersson Audio - VST - Vitaleon` into a folder named `VST2 Plugins` at the top level of a drive the MPC can read. The MPC's internal storage is the recommended place. Full steps in English and German: [INSTALL.md](INSTALL.md).

## First steps

1. Create a plugin track and choose **Vitaleon** under External Plugins.
2. HOME shows the loaded sound with its author, its four macros under their own names, cutoff, resonance, volume and the volume envelope. The plugin starts with the first factory sound loaded. Pick another in the MPC's **PRESET** list: it holds the 32 factory sounds (BA bass, LD lead, PD pad, KY keys, PL pluck, SQ sequence, FX). The row at the top of HOME holds the bank: the two arrows beside its name step to the other banks of the package (Fizmatic, QUB1T - Volume 1 and QUB1T - Volume 2) and load the first sound of the bank; PRESET then lists the sounds of that bank. In the same row, **INIT SOUND** (tapped twice) resets the sound and **SAVE SOUND** stores it under a name of your own.
3. The footer switches between HOME, OSC, FILTER, ENV, LFO, FX and MOD. Each group has its pages across the top; a red line marks the group, page and section you are in. OSC opens with an overview of all three oscillators and the sample. There the two arrows beside the name of a wavetable step through the installed tables, and a tap on the wave picture opens the pages of wavetables: the 32 factory tables first, then the 54 Fizmatic tables on two pages, then your own, 32 to a page, each with a small picture; tap one to load it. A wavetable you load in LIBRARY from a USB stick, an SD card or an SSD is copied to the folder `Vitaleon/Wavetables` beside `VST2 Plugins` and is listed there from then on.
4. **HOME > LIBRARY** browses drives. Tap a row, or its button at the right: OPEN enters a drive, folder or bank; LOAD loads a sound, a wavetable, a WAV file or an LFO shape. TABLE LIST on an oscillator page and SAMPLE LIST on the sample page open this list on the plugin's own wavetables and samples. **WAV AS** on that page says where a WAV goes: into oscillator 1, 2 or 3 as a wavetable, or into the sampler.
5. **MOD > MATRIX** holds the modulation routes. PICK on a row opens the chooser for its source and its target.
6. Macros 1 to 4 are assigned in every factory sound. [FACTORY.md](FACTORY.md) lists all tables, sounds and macro names.

Save your edits with the MPC project. The plugin state, including imported tables and samples, is stored in the project. To keep a sound for other projects, use **SAVE SOUND** on HOME: it writes a `.vital` file to the folder `Vitaleon/Sounds` beside `VST2 Plugins`, and the bank "My sounds" lists it from then on. **LIBRARY > MY SOUNDS** opens that folder; **EXPORT** copies the saved sounds to another drive.

## Performance

The MPC has a small processor for a synth of this size. Vitaleon is set up around that:

- A sound has at most eight voices. Imported sounds start at 1x oversampling, which can be raised under **HOME > VOICES**.
- The effect chain runs on a second processor core. This adds 128 samples of latency, which is reported to the MPC.
- Effects whose mix sits at zero behind a macro are skipped until the macro opens them.
- If the audio load stays too high, the plugin lowers the voice limit in pairs and raises it again when the load falls.

On the test device, at buffer size 128, one held note of a heavy third-party preset used about 30 % of the audio block time. Dense sounds with many unison voices and long chords can still overload the device.

## What it does not do

- Features added to Vital after 1.0.6. Newer spectral modes are switched off on import, and the import tells you so. Details in [COMPATIBILITY.md](COMPATIBILITY.md).
- The original desktop interface, the full wavetable editor, text-to-wavetable, custom tunings.
- FLAC or MP3 files. Samples and wavetables must be WAV.

## Factory content

The release package contains 32 factory wavetables and a bank of 32 factory sounds, created by Drey Andersson. The wavetables are synthesised; no samples or ROM data are used. Each sound is built on one of the tables. [FACTORY.md](FACTORY.md) lists them.

The factory content is **not part of this repository and not under the GPL**. You may use it with Vitaleon and in your own music; you may not redistribute it or ship it with another product. If you make your own port or build, please create your own wavetables and sounds. Terms: [FACTORY-CONTENT-LICENSE.txt](FACTORY-CONTENT-LICENSE.txt).

The release package also holds the bank Fizmatic: 54 sounds by Drey Andersson, each led by one of its 54 wavetables, which are in the package as files too. Bank and tables fall under the factory content licence and are not part of this repository either.

The release package also holds two banks of sounds by qub1t, QUB1T - Volume 1 (95 sounds) and QUB1T - Volume 2 (100 sounds). They are qub1t's work; they are not part of this repository and not under the GPL either.

## Build from source

Vitaleon builds with Akai's MPC Plugin Builder SDK plus a small patch. See [BUILDING.md](BUILDING.md).

## Licence

The Vitaleon program, meaning everything in this repository, is free software under the **GNU General Public License, version 3 or later**. See [LICENSE](LICENSE) and [THIRD-PARTY.txt](THIRD-PARTY.txt).

The factory wavetables and factory sounds in the release package are a separate work with their own licence, [FACTORY-CONTENT-LICENSE.txt](FACTORY-CONTENT-LICENSE.txt). All rights in them are reserved.

The source of the sound engine is Vital by Matt Tytel, https://github.com/mtytel/vital. "Vital" is a trademark of its owner. This project uses the name only to say where the engine comes from.

History of changes: [CHANGELOG.md](CHANGELOG.md).
