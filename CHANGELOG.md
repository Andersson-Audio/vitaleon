# Changelog

Device results refer to an MPC Live II with MPC OS 3.10.1.

## 0.4.0

- HOME: the bank (the two arrows and its name), BANK LIST, INIT SOUND and SAVE SOUND are one row at the top, beside the name of the sound. The comment of the sound stands at the bottom left, under the left edge of the name; the load message and the version beside it.
- A step to another bank with the arrows on HOME loads that bank's first sound; PRESET lists the bank alone. Before, the sound that played stayed and stood in the list as "[Current sound]". Changes to the sound that were not saved are gone with the step. A bank opened in LIBRARY leaves the sound as before.
- Runs on an MPC Live II; not every point above was checked there one by one.

## 0.3.59

- The pages of wavetables (a tap on a wave picture in OSC > OVERVIEW) have groups: FACTORY, then FIZMATIC 1 OF 2 and FIZMATIC 2 OF 2 with the 54 tables of the bank Fizmatic, then USER. The arrows beside the page name go round all of them; the arrows beside a table name on the overview step through every table in the same order.
- A folder of tables put into the plugin's Wavetables folder gets pages of its own under the folder's name.
- Not checked on an MPC yet.

## 0.3.58

- New bank in the package: Fizmatic, 54 sounds, each led by one of its 54 wavetables: 16 pads, 11 sequences, 8 keys, 6 basses, 6 leads, 4 plucks, 3 effects. Layers move through their tables at their own rates; every sound has the same first three macros (WAVE, FILTER, DETUNE) and a fourth of its own. On HOME the arrows beside the bank name step to it.
- Not checked on an MPC yet.

## 0.3.57

- INIT SOUND starts at -6 dB instead of -24 dB (the level Vital itself starts with). One note at full velocity peaks at -12 dBFS; eight notes at full velocity reach full scale, so turn MASTER down for big chords. Sounds that are loaded keep their own level.
- Not checked on an MPC yet.

## 0.3.56

- A page with a picture (oscillators, filters, envelopes, LFOs, wavetables) is drawn once after a page change instead of twice: the picture is ready before the MPC is asked to draw.
- Measured on an MPC Live II: the oscillator overview and the filter page are drawn once and are complete 0.9 to 1.1 seconds after the change (0.3.55: after 1.1 to 1.4 seconds). The LFO page is still drawn twice.
- The page of wavetables opens and turns in 0.3 seconds.

## 0.3.55

- Page changes are fast again. On an MPC Live II the pictures of the user wavetables of 0.3.54 made every page change take 3.6 seconds instead of 0.8. They need no parameters of their own any more and use a much smaller image strip (11 height steps instead of 27).
- Measured on an MPC Live II: a page change takes 0.9 seconds with this version (0.3.52: 0.8 seconds).
- The pages of user wavetables themselves are not checked on an MPC yet.

## 0.3.54

- The pictures the plugin draws for user wavetables are one smooth line now (24 segments, 27 height steps) instead of 24 bars.
- Not checked on an MPC yet.

## 0.3.53

- The page of wavetables has pages: the 32 factory tables, then every other installed table, 32 to a page. Two arrows turn the pages.
- User wavetables: a wavetable loaded in LIBRARY from a USB stick, an SD card or an SSD is copied to `Vitaleon/Wavetables` beside `VST2 Plugins`. The folder is kept when the plugin is updated, LIBRARY lists it as "User wavetables", and files copied there by hand are found as well.
- The plugin draws the picture of each of these tables itself, from the first cycle of the file (WAV and .vitaltable).
- The arrows of the OSC overview step through the user's tables too.
- Not checked on an MPC yet.

## 0.3.52

- OSC overview: two arrows under each wave picture step through the installed wavetables; the name of the table stands between them.
- A tap on a wave picture opens the 32 factory wavetables on one page, as a table with a small picture of each. A tap loads the table into the oscillator; the page stays open so tables can be tried one after another.
- Not checked on an MPC yet.

## 0.3.51

- Two more banks in the package: QUB1T - Volume 1 (95 sounds) and QUB1T - Volume 2 (100 sounds), sounds by qub1t. The plugin starts on the factory bank as before; the arrows beside the bank name on HOME step to the other banks.
- Not checked on an MPC yet.

## 0.3.50

- HOME laid out anew: author and style directly under the name of the sound, SAVE SOUND and the new INIT SOUND at the top right, the bank as one stepper at the bottom with the load message beside it.
- INIT SOUND resets the sound to Init. It is tapped twice (INIT SOUND, then CONFIRM INIT); left alone it goes back after 5 seconds and changes nothing.
- Not checked on an MPC yet.

## 0.3.49

- PAGE 1 / PAGE 2 are two small buttons at the top right of the page, under the tab row, in the same place on the oscillator, LFO and filter pages. On the oscillator page the dials start level with the wave picture.
- Not checked on an MPC yet.

## 0.3.47

- Fixed: a click when the sound is changed. The old sound fades out, and a leftover piece of it is no longer played after the change.
- The skin file is a third of its size (same content), which should shorten the time the screen takes to open.
- Not checked on an MPC yet.

## 0.3.46

- FILTERS shows its controls in two pages: PAGE 1 with cutoff, resonance, model, style, drive, mix, blend and key tracking of both filters, PAGE 2 with the comb and formant controls and the input switch. The pictures of both filters stay.
- Fixed: since 0.3.43 the dials of the effect pages were drawn in the filter's colour.
- Not checked on an MPC yet.

## 0.3.45

- LIBRARY: MY SOUNDS opens the folder of the sounds saved with SAVE SOUND. EXPORT copies them to `Vitaleon/Sounds` on the USB stick or SD card that is open in the library.
- Not checked on an MPC yet.

## 0.3.44

- SAVE SOUND on HOME: the sound that is playing is saved under a name of your choice as a `.vital` file in `<drive>/Vitaleon/Sounds`, beside `VST2 Plugins`. The folder is the bank "My sounds" in the bank selector, and it stays when the plugin is replaced by a newer version.
- Not checked on an MPC yet, and a saved file has not been opened in Vital itself yet.

## 0.3.43

- MOD: PICK on a route opens one page with every source and the groups of targets, so a source is chosen with a tap. The small source slider is gone.
- Every section has its colour: OSC violet, FILTER pink, ENV green, LFO amber, FX blue. The matrix writes each source and target in the colour of its section, so a row shows what is modulated by what.
- Not checked on an MPC yet.

## 0.3.42

- An LFO page shows its controls in two pages as well: PAGE 1 holds rate, shape, timing and smoothing, PAGE 2 the point editor, the shape file and key tracking. All controls are large and on the Q-Links.
- Not checked on an MPC yet.

## 0.3.41

- An oscillator page shows its controls in two pages, PAGE 1 (main controls) and PAGE 2 (fine controls), instead of two Q-Link layers over one crowded page. The wave picture stays; every dial is on a Q-Link and the dials are larger.
- Not checked on an MPC yet.

## 0.3.40

- HOME > OTHER holds the random sources and the expression controls as large dials, in one Q-Link layer.
- HOME > ADVANCED now also holds the curve of a modulation route with its point editor, beside the access to any parameter and Live Graphics.
- Not checked on an MPC yet.

## 0.3.39

- Bank selector: the PRESET list holds one bank. HOME shows the bank with `< BANK` and `BANK >` to step through the installed banks and BANK LIST to see them all; in LIBRARY, INSTALLED lists the installed banks and OPEN puts one into PRESET.
- A new instance starts on the factory bank if it is installed. A project opens the bank it was saved with.
- INSTALL checkbox in LIBRARY: when it is on, a bank opened from a USB stick or SD card is copied into the plugin's Banks folder and stays; when it is off, the bank is used from its drive.
- Not checked on an MPC yet.

## 0.3.38

- The last group is called MOD and holds the modulation matrix.
- New page HOME > OTHER: the random sources, the expression controls and the curve of a modulation route with its point editor, all on one page.
- Not checked on an MPC yet.

## 0.3.37

- The Q-Link knobs are named after the control they reach on the page on screen, no longer "UI Control 1" to "UI Control 16".
- Not checked on an MPC yet.

## 0.3.36

- FX overview: the nine effects are shown in the order of the chain, each with its switch. Tap a number to choose an effect, EARLIER and LATER move it. The separate ORDER page is gone.
- Every effect is on one page, in titled groups of controls that belong together, with large dials wherever the page has room. Each group is one Q-Link bank. Only the compressor has two Q-Link layers.
- The LFO shape dial keeps its place, and turning curve controls no longer interrupts the sound.
- Not checked on an MPC yet.

## 0.3.35

- Each LFO is on one page: a narrower, taller picture, Frequency and Shape as the large dials and every other control around them. The TIMING, PHASE, SHAPE, POINT and SMOOTH sub pages are gone; Q-LINKS MAIN and Q-LINKS MORE switch what the sixteen Q-Links reach.
- Not checked on an MPC yet.

## 0.3.34

- The envelope picture always fills its width, whatever the times, and shows the attack, decay and release curves as they sound. DRAG beside the dials shows the envelope with points that can be dragged; SHAPE returns to the picture.
- Not checked on an MPC yet.

## 0.3.33

- Each envelope is on one page: a larger picture, attack, decay, sustain and release as large dials, delay, hold and the three curve dials beside them. The TIME and CURVE sub pages are gone.
- Not checked on an MPC yet.

## 0.3.32

- Finer pictures: the wavetable on an oscillator page and the sample on its page are drawn in 256 columns instead of 128, the LFO and modulation curve in 384 instead of 192 and with finer levels.
- Not checked on an MPC yet.

## 0.3.31

- The sample is on one page: a picture of the sample, SAMPLE LIST below it, the name of the loaded sample and every control. Level, pan, pitch and tune are the large dials.
- Not checked on an MPC yet.

## 0.3.30

- Both filters are on one page, FILTERS. Each has a picture of its response from 20 Hz to 20 kHz, measured from the filter itself, so every model and style shows what it does. The picture is drawn when you stop turning a control.
- Cutoff and resonance are the large dials; drive, mix, blend, key tracking, model and style are medium; the comb offset, the formant controls and the input switch are small.
- Q-LINKS MAIN and Q-LINKS MORE switch what the sixteen Q-Links reach. Knob groups 1 and 2 belong to filter 1, groups 3 and 4 to filter 2.
- No picture is redrawn while a control is turned, so the controls stay smooth with Live Graphics on. The picture follows a quarter of a second after you stop.
- FILTERS page checked on an MPC Live II; the redraw change on the oscillator pages not yet.

## 0.3.29

- Each oscillator is on one page, without sub pages, with a larger wave picture. Sixteen controls are on the Q-Links in four groups; the first group holds the four large dials Level, Frame, Spectral and Warp. The fine-tuning controls are small dials for the finger.
- Q-LINKS MAIN and Q-LINKS MORE switch what the sixteen Q-Links reach on an oscillator page; every control stays on screen.
- The wavetable is chosen with TABLE LIST; the stepping selector is gone from the oscillator page.

## 0.3.28

- In HOME > LIBRARY a tap anywhere on a row opens or loads it; the buttons at the right still work.
- TABLE LIST on each oscillator page and SAMPLE LIST on the sample page open the list of the plugin's wavetables or samples; a tap loads one into that oscillator or the sampler.
- Vitaleon has its own tile in the MPC's Sounds browser.
- Controls sit on panels, and the names of controls are quieter than their values.

## 0.3.27

- New HOME page. It shows the name of the sound, its author, style and comment, and holds what you play: the sound's four macros under their own names, cutoff, resonance, the filter switch, the volume and the volume envelope with attack, decay, sustain and release. Voices, unison and detune are no longer on HOME; they are on VOICES and on each oscillator's UNISON section.

## 0.3.26

- New page OSC > OVERVIEW, the first page of OSC: all three oscillators and the sample side by side, each with on/off, a picture of its wave (for the sample: of its level over time) and dials for level, wave frame (sample: pan) and pitch. The Q-Links reach them in three groups: levels, frames and pan, pitches.
- A red line marks the group, page and section you are in.

## 0.3.25

- Wavetables, samples and LFO shapes load straight from a drive. HOME > LIBRARY now lists `.vitaltable`, WAV and `.vitallfo` files with LOAD. **WAV AS** chooses where a WAV goes: oscillator 1, 2 or 3 as a wavetable, or the sampler. Before, these files had to be copied into the plugin's `Content` folder.
- The oscillator or sampler that receives a file is switched on if it was off, and the status line says what was loaded and where.
- A wavetable WAV that states its cycle length (Serum, Vital) is read with that length, and a short single-cycle file is read as one cycle instead of being cut at 2048 samples.
- A file that cannot be read is refused with its reason and leaves the sound as it was. A successful load replaces an older failure message.
- Audio stored with a sound or project is rounded to 16 bit instead of cut off, so it no longer changes slightly each time a project is loaded and saved.
- The lightest sounds need about 5 % less processor time on the device.
- Controller, pitch-bend and aftertouch messages no longer split an audio block, which saves processor time while a controller moves. Notes stay sample-accurate.

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
