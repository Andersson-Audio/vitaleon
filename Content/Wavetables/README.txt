Factory holds the 32 factory wavetables (256 frames x 2048 samples, 32-bit float WAV).
Fizmatic holds the 54 wavetables of the bank Fizmatic (256 frames x 2048 samples, 16-bit WAV).
Your own tables: load a .vitaltable or wavetable .wav in HOME > LIBRARY from a USB stick, an SD card or an SSD (WAV AS: OSC 1, 2 or 3, then LOAD).
The file is copied to the folder Vitaleon/Wavetables beside the folder VST2 Plugins and stays there when the plugin is updated; files copied there by hand are found as well.
OSC > OVERVIEW: the arrows step through every table; a tap on a wave picture opens the pages of tables, the factory tables first, then FIZMATIC 1 OF 2 and 2 OF 2, then the user's; on those pages the plugin draws the pictures.
A WAV is read in cycles of 2048 samples unless the file states another length; a file shorter than 4096 samples is taken as a single cycle.
