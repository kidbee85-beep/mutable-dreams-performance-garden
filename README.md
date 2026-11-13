# Mutable Dreams · Performance Garden

A generative performance instrument for VIBE FM, built on Mutable Dreams 2.1 by **Kid Bunka**. Version **0.18.0-preview**.

The 16 pressure-responsive pads shape tone, texture, space and form. Pad 16 captures and loops gestures. The 1280 × 720 Stage and the CUSTOM Dream Console give access to 271 Biomes in 19 families, 47 pages, six favourites, and variations. Itinera plays long-form journeys. The firmware exposes 30 parameters and six dry stems on outputs 3–8 in compatible hosts.

## Play

Download the firmware ZIP and the illustrated guide from [Releases](https://github.com/kidbee85-beep/mutable-dreams-performance-garden/releases). In the [VIBE FM Web Player](https://play.thefirmwarehub.com/) load the WASM and press **Power on**. In the VIBE FM plug-in, add the firmware ZIP and select **Mutable Dreams · Performance Garden**.

The preview has automated coverage for audio, state recall, MIDI, automation, pads, and stems. Physical VIBE FM Mk1 verification is pending. This package targets VIBE FM.

## Build the preview

The release’s corresponding-source ZIP includes the files below, the exact engine and the required SDK sources with their notices. Extract it and point `WASI_SDK_BIN` to the `bin` folder of [WASI SDK 34.0](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-34). Run `python3 build.py`. The resulting WASM and firmware ZIP are in `build/`.

To build from this Git checkout, use the following exact dependencies. Keep their internal directory structures.

- Extract [Mutable Dreams 2.1, commit faf56b9](https://github.com/kidbee85-beep/mutable-dreams/tree/faf56b957b42998dbf12b4b20c58f5ba93f62d58) into `engine-2.1/`.
- Extract the `sdk/` directory and `emulator/js/vibe-editor.js` from the [VIBE FM SDK, commit 86d02ca](https://github.com/Vibe-FM-SDK/vibe-fm-sdk/tree/86d02ca31cdf0459891bba425c864dacfec6351d) into `official/`.

The expected SHA-256 of `build/mutable-dreams-garden.wasm` is `6b6f9f92a47b118c0dfc87ecd3bbca723bbda1c2413467c32f2d26d36fedd5cd`.

## Licence

The combined firmware and Performance Garden source are distributed under GPL-3.0-only. The shared engine includes separately licensed components and notices. The VIBE FM SDK and editor helper are Apache-2.0 with their upstream notices. The VIBE FM plug-in installer is available from The Firmware Hub.

## Watch, follow and support

[Project and illustrated guide](https://kidbee85-beep.github.io/mutable-dreams/#performance-garden) · [Full walkthrough](https://www.youtube.com/watch?v=BT8Njfo1C6s) · [Short demonstration](https://www.youtube.com/shorts/DOzcqtq9R-o) · [Instagram @mutabledreams](https://www.instagram.com/mutabledreams/) · [Mutable Dreams FM](https://kidbee85-beep.github.io/mutable-dreams/radio.html)

[Support development via PayPal](https://paypal.com/donate/?business=kidbee85@gmail.com). Voluntary support goes entirely into firmware updates, new firmware and synthesizer software.
