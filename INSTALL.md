# Installing Vitaleon

[Deutsch weiter unten](#installation-auf-deutsch)

## What you need

- An Akai MPC running in standalone mode with MPC OS 3.10.1 or later. Check **Preferences > Info** and update if it is older.
- The release ZIP from [dreyandersson.com/vitaleon](https://dreyandersson.com/vitaleon/), about 290 MB. Most of it is the wavetables and sounds.

Tested on an MPC Live II. Other models are untested.

## Steps

1. Unzip the download. It contains the folder `Andersson Audio - VST - Vitaleon`. Keep the folder name exactly as it is.
2. Put that folder inside a folder named exactly `VST2 Plugins` at the top level of a drive. Create `VST2 Plugins` if it does not exist. Any of these drives works:
   - **Internal storage (recommended).** Connect the MPC to your computer in computer mode and copy the folder over, or copy it from a stick with the MPC's own file browser.
   - A USB stick, SD card or the built-in SSD.
3. On the MPC open **Preferences > Enhancements (Beta)** and switch on **Enable Enhancements** and **External Plugin Support**.
4. Plug in the drive, or restart the MPC if the plugin does not appear.
5. Create a plugin track and choose **Vitaleon** under External Plugins.

The result looks like this:

```
<drive>/VST2 Plugins/Andersson Audio - VST - Vitaleon/plugin.json
<drive>/VST2 Plugins/Andersson Audio - VST - Vitaleon/VST/...
<drive>/VST2 Plugins/Andersson Audio - VST - Vitaleon/Plugin Skins/...
<drive>/VST2 Plugins/Andersson Audio - VST - Vitaleon/Content/...
```

## Your own sounds

- Load them from any drive with **HOME > LIBRARY** in the plugin: sounds (`.vital`), banks (`.vitalbank`, ZIP), wavetables (`.vitaltable`, WAV), samples (WAV) and LFO shapes (`.vitallfo`). For a WAV, choose first under **WAV AS** whether it becomes the wavetable of oscillator 1, 2 or 3 or the sample. What you load is saved with the MPC project; the drive is not needed afterwards.
- Or copy them into the plugin's `Content` folder: `Presets` for `.vital`, `Banks` for `.vitalbank` and ZIP, `Wavetables` for `.vitaltable` and WAV, `Samples` for WAV, `LFOs` for `.vitallfo`. They are read when the plugin is loaded and appear in the PRESET list, under TABLE LIST on the oscillator pages and in the file selectors of the sample and LFO pages.
- A wavetable WAV is read in cycles of 2048 samples unless the file states another length. A file shorter than 4096 samples is taken as a single cycle. A sample can be up to 95 seconds long at 44.1 kHz.

## Factory sounds and wavetables

The package includes 32 factory wavetables and 32 factory sounds. They are free to use with Vitaleon and in your own music, but they may not be redistributed. See `FACTORY-CONTENT-LICENSE.txt`.

It also includes the bank Fizmatic (54 sounds, each built on one of its 54 wavetables) and those wavetables as files, on two pages of their own behind the factory page of the wavetable pages (same licence as the factory content), and two banks of sounds by qub1t: QUB1T - Volume 1 (95 sounds) and QUB1T - Volume 2 (100 sounds). The plugin starts on the factory bank; the two arrows beside the bank name at the top of HOME step to the other banks and load the first sound of each.

## Updating

Replace the folder with the new one. If you put your own files into its `Content` folder, copy them out first and back in afterwards. Sounds stored with SAVE SOUND and wavetables loaded in LIBRARY are in the folder `Vitaleon` beside `VST2 Plugins` and stay where they are.

## Removing

Delete the folder `Andersson Audio - VST - Vitaleon`.

## If it does not appear

- Is the folder directly inside `VST2 Plugins`, and is `VST2 Plugins` at the top level of the drive?
- Are both switches under Enhancements (Beta) on?
- Try the internal storage instead of a USB stick.
- Restart the MPC.

---

# Installation auf Deutsch

## Voraussetzungen

- Eine Akai MPC im Standalone-Betrieb mit MPC OS 3.10.1 oder neuer. Unter **Preferences > Info** nachsehen und bei Bedarf aktualisieren.
- Das Release-ZIP von [dreyandersson.com/vitaleon](https://dreyandersson.com/vitaleon/), rund 290 MB. Den größten Teil machen die Wavetables und Sounds aus.

Getestet auf einer MPC Live II. Andere Modelle sind ungetestet.

## Schritte

1. Das ZIP entpacken. Es enthält den Ordner `Andersson Audio - VST - Vitaleon`. Den Ordnernamen nicht ändern.
2. Diesen Ordner in einen Ordner mit dem exakten Namen `VST2 Plugins` legen, der auf der obersten Ebene eines Laufwerks liegt. `VST2 Plugins` anlegen, falls er fehlt. Möglich sind:
   - **Interner Speicher (empfohlen).** Die MPC im Computer-Modus an den Rechner anschließen und den Ordner hinüberkopieren, oder ihn mit dem Dateibrowser der MPC von einem Stick kopieren.
   - USB-Stick, SD-Karte oder die eingebaute SSD.
3. Auf der MPC **Preferences > Enhancements (Beta)** öffnen und **Enable Enhancements** sowie **External Plugin Support** einschalten.
4. Das Laufwerk anstecken oder die MPC neu starten, falls das Plugin nicht erscheint.
5. Eine Plugin-Spur anlegen und unter External Plugins **Vitaleon** wählen.

## Eigene Sounds

- Von jedem Laufwerk im Plugin über **HOME > LIBRARY** laden: Sounds (`.vital`), Bänke (`.vitalbank`, ZIP), Wavetables (`.vitaltable`, WAV), Samples (WAV) und LFO-Formen (`.vitallfo`). Bei einer WAV-Datei vorher unter **WAV AS** wählen, ob sie die Wavetable von Oszillator 1, 2 oder 3 wird oder das Sample. Was geladen wurde, wird mit dem MPC-Projekt gespeichert; das Laufwerk wird danach nicht mehr gebraucht.
- Oder in den Ordner `Content` des Plugins kopieren: `Presets` für `.vital`, `Banks` für `.vitalbank` und ZIP, `Wavetables` für `.vitaltable` und WAV, `Samples` für WAV, `LFOs` für `.vitallfo`. Sie werden beim Laden des Plugins eingelesen und erscheinen in der PRESET-Liste, unter TABLE LIST auf den Oszillator-Seiten und in den Dateiwählern der Sample- und LFO-Seiten.
- Eine Wavetable-WAV wird in Zyklen von 2048 Samples gelesen, sofern die Datei keine andere Länge angibt. Eine Datei mit weniger als 4096 Samples gilt als einzelner Zyklus. Ein Sample darf bei 44,1 kHz bis zu 95 Sekunden lang sein.

## Werkssounds und Werks-Wavetables

Das Paket enthält 32 Werks-Wavetables und 32 Werkssounds. Du darfst sie frei mit Vitaleon und in eigener Musik benutzen, aber nicht weitergeben. Siehe `FACTORY-CONTENT-LICENSE.txt`.

Dazu kommen die Bank Fizmatic (54 Sounds, jeder auf einer ihrer 54 Wavetables aufgebaut) und diese Wavetables als Dateien, auf zwei eigenen Seiten hinter der Werksseite der Wavetable-Seiten (gleiche Lizenz wie die Werksinhalte), sowie zwei Bänke mit Sounds von qub1t: QUB1T - Volume 1 (95 Sounds) und QUB1T - Volume 2 (100 Sounds). Das Plugin startet auf der Werksbank; die zwei Pfeile neben dem Banknamen oben auf HOME wechseln zu den anderen Bänken und laden jeweils deren ersten Sound.

## Aktualisieren

Den Ordner durch den neuen ersetzen. Eigene Dateien aus dem Ordner `Content` vorher herauskopieren und danach wieder hineinlegen. Mit SAVE SOUND gespeicherte Sounds und in LIBRARY geladene Wavetables liegen im Ordner `Vitaleon` neben `VST2 Plugins` und bleiben, wo sie sind.

## Entfernen

Den Ordner `Andersson Audio - VST - Vitaleon` löschen.

## Wenn das Plugin nicht erscheint

- Liegt der Ordner direkt in `VST2 Plugins`, und liegt `VST2 Plugins` auf der obersten Ebene des Laufwerks?
- Sind beide Schalter unter Enhancements (Beta) eingeschaltet?
- Den internen Speicher statt eines USB-Sticks versuchen.
- Die MPC neu starten.
