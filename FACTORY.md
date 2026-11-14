# Vitaleon factory content

The 32 wavetables and 32 sounds of the Vitaleon release package were created by Drey Andersson. The tables are synthesised (additive, FM, phase distortion, sync, folding, formant filters); no samples or ROM data are used. They are licensed separately from the program and may not be redistributed: see `FACTORY-CONTENT-LICENSE.txt`.

## Wavetables (`Content/Wavetables/Factory`, 256 frames x 2048 samples)

| No | Table | Family | What moves across the table | Sound built on it |
|---|---|---|---|---|
| 01 | Classic Shapes | VA | Sine, triangle, saw, square, narrowing pulse | LD Classic Mono |
| 02 | Saw Filter | VA | Sawtooth through a 24 dB low-pass, closed to open | BA Beef Bass |
| 03 | Pulse Width | VA | Pulse from 50 % down to 4 % | PD PWM Strings |
| 04 | Sync Saw | VA | Hard-synced sawtooth, slave ratio 1 to 5 | LD Sync Screamer |
| 05 | Sync Formant | VA | Windowed sine sync around the cycle centre, ratio 1 to 10 | PL Laser Stab |
| 06 | Saw Stack | VA | Sawtooth with octave, fifth and double-octave layers fading in | LD Festival Saws |
| 07 | Square Stack | VA | Square with octave layers, hollow to organ-like | BA Garage Organ |
| 08 | Reso Saw | VA | Sawtooth with a strong resonant peak sweeping upward | BA Acid Reso |
| 09 | Triangle Fold | VA | Triangle through a wavefolder, gain 1 to 9 | PL West Coast |
| 10 | Sine Drive | VA | Sine into soft clipping, then asymmetric fuzz | BA Sub Driver |
| 11 | Vector Bell | Vector | Sparse bell and chime spectra | KY Vector Bell |
| 12 | Vector Voice | Vector | Small vocal waves: oo, oh, ah, eh, ee and back | PD Circuit Choir |
| 13 | Vector Organ | Vector | Eight drawbar-style registrations | KY Drawbar Keys |
| 14 | Vector Glass | Vector | Fundamental with moving high harmonic clusters | PD Glass Cathedral |
| 15 | Vector Reed | Vector | Nasal reed waves from clarinet-like to oboe-like | LD Reed Solo |
| 16 | Vector Wood | Vector | Hollow odd-harmonic waves with moving notches | KY Wood Mallet |
| 17 | Vector Digital | Vector | Eight angular digital spectra | PD Vector Sweep |
| 18 | FM Brass | FM | Two-operator FM 1:1, index 0 to 9 | BA FM Slap |
| 19 | FM Bell | FM | Three-operator FM, modulators at 7 and 3, index rising | KY FM Tines |
| 20 | Phase Bend | FM | Phase-distortion cosine, knee from centre to the edge | BA Rubber CZ |
| 21 | Trans Resonance | Transwave | A narrow resonance travels up through a pulse spectrum | PD Trans Sweep |
| 22 | Trans Scanner | Transwave | A window of harmonics scans from low to high | SQ Harmonic Scanner |
| 23 | Trans Vocal | Transwave | Continuous vowel glide oo - oh - ah - eh - ee on a sawtooth | LD Talking Lead |
| 24 | Trans Crystal | Transwave | Shimmering harmonics that rise and fall independently; loops seamlessly | PD Crystal Drift |
| 25 | Trans Comb | Transwave | Sawtooth through a comb filter whose teeth move apart | PD Comb Jet |
| 26 | Odd Even | Transwave | Odd harmonics only, then all, then even-heavy | BA Hollow Reese |
| 27 | Pluck Decay | Transwave | A bright struck spectrum dying away to a soft tone | PL Pluck Fields |
| 28 | Trans Phaser | Transwave | Sawtooth with three notches sweeping upward | PD Phase Strings |
| 29 | Growl | Modern | Formant bass that opens its mouth and distorts | BA Growl Machine |
| 30 | Stairs | Modern | Sawtooth reduced to ever fewer steps | LD Chip Hero |
| 31 | Chord Stack | Modern | Harmonics 4, 6, 5 and 7 as sawtooth layers: root, fifth, third, seventh | KY House Chords |
| 32 | Static | Modern | A tone dissolving into bright pitched noise | FX Static Riser |

## Sounds (`Content/Banks/Vitaleon Factory.vitalbank`)

Prefixes: BA bass, LD lead, PD pad, KY keys, PL pluck, SQ sequence, FX effect. Every sound uses only tables from the list above. Oscillators that are switched off hold a six-wave short form of table 01.

| Sound | Table(s) | Macro 1 | Macro 2 | Macro 3 | Macro 4 | Notes |
|---|---|---|---|---|---|---|
| BA Beef Bass | 02, 10 | CUTOFF | ENV DECAY | DRIVE | WIDTH | Detuned saws with a sine sub through the ladder filter. Mono, legato glide. |
| BA Acid Reso | 08 | RESO | DECAY | DRIVE | DELAY | The resonance lives in the wavetable: envelope and accent sweep it. Mono, legato glide. |
| BA Sub Driver | 10 | DRIVE | PUNCH | DECAY | GLIDE | Sine sub with a pitch punch; the table adds drive and fuzz. Mono, legato glide. |
| BA Growl Machine | 10, 29 | RATE | MOUTH | DIRT | SQUASH | Tempo-synced LFO opens the growl table; clean sine sub underneath. Mono, legato glide. |
| BA Rubber CZ | 20 | BEND | DECAY | CUTOFF | CHORUS | Phase-distortion bass with a bouncy envelope. |
| BA FM Slap | 18 | FM | DECAY | TONE | WIDTH | FM index snaps down on every note. |
| BA Hollow Reese | 26 | CUTOFF | DETUNE | HOLLOW | DIRT | Detuned reese; the table turns it from full to hollow. Mono, legato glide. |
| BA Garage Organ | 07 | LAYERS | CUTOFF | CLICK | CHORUS | Square stack with a short filter click, for house and garage lines. |
| LD Classic Mono | 01 | SHAPE | CUTOFF | GLIDE | ECHO | Ladder-filtered mono lead; the macro walks saw, square and pulse. Mono, legato glide. |
| LD Sync Screamer | 04 | SYNC | SWEEP | DRIVE | ECHO | Hard sync sweep on every note, mod wheel pushes it further. Mono, legato glide. |
| LD Festival Saws | 06 | FILTER | DETUNE | OCTAVES | SPACE | Seven-voice saw stack with the octave layers of the table. |
| LD Talking Lead | 23 | VOWEL | TALK RATE | LESS TALK | ECHO | Vowel glide driven by a synced LFO and the mod wheel. Mono, legato glide. |
| LD Chip Hero | 30 | BITS | VIBRATO | OCTAVE ARP | ECHO | Stepped waveform lead with delayed vibrato and an octave-jump option. |
| LD Reed Solo | 15 | NASAL | BRIGHT | VIBRATO | ROOM | Velocity-sensitive reed lead with delayed vibrato. Mono, legato glide. |
| PD PWM Strings | 03 | CUTOFF | WIDTH | SLOW ATTACK | CHORUS | Pulse-width string ensemble. |
| PD Glass Cathedral | 14 | SCAN | SPEED | DARK | REVERB | Glass clusters drifting through a long reverb. |
| PD Vector Sweep | 17 | VECTOR X | VECTOR Y | CUTOFF | SPACE | Unison voices sit on different digital waves and travel together. |
| PD Circuit Choir | 12 | VOWEL | MOTION | CUTOFF | SPACE | Small vocal waves, slowly changing vowel. |
| PD Crystal Drift | 24 | SPEED | CUTOFF | SPREAD | SPACE | The table loops seamlessly, so the motion never ends. |
| PD Phase Strings | 28 | PHASE | RATE | DARK | ENSEMBLE | String machine with the phaser built into the table. |
| PD Trans Sweep | 21 | SWEEP | FASTER | CHORUS | SPACE | A slow envelope sends the resonance up through the harmonics. |
| PL Pluck Fields | 27 | BRIGHT | DECAY | DELAY | REVERB | The envelope runs through the decay table: bright strike, soft tail. |
| KY House Chords | 31 | SEVENTH | CUTOFF | LENGTH | SPACE | One key plays a chord: root, fifth and third come from the table. |
| KY Vector Bell | 11 | TIMBRE | DECAY | CHORUS | REVERB | Digital bell whose spectrum settles after the strike. |
| KY FM Tines | 19 | BELL | TREMOLO | TONE | CHORUS | FM electric piano; velocity adds the bell. |
| KY Drawbar Keys | 13 | DRAWBARS | ROTARY | DRIVE | ROOM | Eight registrations on the macro, chorus as rotary. |
| KY Wood Mallet | 16 | HARDNESS | DECAY | TONE | ROOM | Hollow mallet with a short bright strike. |
| PL West Coast | 09 | FOLD | DECAY | CUTOFF | ECHO | Wavefolder pluck through a low-pass gate. |
| SQ Harmonic Scanner | 22 | RATE | RANGE | DETUNE | ECHO | Hold a note: an eight-step LFO plays the overtones as a pattern. |
| PL Laser Stab | 05 | FORMANT | ZAP TIME | DRIVE | ECHO | The formant drops through the table on every note. |
| PD Comb Jet | 25 | JET | RATE | CUTOFF | SPACE | Comb-filter sweep baked into the table; pad and keys in one. |
| FX Static Riser | 32 | FASTER | MORE PITCH | NOISE | SPACE | Hold a note: tone turns into noise while pitch and filter rise. |
