// 0.25: the DAW's song position. The host starts in the middle of a bar and loops; with SONG POS (default) the
// dream's notes fall on the song's sixteenths, also after the loop jumps back; FREE (MIDI F2) keeps its own grid.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const BUTTON = 3, ENCODER = 4, TRANSPORT = 8, MENU = 19, ENTER = 9, F1 = 0;
const GROOVE = 28, MIDI = 46, rate = 48000, block = 64, bpm = 120;

function run({start, free = false, loop = null, seconds = 14}) {
  const e = instantiate(module).exports;
  e.vibe_wasm_init(rate);
  const press = id => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); };
  const open = v => { press(MENU); e.vibe_wasm_event(ENCODER, 0, 0, -60, 0); e.vibe_wasm_event(ENCODER, 0, 0, v, 0); press(ENTER); };
  open(GROOVE); e.vibe_wasm_event(ENCODER, 0, 0, -100, 0);                     // no swing: a plain sixteenth grid
  open(MIDI); press(F1);                                                         // notes out
  if (free) press(F1 + 1);                                                       // SONG POS off: FREE
  const tp = () => new DataView(e.memory.buffer, e.vibe_wasm_transport());
  let beat = start;
  const fill = (playing) => { const d = tp(); d.setInt32(0, 1, true); d.setInt32(4, playing, true); d.setFloat32(8, bpm, true);
    d.setInt32(12, 4, true); d.setInt32(16, 4, true); d.setFloat64(24, beat, true); d.setFloat64(32, Math.floor(beat / 4) * 4, true); };
  e.vibe_wasm_event(TRANSPORT, 1, 1, bpm, 0);                                    // tempo
  fill(1);
  e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0);                                      // PLAY
  const notes = [];                                                              // host sixteenth position of each note-on
  let jumps = 0;
  for (let k = 0; k < rate * seconds / block; k++) {
    fill(1);
    e.vibe_wasm_audio(block, 0);
    const m = new DataView(e.memory.buffer, e.vibe_wasm_midi_out());
    const w = m.getUint32(0, true), r = m.getUint32(4, true);
    for (let i = r; i < w; i++) { const o = 16 + (i % 256) * 6; if ((m.getUint8(o + 2) & 0xf0) === 0x90 && m.getUint8(o + 4) > 0) notes.push(beat * 4); }
    m.setUint32(4, w, true);
    beat += block / rate * bpm / 60;
    if (loop && beat >= loop[1]) { beat = loop[0] + (beat - loop[1]); jumps++; }
  }
  // how far each note lies from the song's sixteenth grid (the note is seen at the end of its block: allow one block)
  const tol = block / rate * bpm / 60 * 4 + .02;
  const off = notes.map(p => { const f = p - Math.floor(p); return f; });
  const onGrid = off.filter(f => f <= tol || f >= 1 - .02).length;
  return {notes: notes.length, onGrid, share: +(onGrid / Math.max(1, notes.length)).toFixed(3), jumps};
}

const R = {};
R.follow = run({start: 37.3});                                                  // starts 0.2 of a sixteenth after the grid
R.free = run({start: 37.3, free: true});
R.loop = run({start: 1.7, loop: [0.6, 12.6]});                                  // a loop that does not start on the grid
console.log(JSON.stringify(R, null, 1));
assert(R.follow.notes >= 8, "the dream plays");
assert(R.follow.share > .95, `with SONG POS the notes fall on the song's sixteenths (${R.follow.share})`);
assert(R.free.share < .5, `FREE keeps its own grid, off the song's (${R.free.share})`);
assert(R.loop.jumps >= 1 && R.loop.share > .95, `after the loop jumps back the notes stay on the grid (${R.loop.share})`);
console.log("PASSED: SONG POS – the sequencer starts where the DAW's song stands and follows loops; FREE keeps its own grid.");
