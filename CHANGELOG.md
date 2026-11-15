# Changelog

Device results refer to an MPC Live II with MPC OS 3.10.1.

## 0.3.24

- The voice control ends at eight voices (was 32).

## 0.3.23

- Controls respond directly again. While a control moves, the plugin no longer calls into the MPC; 0.3.22 did so on every movement and the controls felt sluggish on the device.
- Small Q-Link turns on a control with fixed steps (voices, lists) add up instead of being lost.

## 0.3.22

- Knobs and faders follow the Q-Links smoothly. They are now bound to the Q-Link slots themselves, so the MPC moves the control that its knob writes.
- A frame marks the four controls that the Q-Links currently reach. It follows the knob group turned last.
- Removed the display refresh of 0.3.21: on the device it made every control stutter.

## 0.3.21

- Not released. It refreshed the whole display after every Q-Link move, which made the controls stutter on the device.

## 0.3.20

- The PRESET list holds the installed sounds and nothing else. The eight basic built-in sounds are gone.
- A new instance starts with the first sound of the list loaded.
- A plugin without any installed sound (a build from source) has one sound named Init.
- A sound with more voices than the one before gets all its voices at once. Before, the voice limit rose over several seconds.

## 0.3.19

- Library page reworked. Removable drives are listed right after Plugin Content. Rows show kind, name and one button: OPEN for a drive, folder or bank, LOAD for a sound.
- A bank opens like a folder and lists its sounds with LOAD. UP returns to the bank's folder.
- Library buttons no longer stay lit after a tap.

## 0.3.18

- Fixed audio dropouts while a live graph was moving. Measured on the device: announcing each graph value to the host delayed the MPC's audio callback (240 late callbacks in 10 s). The plugin now asks for one display update per picture; with that, no late callback was measured at up to 30 pictures a second.
- Finer graphs: oscillator wave 128 columns by 108 levels, LFO and modulation curve 192 by 96.

## 0.3.17

- Modulation matrix as a table: source, amount, target, with BIPOLAR, STEREO and MUTE per row. PICK opens a target chooser, first the group, then the entry.
- 32 factory wavetables, 256 frames of 2048 samples each.
- Factory bank with 32 sounds, each built on one of the factory wavetables.
- Table, sample and LFO-shape selectors can be set by touch.

## 0.3.16

- The effect chain runs on a second processor core. Adds 128 samples of latency, reported to the host.

## 0.3.15

- Effects whose mix is zero behind a macro are not processed.
- CPU guard: under sustained overload the voice limit steps down in pairs and recovers afterwards.

## 0.3.10 to 0.3.13

- Choice fields are faders with a value overlay. The SDK's menu overlay shows an empty list on MPC OS 3.10.1 and is not used.
- Live graphs follow the playing sound. The LFO playhead rests when the LFO is not routed.

## 0.3.4 to 0.3.7

- Tolerant import: settings this engine does not know are skipped and reported instead of rejecting the sound.
- Single `.vital` files can be loaded from the library.
- Imported sounds start at 1x oversampling and at most eight voices.
- The engine sleeps when silent.
- Duplicate controls removed; oscillator, envelope and LFO pages with larger graphs.

## 0.3.2 and 0.3.3

- Section buttons inside a page switch their content.

## 0.3.1 and earlier

- First port of the engine, the importer and the touch surface.
