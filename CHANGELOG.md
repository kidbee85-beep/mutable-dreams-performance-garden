# Changelog · Mutable Dreams · Performance Garden for VIBE FM

By Kid Bunka. Newest first. Parameter ids and saved gardens stay compatible across all versions listed here.

## 0.32 - the living garden (private preview)
- Seven page-group backgrounds: fireflies, breathing light, wind, constellations, water and mist, sequence lights, aurora.
- A 0.45-second petal transition behind page content; normal navigation remains immediate.
- Garden-style pad and navigation-button lights; active-state indication remains separate from decorative breathing.
- Four Stage macros grow as plants, with leaves and flowers; glass controls, sprouts and grass-like value meters.
- Sound engine, parameter IDs and save format unchanged from the reviewed 0.31.
- Reviewed on this Mac: 36 function tests, SDK validation, controls and four-rate bit-identical audio comparison.
- VIBE FM hardware acceptance and a new real-DAW pass for this visual preview remain pending.

## 0.31 – towards 1.0
- The source tree is complete: the engine's generators (Biome presets, Satin wavetables, lo-fi kit) and the worlds
  pipeline are included and rebuild their tables byte for byte; the drop-in zip is reproducible.
- The zip carries every licence text and notice (`mutable-dreams-garden.editor/licenses/`); the icon has its source.
- A fuzzer (`port/test_fuzz.mjs`) and a parameter-id guard (`port/test_ids.mjs`) join the tests.
- Display: active keys and tabs keep long names readable; tidier MIDI page and value cells.

## 0.30
- REC records the dry input too; MOVES and the meters hear both inputs; the ear resets when the input is switched;
  KEY takes only a newly sung note; the reversed SING answer keeps its rhythm (second Codex review).
- SING ANSWER shows the call and response: your phrase as a contour, and whose turn it is.
- CV GATE shows where CV IN goes; GATE OUT lights while open. FIRST STEPS speaks inside the menu.
- Demo videos straight from the firmware (60 s and 84 s) with captions in their own band.

## 0.29
- Demo video rendered by the firmware itself; sharper PAGES view (Scale2x).

## 0.28
- Reviewed with Codex: no hanging MIDI notes after mute, blank or a full queue; held F keys do nothing after a page
  change; a more precise input filter. Large numbers in the value cells.

## 0.27
- A new, calmer look: quiet keys, accent-filled active states, value cells, page counter, MENU in two banks,
  two-page HOME help, a cloud of its own on CLOUD, clearer pads on the stage, glass keys in the Dream Console.

## 0.26
- FIRST STEPS, an interactive tour for new players (device, stage and console). MOVES: your loudness lifts a macro.
  KEY: a sung note becomes the key. F keys act on release; held, they explain themselves.

## 0.25
- The sequencer follows the DAW's song position, loops included. A band-limited input resampler, a soft ceiling,
  a gliding THROUGH; strict WebAssembly validation.

## 0.24 – the garden listens
- IN 1 / IN 2 into the garden's cloud, echo and room (LISTEN); SING with a pitch follower (FOLLOW, HARMONY, ANSWER);
  CV / gate out and CV in; PLACES with six places; 50 pages.

## 0.19 – 0.23
- The night garden of the Brick and MPC ports behind the stage and pages; GARDEN / PLACES / PAGES on the stage;
  Kid Bunka's 48 worlds, which can wander.

## 0.11 – 0.18
- REC takes as WAV, draggable macros on the stage, Dream as MIDI, intro animation, 30 DAW parameters; the Mutable
  Dreams FM radio; 8 × 8 grids; stems on OUT 3–8; Program Change picks a Biome; VIBE FM Link; the Dream Console
  editor page with all 50 pages and the Biome library; the stage at 1280 × 720.

## 0.9 – 0.10
- The complete Mutable Dreams 2.1 (engine 2467CC38): 50 pages, the MENU, a browser for 271 Biomes, Itinera, Journey,
  banks, MIDI learn; SEQ GRID, SEQ TOOLS and SEQ RULES as on the FM-1.

## 0.2 – 0.8
- Six pages and 16 effect pads, then the pad rows TONE / TEXTURE / SPACE / FORM, the gesture recorder, the stage,
  saving and DAW parameters, latch, overdub, moments with wander, tone modes and gesture speed.
