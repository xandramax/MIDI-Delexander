# MIDI Delexander
A plugin for VCV Rack containing modules with a focus on MIDI

![MIDI-Delexander](https://github.com/anlexmatos/MIDI-Delexander/blob/master/MIDI-Delexander.PNG)

# Duo MIDI-CV
Based on MIDI-CV from VCV Rack [Core modules](https://github.com/VCVRack/Rack/tree/v1/src/core)
* MIDI pitch bend range is configurable via context menu
* Bend range up and down can be set separately
* Adds a Bent Pitch output which carries a sum of pitch bend and note data
* Allows for Rotate, Reuse, and Reset modes to be used with MPE
* Adds a secondary output for all note-related MIDI data, allowing for polyphony up to 32
* Number of voices is individually configurable for each output
* Adds a polyphonic "Note Stop" trigger input, which resets the note on the corresponding VCV channel

# Super MIDI 64
Based on MIDIpolyMPE by Pablo Delaloza ([moDllz](https://github.com/dllmusic/moDllz))
* Quadruples all polyphonic outputs, allowing for polyphony up to 64
* Allows for setting number of active outputs and number of voices per output
* Adds an Output Rotation mode, wherein incoming notes are rotated first across the module's outputs and then across each output's channels (A1 -> B1 -> C1 -> A2...)
* Expanded from 8 assignable monophonic CC outputs up to 20
* Allows for note and velocity range exclusion by setting minimum note/velocity higher than maximum note/velocity. The range in between is excluded, and all other values are allowed.
