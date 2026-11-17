# VIBE 0.8.0 / anyDAW Studio 0.3.61 compatibility

The patch is for the **anyDAW host**, not the firmware or VIBE plug-in binary. It is deliberately limited to descriptors named VIBE FM VST, manufacturer VIBE FM, version 0.8.0, format VST3 or AudioUnit. It was tested in a separate, privately signed copy of anyDAW Studio. It is not an official anyDAW update.

- VST3 exposes 2352 parameters: 0–255 firmware slots, then transient MIDI/controller proxies. Restoring all of them replays Program 0, overwriting restored Biome values on the first audio block. Restore only finite values for named active firmware controls; leave explicit MIDI and automation untouched.
- The AU loses a firmware selection applied before prepareToPlay. Apply its chunk after preparation, then restore the active firmware controls.
- Both creation and existing-instance setState paths are covered. With the same tests the original adapter passed 20/26 and the patched adapter 26/26, including five fresh-process save/reload cycles per format, automation, save during processing, block sizes and independent instances.
- The complete app additionally passed real file save/load, saving with the AudioContext suspended, three arrangement automation lanes, and a cold app restart. A 120.454-second focused real-time check with animation reported zero new late blocks. Earlier concurrent startup/signing stress produced late blocks; these measurements are not a blanket performance guarantee.

The raw VIBE chunk alone may still be stale immediately after a parameter edit without an audio process block. The tested anyDAW project saves separate parameter values and the compatibility layer restores them. This is not a universal fix for every DAW or raw preset file.

The tested anyDAW route is stereo at 44.1 kHz. Multichannel stems, plug-in MIDI output and physical VIBE jacks were not covered by that host test. They must not be advertised as hardware-tested here.
